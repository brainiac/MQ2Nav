
#pragma once

#include <string>

class dtNavMesh;

class NavMeshFile
{
public:
	NavMeshFile(const std::string& zoneShortName);
	~NavMeshFile();

	void SetFilename(const std::string& filename);
	std::string GetFilename() const;

	bool HasModifications() const;

	bool LoadFromDisk();
	void SaveToDisk();

	void SetNavMesh(dtNavMesh* navMesh);
	dtNavMesh* GetNavMesh() const;
};
