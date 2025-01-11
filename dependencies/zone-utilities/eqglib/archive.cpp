
#include "pch.h"
#include "archive.h"

#include "buffer_reader.h"
#include "pfs_crc.h"
#include "str_util.h"

#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace eqg {

static uint32_t InflateData(const char* buffer, uint32_t len, char* out_buffer, uint32_t out_len_max)
{
	z_stream zstream;
	int zerror = 0;
	int i;

	zstream.next_in = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(buffer));
	zstream.avail_in = len;
	zstream.next_out = reinterpret_cast<unsigned char*>(out_buffer);;
	zstream.avail_out = out_len_max;
	zstream.zalloc = Z_NULL;
	zstream.zfree = Z_NULL;
	zstream.opaque = Z_NULL;

	i = inflateInit2(&zstream, 15);
	if (i != Z_OK)
	{
		return 0;
	}

	zerror = inflate(&zstream, Z_FINISH);
	if (zerror == Z_STREAM_END)
	{
		inflateEnd(&zstream);
		return zstream.total_out;
	}

	if (zerror == -4 && zstream.msg == 0)
	{
		return 0;
	}

	zerror = inflateEnd(&zstream);
	return 0;
}


Archive::CRCKey::CRCKey(std::string_view s)
{
	value = CRC().Get(s);
}

Archive::Archive()
{
}

Archive::~Archive()
{
	Close();
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

	m_archiveName = filename;

	BufferReader reader(buffer);

	uint32_t dir_offset = reader.read<uint32_t>();
	uint32_t magic = reader.read<uint32_t>();
	if (magic != ' SFP')
		return false;

	reader.seek(dir_offset);
	uint32_t dir_count = reader.read<uint32_t>();

	struct ArchiveFileRecord
	{
		int32_t crc;
		uint32_t offset;
		uint32_t size;
	};
	std::vector<ArchiveFileRecord> archive_entries;
	archive_entries.resize(dir_count);

	for (uint32_t i = 0; i < dir_count; ++i)
	{
		reader.read(archive_entries[i]);
	}

	// Insert entries by CRC
	for (const auto& entry : archive_entries)
	{
		if (!StoreBlocksByFileOffset(buffer, entry.offset, entry.size, entry.crc))
			return false;
	}

	// We can now look up files by name via the crc algorithm.
	auto iter = m_fileEntries.find("__PFSFileNames__");
	if (iter != m_fileEntries.end())
	{
		auto& entry = iter->second;

		// Add filenames.
		std::vector<std::string> fileNames;

		auto filenamesBuffer = InflateByFileOffset(0, entry.uncompressedSize, entry.data);
		if (!filenamesBuffer)
			return false;

		BufferReader filenamesReader(filenamesBuffer.get(), entry.uncompressedSize);
		uint32_t filename_count;
		if (!filenamesReader.read(filename_count))
			return false;

		fileNames.reserve(filename_count);
		for (uint32_t j = 0; j < filename_count; ++j)
		{
			uint32_t filename_length;
			if (!filenamesReader.read(filename_length))
				return false;

			std::string filename;
			filename.resize(filename_length - 1);
			if (!filenamesReader.read(&filename[0], filename_length))
				return false;

			fileNames.push_back(std::move(filename));
		}

		// After we have all the filenames, we can map them to our crc entries.
		m_fileNames = std::move(fileNames);

		for (const std::string& name : m_fileNames)
		{
			auto iter = m_fileEntries.find(name);
			if (iter == m_fileEntries.end())
				return false;

			iter->second.fileName = name;
			m_fileEntriesByName.emplace(name, &iter->second);
		}
	}

	return true;
}

void Archive::Close()
{
	m_fileEntries.clear();
	m_archiveName.clear();
	m_fileNames.clear();
	m_fileEntriesByName.clear();
}

bool Archive::Get(std::string_view filename, std::vector<char>& buf) const
{
	return Get(CRCKey(filename).value, buf);
}

bool Archive::Get(int crc, std::vector<char>& buf) const
{
	auto iter = m_fileEntries.find(crc);
	if (iter == m_fileEntries.end())
		return false;

	buf.clear();

	auto data = InflateByFileOffset(0, iter->second.uncompressedSize, iter->second.data);
	if (!data)
		return false;

	buf.resize(iter->second.uncompressedSize);
	memcpy(&buf[0], data.get(), iter->second.uncompressedSize);

	return true;
}

std::unique_ptr<uint8_t[]> Archive::Get(std::string_view filename, uint32_t& size) const
{
	return Get(CRCKey(filename).value, size);
}

std::unique_ptr<uint8_t[]> Archive::Get(int crc, uint32_t& size) const
{
	auto iter = m_fileEntries.find(crc);
	if (iter == m_fileEntries.end())
		return nullptr;

	size = iter->second.uncompressedSize;
	return InflateByFileOffset(0, size, iter->second.data);
}

bool Archive::Exists(int crc) const
{
	return m_fileEntries.contains(crc);
}

bool Archive::Exists(std::string_view filename) const
{
	return m_fileEntries.contains(filename);
}

std::vector<std::string> Archive::GetFileNames(std::string_view ext) const
{
	size_t elen = ext.length();
	bool all_files = ext.compare("*") == 0;
	std::vector<std::string> out_files;

	for (const auto& name : m_fileNames)
	{
		if (all_files)
		{
			out_files.emplace_back(name);
		}
		else if (name.length() > elen)
		{
			if (ci_ends_with(name, ext))
			{
				out_files.emplace_back(name);
			}
		}
	}

	return out_files;
}

bool Archive::StoreBlocksByFileOffset(
	const std::vector<char>& in_buffer,
	uint32_t offset,
	uint32_t size,
	int crc)
{
	uint32_t position = offset;
	uint32_t block_size = 0;
	uint32_t inflate = 0;

	BufferReader reader(in_buffer);
	reader.seek(offset);

	// Traverse the compressed blocks and calculate the total size
	while (inflate < size)
	{
		uint32_t deflate_length = reader.read<uint32_t>();
		uint32_t inflate_length = reader.read<uint32_t>();
		reader.skip(deflate_length);

		inflate += inflate_length;
	}

	block_size = (uint32_t)reader.pos() - offset;

	std::vector<char> tbuffer;
	tbuffer.resize(block_size);

	memcpy(&tbuffer[0], &in_buffer[offset], block_size);

	m_fileEntries.emplace(std::piecewise_construct,
		std::forward_as_tuple(crc),
		std::forward_as_tuple(std::string_view(), std::move(tbuffer), size, inflate, crc));

	return true;
}

std::unique_ptr<uint8_t[]> Archive::InflateByFileOffset(
	uint32_t offset,
	uint32_t size,
	const std::vector<char>& in_buffer) const
{
	std::unique_ptr<uint8_t[]> out_buffer = std::make_unique<uint8_t[]>(size);
	memset(out_buffer.get(), 0, size);

	BufferReader reader(in_buffer.data() + offset, in_buffer.size());
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

		uint32_t inflate_out = InflateData(&temp_buffer[0], deflate_length,
			reinterpret_cast<char*>(out_buffer.get()) + inflate, inflate_length);
		if (inflate_out != inflate_length)
			return nullptr;
		inflate += inflate_length;
	}

	return out_buffer;
}

} // namespace eqg
