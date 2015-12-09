//
// ZoneData.cpp
//

#include "ZoneData.h"
#include "../MQ2plugin.h"

#include "dependencies/zone-utilities/common/eqg_model_loader.h"
#include "dependencies/zone-utilities/common/safe_alloc.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iterator>

typedef std::shared_ptr<EQEmu::EQG::Geometry> ModelPtr;
typedef std::shared_ptr<EQEmu::S3D::Geometry> OldModelPtr;

class ZoneDataLoader
{
public:
	virtual ~ZoneDataLoader() {}

	virtual bool Load() = 0;

	virtual bool GetBoundingBox(const std::string& modelName, BoundingBox& bb) = 0;
};

//----------------------------------------------------------------------------

class EQGDataLoader : public ZoneDataLoader
{
public:
	EQGDataLoader(ZoneData* zd) : m_zd(zd) {}

	static std::string GetZoneFile(ZoneData* zd)
	{
		return (boost::format("%s\\%s.eqg")
			% zd->GetEQPath()
			% zd->GetZoneName()).str();
	}

	static bool IsValid(ZoneData* zd)
	{
		return boost::filesystem::exists(GetZoneFile(zd), boost::system::error_code());
	}

	virtual bool Load() override
	{
		return m_archive.Open(GetZoneFile(m_zd));
	}

	ModelPtr GetModel(const std::string& modelName)
	{
		std::string name = boost::to_lower_copy(modelName) + ".mod";

		auto iter = m_models.find(modelName);
		if (iter != m_models.end())
		{
			return iter->second;
		}

		EQEmu::EQGModelLoader model_loader;
		ModelPtr model;

		if (model_loader.Load(m_archive, name, model))
		{
			model->SetName(modelName);
			m_models[modelName] = model;
		}

		return model;
	}

	virtual bool GetBoundingBox(const std::string& modelName, BoundingBox& bb) override
	{
		if (ModelPtr model = GetModel(modelName))
		{
			for (auto& vert : model->GetVertices())
			{
				bb.min.x = std::min(vert.pos.y, bb.min.x);
				bb.min.y = std::min(vert.pos.x, bb.min.y);
				bb.min.z = std::min(vert.pos.z, bb.min.z);

				bb.max.x = std::max(vert.pos.y, bb.max.x);
				bb.max.y = std::max(vert.pos.x, bb.max.y);
				bb.max.z = std::max(vert.pos.z, bb.max.z);
			}

			bb.newModel = model;

			return true;
		}

		return false;
	}

private:
	ZoneData* m_zd;
	EQEmu::PFS::Archive m_archive;
	std::map<std::string, ModelPtr> m_models;
};

//----------------------------------------------------------------------------

class S3DDataLoader : public ZoneDataLoader
{
public:
	S3DDataLoader(ZoneData* zd) : m_zd(zd) {}

	static bool IsValid(ZoneData* zd)
	{
		std::string filename = (boost::format("%s\\%s.s3d")
			% zd->GetEQPath()
			% zd->GetZoneName()).str();

		return boost::filesystem::exists(filename, boost::system::error_code());
	}

	virtual bool Load() override
	{
		std::vector<EQEmu::S3D::WLDFragment> zone_object_frags;

		std::string base_filename = (boost::format("%s\\%s")
			% m_zd->GetEQPath()
			% m_zd->GetZoneName()).str();
		bool loadedSomething = false;

		EQEmu::PFS::Archive archive;

		for (int i = 0; i < 3; i++)
		{
			std::string suffix;
			if (i > 0)
				suffix = "_obj";
			if (i > 1)
				suffix += std::to_string(i);

			std::string wld_name = m_zd->GetZoneName() + suffix + ".wld";
			std::string file_name = (boost::format("%s\\%s.s3d")
				% m_zd->GetEQPath() % (m_zd->GetZoneName() + suffix)).str();

			EQEmu::S3DLoader loader;
			std::vector<EQEmu::S3D::WLDFragment> frags;

			if (loader.ParseWLDFile(file_name, wld_name, frags))
			{
				for (auto& frag : frags)
				{
					if (frag.type == 0x36)
					{
						EQEmu::S3D::WLDFragment36 &frag36 = reinterpret_cast<EQEmu::S3D::WLDFragment36&>(frag);
						auto model = frag36.GetData();

						if (m_s3dModels.find(model->GetName()) == m_s3dModels.end())
						{
							DebugSpewAlways("Loaded S3D Model: %s", model->GetName().c_str());

							m_s3dModels[model->GetName()] = model;
							loadedSomething = true;
						}
					}
				}
			}
		}

		// next we need to try to read an _assets file and load more eqg based data.
		std::string assets_file = base_filename + "_assets.txt";
		boost::system::error_code ec;
		if (boost::filesystem::exists(assets_file, ec))
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
					std::string asset_file = (boost::format("%s\\%s") % m_zd->GetEQPath() % name).str();
					EQEmu::PFS::Archive archive;

					if (!archive.Open(asset_file))
						continue;

					std::vector<std::string> models;

					if (archive.GetFilenames("mod", models))
					{
						for (auto& modelName : models)
						{
							EQEmu::EQGModelLoader model_loader;
							ModelPtr model;

							model_loader.Load(archive, modelName, model);
							if (model)
							{
								DebugSpewAlways("Loaded EQG Model: %s", modelName.c_str());

								model->SetName(modelName);
								m_eqgModels[modelName] = model;
								loadedSomething = true;
							}
						}
					}
				}
			}
		}


		return loadedSomething;
	}

	OldModelPtr GetS3dModel(const std::string& modelName)
	{
		std::string actualName = boost::to_upper_copy(modelName) + "_DMSPRITEDEF";

		auto iter = m_s3dModels.find(actualName);
		if (iter != m_s3dModels.end())
			return iter->second;

		return nullptr;
	}

	ModelPtr GetEQGModel(const std::string& modelName)
	{
		std::string eqgName = boost::to_lower_copy(modelName) + ".mod";

		auto iter = m_eqgModels.find(eqgName);
		if (iter != m_eqgModels.end())
			return iter->second;

		return nullptr;
	}

	virtual bool GetBoundingBox(const std::string& modelName, BoundingBox& bb) override
	{
		// try to find the s3d model first, that is the most common
		if (OldModelPtr model = GetS3dModel(modelName))
		{
			for (auto& vert : model->GetVertices())
			{
				bb.min.x = std::min(vert.pos.y, bb.min.x);
				bb.min.y = std::min(vert.pos.x, bb.min.y);
				bb.min.z = std::min(vert.pos.z, bb.min.z);

				bb.max.x = std::max(vert.pos.y, bb.max.x);
				bb.max.y = std::max(vert.pos.x, bb.max.y);
				bb.max.z = std::max(vert.pos.z, bb.max.z);
			}

			bb.oldModel = model;

			return true;
		}

		// otherwise, if we have eqg models, try them too
		if (!m_eqgModels.empty())
		{
			if (ModelPtr model = GetEQGModel(modelName))
			{
				for (auto& vert : model->GetVertices())
				{
					bb.min.x = std::min(vert.pos.y, bb.min.x);
					bb.min.y = std::min(vert.pos.x, bb.min.y);
					bb.min.z = std::min(vert.pos.z, bb.min.z);

					bb.max.x = std::max(vert.pos.y, bb.max.x);
					bb.max.y = std::max(vert.pos.x, bb.max.y);
					bb.max.z = std::max(vert.pos.z, bb.max.z);
				}

				bb.newModel = model;

				return true;
			}
		}

		return false;
	}

private:
	ZoneData* m_zd;

	std::map<std::string, OldModelPtr> m_s3dModels;
	std::map<std::string, ModelPtr> m_eqgModels;
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

bool ZoneData::GetBoundingBox(const std::string& modelName, BoundingBox& bb)
{
	if (m_loader)
		return m_loader->GetBoundingBox(modelName, bb);

	return false;
}

bool ZoneData::IsLoaded()
{
	return m_loader != nullptr;
}
