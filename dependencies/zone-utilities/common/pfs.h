
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace EQEmu::PFS {

class Archive
{
public:
	Archive();
	~Archive();

	bool Open();
	bool Open(uint32_t date);
	bool Open(std::string filename);
	bool Save(std::string filename);
	void Close();

	bool Get(std::string filename, std::vector<char>& buf);
	std::unique_ptr<uint8_t[]> Get(std::string filename, uint32_t& size);

	bool Set(std::string filename, const std::vector<char>& buf);
	bool Delete(std::string filename);
	bool Rename(std::string filename, std::string filename_new);
	bool Exists(std::string filename);
	bool GetFilenames(std::string ext, std::vector<std::string>& out_files);

private:
	bool StoreBlocksByFileOffset(uint32_t offset, uint32_t size, const std::vector<char>& in_buffer, std::string filename);
	std::unique_ptr<uint8_t[]> InflateByFileOffset(uint32_t offset, uint32_t size, const std::vector<char>& in_buffer);
	bool WriteDeflatedFileBlock(const std::vector<char>& file, std::vector<char>& out_buffer);

	std::map<std::string, std::vector<char>> m_files;
	std::map<std::string, uint32_t> m_files_uncompressed_size;
	bool m_footer;
	uint32_t m_footer_date;
};

} // namespace EQEmu::PFS
