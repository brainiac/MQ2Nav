#include "pfs.h"
#include "pfs_crc.h"
#include "compression.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <tuple>

#include "read_buffer.h"

#define ReadFromBuffer(type, var, buffer, idx)                            \
	if (idx + sizeof(type) > buffer.size()) {                             \
		return false;                                                     \
	}                                                                     \
	type var = *(type*)&buffer[idx];

#define ReadFromBufferLength(var, len, buffer, idx)                       \
	if (idx + len > buffer.size()) {                                      \
		return false;                                                     \
	}                                                                     \
	memcpy(var, &buffer[idx], len);

#define WriteToBuffer(type, val, buffer, idx)                             \
	if (idx + sizeof(type) > buffer.size()) {                             \
		buffer.resize(idx + sizeof(type));                                \
	}                                                                     \
	*(type*)&buffer[idx] = val;  

#define WriteToBufferLength(var, len, buffer, idx)                        \
	if (idx + len > buffer.size()) {                                      \
		buffer.resize(idx + len);                                         \
	}                                                                     \
	memcpy(&buffer[idx], var, len);


namespace EQEmu::PFS {

constexpr int32_t MAX_BLOCK_SIZE = 8192;          // the client will crash if you make this bigger, so don't.
constexpr int32_t PFS_MAGIC = 'PFS ';

Archive::Archive()
{
}

Archive::~Archive()
{
}

bool Archive::Open()
{
	Close();
	return true;
}

bool Archive::Open(uint32_t date)
{
	Close();
	m_footer = true;
	m_footer_date = date;
	return true;
}

bool Archive::Open(const std::string& filename)
{
	Close();

	std::vector<char> buffer;
	FILE* f = _fsopen(filename.c_str(), "rb", _SH_DENYNO);
	if (f)
	{
		fseek(f, 0, SEEK_END);
		size_t sz = ftell(f);
		rewind(f);

		buffer.resize(sz);
		size_t res = fread(&buffer[0], 1, sz, f);
		if (res != sz)
		{
			return false;
		}

		fclose(f);
	}
	else
	{
		return false;
	}

	char magic[4];
	ReadFromBuffer(uint32_t, dir_offset, buffer, 0);
	ReadFromBufferLength(magic, 4, buffer, 4);

	if (magic[0] != 'P' || magic[1] != 'F' || magic[2] != 'S' || magic[3] != ' ')
	{
		return false;
	}

	ReadFromBuffer(uint32_t, dir_count, buffer, dir_offset);
	std::vector<std::tuple<int32_t, uint32_t, uint32_t>> directory_entries;
	std::vector<std::tuple<int32_t, std::string>> filename_entries;
	for (uint32_t i = 0; i < dir_count; ++i)
	{
		ReadFromBuffer(int32_t, crc, buffer, dir_offset + 4 + (i * 12));
		ReadFromBuffer(uint32_t, offset, buffer, dir_offset + 8 + (i * 12));
		ReadFromBuffer(uint32_t, size, buffer, dir_offset + 12 + (i * 12));

		if (crc == 0x61580ac9) // "__PFSFileNames__"
		{
			auto filename_buffer = InflateByFileOffset(offset, size, buffer);
			if (!filename_buffer)
			{
				return false;
			}

			ReadBuffer reader(filename_buffer, size);

			uint32_t filename_count;
			if (!reader.read(filename_count))
				return false;

			for (uint32_t j = 0; j < filename_count; ++j)
			{
				uint32_t filename_length;
				if (!reader.read(filename_length))
					return false;

				std::string filename;
				filename.resize(filename_length - 1);
				if (!reader.read(&filename[0], filename_length))
					return false;

				std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
				int32_t crc = CRC().Get(filename);

				filename_entries.push_back(std::make_tuple(crc, filename));
			}
		}
		else
		{
			directory_entries.push_back(std::make_tuple(crc, offset, size));
		}
	}

	auto iter = directory_entries.begin();
	while (iter != directory_entries.end())
	{
		int32_t crc = std::get<0>((*iter));

		auto f_iter = filename_entries.begin();
		while (f_iter != filename_entries.end())
		{
			int32_t f_crc = std::get<0>((*f_iter));

			if (crc == f_crc)
			{
				uint32_t offset = std::get<1>((*iter));
				uint32_t size = std::get<2>((*iter));
				std::string filename = std::get<1>((*f_iter));
				if (!StoreBlocksByFileOffset(offset, size, buffer, filename))
				{
					return false;
				}

				break;
			}

			++f_iter;
		}
		++iter;
	}

	uint32_t footer_offset = dir_offset + 4 + (12 * dir_count);
	if (footer_offset == buffer.size())
	{
		m_footer = false;
	}
	else
	{
		char magic[5];
		ReadFromBufferLength(magic, 5, buffer, footer_offset);
		ReadFromBuffer(uint32_t, date, buffer, footer_offset + 5);
		m_footer = true;
		m_footer_date = date;
	}

	m_filename = filename;

	return true;
}

bool Archive::Save(const std::string& filename)
{
	std::vector<char> buffer;

	//Write Header
	WriteToBuffer(uint32_t, 0, buffer, 0);
	WriteToBuffer(uint8_t, 'P', buffer, 4);
	WriteToBuffer(uint8_t, 'F', buffer, 5);
	WriteToBuffer(uint8_t, 'S', buffer, 6);
	WriteToBuffer(uint8_t, ' ', buffer, 7);
	WriteToBuffer(uint32_t, 131072, buffer, 8);

	std::vector<std::tuple<int32_t, uint32_t, uint32_t>> dir_entries;
	std::vector<char> files_list;
	uint32_t file_offset = 0;
	uint32_t file_size = 0;
	uint32_t dir_offset = 0;
	uint32_t file_count = (uint32_t)m_files.size();
	uint32_t file_pos = 0;

	WriteToBuffer(uint32_t, file_count, files_list, file_pos);
	file_pos += 4;

	auto iter = m_files.begin();
	while (iter != m_files.end())
	{
		int32_t crc = CRC().Get(iter->first);
		uint32_t offset = (uint32_t)buffer.size();
		uint32_t sz = m_files_uncompressed_size[iter->first];

		buffer.insert(buffer.end(), iter->second.begin(), iter->second.end());

		dir_entries.push_back(std::make_tuple(crc, offset, sz));

		uint32_t filename_len = (uint32_t)iter->first.length() + 1;
		WriteToBuffer(uint32_t, filename_len, files_list, file_pos);
		file_pos += 4;

		WriteToBufferLength(&(iter->first[0]), filename_len - 1, files_list, file_pos);
		file_pos += filename_len;

		WriteToBuffer(uint8_t, 0, files_list, file_pos - 1);
		++iter;
	}

	file_offset = (uint32_t)buffer.size();
	if (!WriteDeflatedFileBlock(files_list, buffer))
	{
		return false;
	}

	file_size = (uint32_t)files_list.size();

	dir_offset = (uint32_t)buffer.size();
	WriteToBuffer(uint32_t, dir_offset, buffer, 0);

	uint32_t cur_dir_entry_offset = dir_offset;
	uint32_t dir_count = (uint32_t)dir_entries.size() + 1;
	WriteToBuffer(uint32_t, dir_count, buffer, cur_dir_entry_offset);

	cur_dir_entry_offset += 4;
	auto dir_iter = dir_entries.begin();
	while (dir_iter != dir_entries.end())
	{
		int32_t crc = std::get<0>(*dir_iter);
		uint32_t offset = std::get<1>(*dir_iter);
		uint32_t size = std::get<2>(*dir_iter);

		WriteToBuffer(int32_t, crc, buffer, cur_dir_entry_offset);
		WriteToBuffer(uint32_t, offset, buffer, cur_dir_entry_offset + 4);
		WriteToBuffer(uint32_t, size, buffer, cur_dir_entry_offset + 8);

		cur_dir_entry_offset += 12;
		++dir_iter;
	}

	WriteToBuffer(int32_t, 0x61580AC9, buffer, cur_dir_entry_offset);
	WriteToBuffer(uint32_t, file_offset, buffer, cur_dir_entry_offset + 4);
	WriteToBuffer(uint32_t, file_size, buffer, cur_dir_entry_offset + 8);
	cur_dir_entry_offset += 12;

	if (m_footer)
	{
		WriteToBuffer(int8_t, 'S', buffer, cur_dir_entry_offset);
		WriteToBuffer(int8_t, 'T', buffer, cur_dir_entry_offset + 1);
		WriteToBuffer(int8_t, 'E', buffer, cur_dir_entry_offset + 2);
		WriteToBuffer(int8_t, 'V', buffer, cur_dir_entry_offset + 3);
		WriteToBuffer(int8_t, 'E', buffer, cur_dir_entry_offset + 4);
		WriteToBuffer(uint32_t, m_footer_date, buffer, cur_dir_entry_offset + 5);
	}

	FILE* f = _fsopen(filename.c_str(), "wb", _SH_DENYWR);
	if (f)
	{
		size_t sz = fwrite(&buffer[0], buffer.size(), 1, f);
		if (sz != 1)
		{
			fclose(f);
			return false;
		}
		fclose(f);
	}
	else
	{
		return false;
	}

	return true;
}

void Archive::Close()
{
	m_footer = false;
	m_footer_date = 0;
	m_files.clear();
	m_files_uncompressed_size.clear();
}

bool Archive::Get(std::string filename, std::vector<char>& buf)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	auto iter = m_files.find(filename);
	if (iter != m_files.end())
	{
		buf.clear();

		uint32_t uc_size = m_files_uncompressed_size[filename];
		auto data = InflateByFileOffset(0, uc_size, iter->second);
		if (!data)
		{
			return false;
		}

		buf.resize(uc_size);
		memcpy(&buf[0], data.get(), uc_size);

		return true;
	}

	return false;
}

std::unique_ptr<uint8_t[]> Archive::Get(std::string filename, uint32_t& size)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	auto iter = m_files.find(filename);
	if (iter != m_files.end())
	{
		size = m_files_uncompressed_size[filename];
		return InflateByFileOffset(0, size, iter->second);
	}

	return nullptr;
}

bool Archive::Set(std::string filename, const std::vector<char>& buf)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	std::vector<char> vec;
	uint32_t uc_size = (uint32_t)buf.size();

	if (!WriteDeflatedFileBlock(buf, vec))
	{
		return false;
	}

	m_files[filename] = vec;
	m_files_uncompressed_size[filename] = uc_size;

	return true;
}

bool Archive::Delete(std::string filename)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	m_files.erase(filename);
	m_files_uncompressed_size.erase(filename);

	return true;
}

bool Archive::Rename(std::string filename, std::string filename_new)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
	std::transform(filename_new.begin(), filename_new.end(), filename_new.begin(), ::tolower);

	if (m_files.count(filename_new) != 0)
	{
		return false;
	}

	auto iter = m_files.find(filename);
	if (iter != m_files.end())
	{
		m_files[filename_new] = iter->second;
		m_files.erase(iter);

		auto iter_s = m_files_uncompressed_size.find(filename);
		m_files_uncompressed_size[filename_new] = iter_s->second;
		m_files_uncompressed_size.erase(iter_s);
		return true;
	}

	return false;
}

bool Archive::Exists(std::string filename)
{
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

	return m_files.count(filename) != 0;
}

bool Archive::GetFilenames(std::string ext, std::vector<std::string>& out_files)
{
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	out_files.clear();

	size_t elen = ext.length();
	bool all_files = !ext.compare("*");

	auto iter = m_files.begin();
	while (iter != m_files.end())
	{
		if (all_files)
		{
			out_files.push_back(iter->first);
			++iter;
			continue;
		}

		size_t flen = iter->first.length();
		if (flen <= elen)
		{
			++iter;
			continue;
		}

		if (!strcmp(iter->first.c_str() + (flen - elen), ext.c_str()))
		{
			out_files.push_back(iter->first);
		}
		++iter;
	}
	return out_files.size() > 0;
}

bool Archive::StoreBlocksByFileOffset(uint32_t offset, uint32_t size, const std::vector<char>& in_buffer, std::string filename)
{
	uint32_t position = offset;
	uint32_t block_size = 0;
	uint32_t inflate = 0;

	while (inflate < size)
	{
		ReadFromBuffer(uint32_t, deflate_length, in_buffer, position);
		ReadFromBuffer(uint32_t, inflate_length, in_buffer, position + 4);
		inflate += inflate_length;
		position += deflate_length + 8;
	}

	block_size = position - offset;

	std::vector<char> tbuffer;
	tbuffer.resize(block_size);

	memcpy(&tbuffer[0], &in_buffer[offset], block_size);

	m_files[filename] = tbuffer;
	m_files_uncompressed_size[filename] = size;
	return true;
}

std::unique_ptr<uint8_t[]> Archive::InflateByFileOffset(
	uint32_t offset,
	uint32_t size,
	const std::vector<char>& in_buffer)
{
	std::unique_ptr<uint8_t[]> out_buffer = std::make_unique<uint8_t[]>(size);
	memset(out_buffer.get(), 0, size);

	ReadBuffer reader(in_buffer.data() + offset, in_buffer.size());
	uint32_t inflate = 0;

	while (inflate < size)
	{
		std::vector<char> temp_buffer;

		uint32_t deflate_length, inflate_length;
		if (!reader.read(deflate_length))
			return nullptr;
		if (!reader.read(inflate_length))
			return nullptr;

		temp_buffer.resize(deflate_length + 1);

		if (!reader.read(&temp_buffer[0], deflate_length))
			return nullptr;

		uint32_t inflate_out = EQEmu::InflateData(&temp_buffer[0], deflate_length,
			reinterpret_cast<char*>(out_buffer.get()) + inflate, inflate_length);
		if (inflate_out != inflate_length)
			return nullptr;
		inflate += inflate_length;
	}

	return out_buffer;
}

bool Archive::WriteDeflatedFileBlock(const std::vector<char>& file, std::vector<char>& out_buffer)
{
	uint32_t pos = 0;
	uint32_t remain = (uint32_t)file.size();
	uint8_t block[MAX_BLOCK_SIZE + 128];

	while (remain > 0)
	{
		uint32_t sz;
		if (remain >= MAX_BLOCK_SIZE)
		{
			sz = MAX_BLOCK_SIZE;
			remain -= MAX_BLOCK_SIZE;
		}
		else
		{
			sz = remain;
			remain = 0;
		}

		uint32_t block_len = sz + 128;
		uint32_t deflate_size = (uint32_t)EQEmu::DeflateData(&file[pos], sz, (char*)&block[0], block_len);
		if (deflate_size == 0)
			return false;

		pos += sz;

		uint32_t idx = (uint32_t)out_buffer.size();
		WriteToBuffer(uint32_t, deflate_size, out_buffer, idx);
		WriteToBuffer(uint32_t, sz, out_buffer, idx + 4);
		out_buffer.insert(out_buffer.end(), block, block + deflate_size);
	}

	return true;
}

} // namespace EQEmu::PFS
