
#pragma once

#include "container_util.h"

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>

namespace eqg {

class Archive
{
public:
	Archive();
	~Archive();

	bool Open(const std::string& filename);
	void Close();

	const std::string& GetFileName() const { return m_archiveName; }

	std::vector<std::string> GetFileNames(std::string_view ext) const;
	const std::vector<std::string>& GetFileNames() const { return m_fileNames; }

	bool Get(int crc, std::vector<char>& buf) const;
	bool Get(std::string_view filename, std::vector<char>& buf) const;

	std::unique_ptr<uint8_t[]> Get(int crc, uint32_t& size) const;
	std::unique_ptr<uint8_t[]> Get(std::string_view filename, uint32_t& size) const;

	bool Exists(int crc) const;
	bool Exists(std::string_view filename) const;

private:
	bool StoreBlocksByFileOffset(const std::vector<char>& in_buffer, uint32_t offset, uint32_t size, int crc);
	std::unique_ptr<uint8_t[]> InflateByFileOffset(uint32_t offset, uint32_t size, const std::vector<char>& in_buffer) const;

	struct FileEntry
	{
		std::string_view fileName;
		std::vector<char> data;
		uint32_t compressedSize;
		uint32_t uncompressedSize;
		int crc;
	};

	struct CRCKey
	{
		CRCKey(int value_) : value(value_) {}
		CRCKey(const CRCKey& other) : value(other.value) {}
		CRCKey(std::string_view str);

		bool operator==(const CRCKey& other) const { return value == other.value; }

		struct equals
		{
			bool operator()(const CRCKey& a, const CRCKey& b) const { return a == b; }

			template <typename T, typename U>
			bool operator()(const T& a, const U& b) const { return CRCKey(a) == CRCKey(b); }

			using is_transparent = void;
		};

		struct hash
		{
			size_t operator()(CRCKey a) const { return (size_t)a.value; }

			template <typename T>
			size_t operator()(const T& t) const { return CRCKey(t).value; }

			using is_transparent = void;
		};

		int value;
	};

	std::string m_archiveName;
	std::unordered_map<CRCKey, FileEntry, CRCKey::hash, CRCKey::equals> m_fileEntries;

	std::vector<std::string> m_fileNames;
	ci_ordered::map<std::string_view, FileEntry*> m_fileEntriesByName;
};

} // namespace eqg
