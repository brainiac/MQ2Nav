
#pragma once

#include "pfs.h"
#include "wld_fragment.h"
#include "wld_structs.h"

#include <vector>
#include <stdint.h>
#include <string>

#ifdef GetObject
#undef GetObject
#endif

namespace EQEmu::S3D {

void decode_s3d_string(char* str, size_t len);

class WLDLoader
{
public:
	WLDLoader();
	~WLDLoader();

	// Typically, the archive would be <zonename>.s3d and the file would be <zonename>.wld
	bool Init(PFS::Archive* archive, const std::string& fileName);

	void Reset();

	uint32_t GetObjectIndexFromID(int nID, uint32_t currID = (uint32_t)-1);

	S3DFileObject& GetObjectFromID(int nID, uint32_t currID = (uint32_t)-1)
	{
		return m_objects[GetObjectIndexFromID(nID, currID)];
	}

	uint32_t GetNumObjects() const { return (uint32_t)m_objects.size(); }
	S3DFileObject& GetObject(uint32_t index) { return m_objects[index]; }

	std::vector<S3D::S3DFileObject>& GetObjectList() { return m_objects; }

	std::string_view GetString(int nID) const
	{
		return &m_stringPool[-nID];
	}

	static bool ParseWLDFile(WLDLoader& loader,
		const std::string& file_name, const std::string& wld_name);

	bool IsOldVersion() const { return m_oldVersion; }
	bool IsValid() const { return m_valid; }

	const std::string& GetFileName() const { return m_fileName; }

private:
	bool ParseWLDObjects();

private:
	PFS::Archive*              m_archive = nullptr;
	std::string                m_fileName;
	std::unique_ptr<uint8_t[]> m_wldFileContents;
	uint32_t                   m_wldFileSize = 0;
	uint32_t                   m_version = 0;
	uint32_t                   m_numObjects = 0;
	uint32_t                   m_numRegions = 0;
	uint32_t                   m_maxObjectSize = 0;
	uint32_t                   m_stringPoolSize = 0;
	uint32_t                   m_numStrings = 0;
	char*                      m_stringPool = nullptr;
	std::vector<S3D::S3DFileObject> m_objects;
	bool                       m_oldVersion = false;
	bool                       m_valid = false;
};

} // namespace EQEmu
