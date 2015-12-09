//
// ZoneData.h
//

#pragma once


#include "dependencies/zone-utilities/common/eqg_loader.h"
#include "dependencies/zone-utilities/common/s3d_loader.h"

typedef std::shared_ptr<EQEmu::EQG::Geometry> ModelPtr;
typedef std::shared_ptr<EQEmu::S3D::Geometry> OldModelPtr;

class ZoneDataLoader;

struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;

	OldModelPtr oldModel;
	ModelPtr newModel;
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
