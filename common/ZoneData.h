//
// ZoneData.h
//

#pragma once

#include "zone-utilities/common/eqg_loader.h"
#include "zone-utilities/common/wld_loader.h"

typedef std::shared_ptr<EQEmu::EQG::Geometry> ModelPtr;
typedef std::shared_ptr<EQEmu::S3D::Geometry> OldModelPtr;

class ZoneDataLoader;

struct ModelInfo
{
	glm::vec3 min = { 0, 0, 0 };
	glm::vec3 max = { 0, 0, 0 };

	OldModelPtr oldModel;
	ModelPtr newModel;
};

class ZoneData
{
public:
	ZoneData(const std::string& eqPath, const std::string& zoneName);
	~ZoneData();

	bool IsLoaded();

	std::shared_ptr<ModelInfo> GetModelInfo(const std::string& modelName);

	std::string GetZoneName() const { return m_zoneName; }
	std::string GetEQPath() const { return m_eqPath; }

private:
	void LoadZone();
	
	std::string m_zoneName;
	std::string m_eqPath;

	// For EQG files
	std::unique_ptr<ZoneDataLoader> m_loader;
	std::map<std::string, std::shared_ptr<ModelInfo>> m_modelInfo;
};
