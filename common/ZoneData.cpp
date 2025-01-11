//
// ZoneData.cpp
//

#include "ZoneData.h"

#include "eqglib/eqg_loader.h"
#include "eqglib/s3d_types.h"

#include <fmt/format.h>
#include <mq/base/String.h>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <filesystem>

namespace fs = std::filesystem;

class ZoneDataLoader
{
public:
	virtual ~ZoneDataLoader() {}

	virtual bool Load() = 0;

	virtual std::shared_ptr<ModelInfo> GetModelInfo(const std::string& modelName) = 0;
};

//----------------------------------------------------------------------------

class EQGDataLoader : public ZoneDataLoader
{
public:
	EQGDataLoader(ZoneData* zd) : m_zd(zd) {}

	static std::string GetZoneFile(ZoneData* zd)
	{
		return fmt::format("{}\\{}.eqg", zd->GetEQPath(), zd->GetZoneName());
	}

	static bool IsValid(ZoneData* zd)
	{
		std::error_code ec;
		return fs::exists(GetZoneFile(zd), ec);
	}

	virtual bool Load() override
	{
		bool loadedSomething = m_archive.Open(GetZoneFile(m_zd));

		std::string base_filename = fmt::format("{}\\{}", m_zd->GetEQPath(), m_zd->GetZoneName());

		// next we need to try to read an _assets file and load more eqg based data.
		std::string assets_file = base_filename + "_assets.txt";
		std::error_code ec;
		if (fs::exists(assets_file, ec))
		{
			std::vector<std::string> filenames;
			std::ifstream assets(assets_file.c_str());

			if (assets.is_open())
			{
				std::copy(std::istream_iterator<std::string>(assets),
					std::istream_iterator<std::string>(),
					std::back_inserter(filenames));

				for (auto& name : filenames)
				{
					std::string asset_file = fmt::format("{}\\{}", m_zd->GetEQPath(), name);
					eqg::Archive archive;

					if (!archive.Open(asset_file))
						continue;

					std::vector<std::string> models = archive.GetFileNames("mod");
					for (auto& modelName : models)
					{
						EQGGeometryPtr model;

						eqg::LoadEQGModel(archive, modelName, model);
						if (model)
						{
							model->SetName(modelName);
							m_modelsByFile[modelName] = model;
							loadedSomething = true;
						}
					}
				}
			}
		}

		return loadedSomething;
	}

	EQGGeometryPtr GetModel(const std::string& modelName)
	{
		std::string name = mq::to_lower_copy(modelName) + ".mod";
		auto fileIter = m_modelsByFile.find(name);
		if (fileIter != m_modelsByFile.end())
		{
			return fileIter->second;
		}

		auto iter = m_models.find(modelName);
		if (iter != m_models.end())
		{
			return iter->second;
		}

		EQGGeometryPtr model;
		if (eqg::LoadEQGModel(m_archive, name, model))
		{
			model->SetName(modelName);
			m_models[modelName] = model;
		}

		return model;
	}

	virtual std::shared_ptr<ModelInfo> GetModelInfo(const std::string& modelName) override
	{
		if (EQGGeometryPtr model = GetModel(modelName))
		{
			std::shared_ptr<ModelInfo> modelInfo = std::make_shared<ModelInfo>();

			for (auto& vert : model->GetVertices())
			{
				modelInfo->min.x = std::min(vert.pos.y, modelInfo->min.x);
				modelInfo->min.y = std::min(vert.pos.x, modelInfo->min.y);
				modelInfo->min.z = std::min(vert.pos.z, modelInfo->min.z);

				modelInfo->max.x = std::max(vert.pos.y, modelInfo->max.x);
				modelInfo->max.y = std::max(vert.pos.x, modelInfo->max.y);
				modelInfo->max.z = std::max(vert.pos.z, modelInfo->max.z);
			}

			modelInfo->newModel = model;
			return modelInfo;
		}

		return nullptr;
	}

private:
	ZoneData* m_zd;
	eqg::Archive m_archive;

	std::map<std::string, EQGGeometryPtr> m_models;
	std::map<std::string, EQGGeometryPtr> m_modelsByFile;
};

//----------------------------------------------------------------------------

class S3DDataLoader : public ZoneDataLoader
{
public:
	S3DDataLoader(ZoneData* zd) : m_zd(zd) {}

	static bool IsValid(ZoneData* zd)
	{
		std::string filename = fmt::format("{}\\{}.s3d", zd->GetEQPath(), zd->GetZoneName());

		std::error_code ec;
		return fs::exists(filename, ec);
	}

	virtual bool Load() override
	{
		std::vector<eqg::WLDFragment> zone_object_frags;

		std::string base_filename = fmt::format("{}\\{}", m_zd->GetEQPath(), m_zd->GetZoneName());
		bool loadedSomething = false;

		eqg::Archive archive;

		for (int i = 0; i < 3; i++)
		{
			std::string suffix;
			if (i > 0)
				suffix = "_obj";
			if (i > 1)
				suffix += std::to_string(i);

			std::string wld_name = m_zd->GetZoneName() + suffix + ".wld";
			std::string file_name = fmt::format("{}\\{}.s3d", m_zd->GetEQPath(), m_zd->GetZoneName() + suffix);

			eqg::WLDLoader loader;

			if (loader.ParseWLDFile(loader, file_name, wld_name))
			{
				for (uint32_t i = 1; i < loader.GetNumObjects(); ++i)
				{
					auto& frag = loader.GetObject(i);

					if (frag.type == eqg::WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
					{
						eqg::WLDFragment36* frag36 = static_cast<eqg::WLDFragment36*>(frag.parsed_data);
						auto model = frag36->geometry;

						if (!m_s3dModels.contains(model->GetName()))
						{
							m_s3dModels[model->GetName()] = model;
							loadedSomething = true;
						}
					}
				}
			}
		}

		// next we need to try to read an _assets file and load more eqg based data.
		std::string assets_file = base_filename + "_assets.txt";
		std::error_code ec;
		if (fs::exists(assets_file, ec))
		{
			std::vector<std::string> filenames;
			std::ifstream assets(assets_file.c_str());

			if (assets.is_open())
			{
				std::copy(std::istream_iterator<std::string>(assets),
					std::istream_iterator<std::string>(),
					std::back_inserter(filenames));

				if (m_zd->GetZoneName() == "poknowledge")
				{
					filenames.push_back("poknowledge_obj3.eqg");
				}

				for (auto& name : filenames)
				{
					std::string asset_file = fmt::format("{}\\{}", m_zd->GetEQPath(), name);
					eqg::Archive archive;

					if (!archive.Open(asset_file))
						continue;

					std::vector<std::string> models = archive.GetFileNames(".mod");

					for (auto& modelName : models)
					{
						EQGGeometryPtr model;

						eqg::LoadEQGModel(archive, modelName, model);
						if (model)
						{
							model->SetName(modelName);
							m_eqgModels[modelName] = model;
							loadedSomething = true;
						}
					}
				}
			}
		}

		return loadedSomething;
	}

	S3DGeometryPtr GetS3dModel(const std::string& modelName)
	{
		std::string actualName = mq::to_upper_copy(modelName) + "_DMSPRITEDEF";

		auto iter = m_s3dModels.find(actualName);
		if (iter != m_s3dModels.end())
			return iter->second;

		return nullptr;
	}

	EQGGeometryPtr GetEQGModel(const std::string& modelName)
	{
		std::string eqgName = mq::to_lower_copy(modelName) + ".mod";

		auto iter = m_eqgModels.find(eqgName);
		if (iter != m_eqgModels.end())
			return iter->second;

		return nullptr;
	}

	virtual std::shared_ptr<ModelInfo> GetModelInfo(const std::string& modelName) override
	{
		// try to find the s3d model first, that is the most common
		if (S3DGeometryPtr model = GetS3dModel(modelName))
		{
			std::shared_ptr<ModelInfo> modelInfo = std::make_shared<ModelInfo>();

			for (auto& vert : model->GetVertices())
			{
				modelInfo->min.x = std::min(vert.pos.y, modelInfo->min.x);
				modelInfo->min.y = std::min(vert.pos.x, modelInfo->min.y);
				modelInfo->min.z = std::min(vert.pos.z, modelInfo->min.z);

				modelInfo->max.x = std::max(vert.pos.y, modelInfo->max.x);
				modelInfo->max.y = std::max(vert.pos.x, modelInfo->max.y);
				modelInfo->max.z = std::max(vert.pos.z, modelInfo->max.z);
			}

			modelInfo->oldModel = model;
			return modelInfo;
		}

		// otherwise, if we have eqg models, try them too
		if (EQGGeometryPtr model = GetEQGModel(modelName))
		{
			std::shared_ptr<ModelInfo> modelInfo = std::make_shared<ModelInfo>();

			for (auto& vert : model->GetVertices())
			{
				modelInfo->min.x = std::min(vert.pos.y, modelInfo->min.x);
				modelInfo->min.y = std::min(vert.pos.x, modelInfo->min.y);
				modelInfo->min.z = std::min(vert.pos.z, modelInfo->min.z);

				modelInfo->max.x = std::max(vert.pos.y, modelInfo->max.x);
				modelInfo->max.y = std::max(vert.pos.x, modelInfo->max.y);
				modelInfo->max.z = std::max(vert.pos.z, modelInfo->max.z);
			}

			modelInfo->newModel = model;
			return modelInfo;
		}

		return nullptr;
	}

private:
	ZoneData* m_zd;

	std::map<std::string, S3DGeometryPtr> m_s3dModels;
	std::map<std::string, EQGGeometryPtr> m_eqgModels;
};

//----------------------------------------------------------------------------

ZoneData::ZoneData(const std::string& eqPath, const std::string& zoneName)
	: m_eqPath(eqPath)
	, m_zoneName(zoneName)
{
	LoadZone();
}

ZoneData::~ZoneData()
{
}

void ZoneData::LoadZone()
{
	m_loader.reset();

	if (EQGDataLoader::IsValid(this))
		m_loader = std::make_unique<EQGDataLoader>(this);
	else if (S3DDataLoader::IsValid(this))
		m_loader = std::make_unique<S3DDataLoader>(this);
	else
		return;

	if (!m_loader->Load())
		m_loader.reset();
}

std::shared_ptr<ModelInfo> ZoneData::GetModelInfo(const std::string& modelName)
{
	auto iter = m_modelInfo.find(modelName);
	if (iter != m_modelInfo.end())
		return iter->second;

	return m_loader ? m_loader->GetModelInfo(modelName) : nullptr;
}

bool ZoneData::IsLoaded()
{
	return m_loader != nullptr;
}
