//
// ZoneData.h
//

#pragma once


#include "dependencies/zone-utilities/common/eqg_loader.h"
#include "dependencies/zone-utilities/common/s3d_loader.h"


class ZoneDataLoader;

struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;
};

class ZoneData
{
public:
	ZoneData(const std::string& eqPath, const std::string& zoneName);
	~ZoneData();

	bool IsLoaded();

	bool GetBoundingBox(const std::string& modelName, BoundingBox& bb);

	std::string GetZoneName() const { return m_zoneName; }
	std::string GetEQPath() const { return m_eqPath; }

private:
	void LoadZone();
	
	std::string m_zoneName;
	std::string m_eqPath;

	// For EQG files
	std::unique_ptr<ZoneDataLoader> m_loader;
};
