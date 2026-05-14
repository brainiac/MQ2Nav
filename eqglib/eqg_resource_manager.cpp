#include "pch.h"
#include "eqg_resource_manager.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_animation.h"
#include "eqg_geometry.h"
#include "eqg_material.h"
#include "eqg_particles.h"
#include "eqg_terrain.h"
#include "log_internal.h"

#include <ddraw.h> // for DDSURFACEDESC2
#include <filesystem>

namespace eqg {

std::vector<ResourceManager*> s_resourceMgrStack;

ResourceManager::ResourceManager(const std::string& data_path, ResourceManager* parent)
	: m_dataPath(data_path)
	, m_parent(parent)
{
	// Prepopulate the sorted resource mapping
	for (int i = 0; i < static_cast<int>(ResourceType::Max); ++i)
	{
		m_sortedResources[static_cast<ResourceType>(i)] = std::map<std::string_view, std::shared_ptr<Resource>>();
	}
}

ResourceManager::~ResourceManager()
{
}

ResourceManager* ResourceManager::Get()
{
	if (s_resourceMgrStack.empty())
		return nullptr;

	return s_resourceMgrStack[s_resourceMgrStack.size() - 1];
}

void ResourceManager::PushActive(ResourceManager* resourceMgr)
{
	s_resourceMgrStack.push_back(resourceMgr);
}

void ResourceManager::PopActive()
{
	s_resourceMgrStack.pop_back();
}

//-------------------------------------------------------------------------------------------------

std::shared_ptr<Resource> ResourceManager::Get(std::string_view tag, ResourceType type) const
{
	auto it = m_resources.find(std::make_pair(tag, type));
	if (it != m_resources.end())
		return it->second;

	if (m_parent)
	{
		return m_parent->Get(tag, type);
	}

	return nullptr;
}

bool ResourceManager::Add(std::string_view tag, ResourceType type, const std::shared_ptr<Resource>& value)
{
	auto result = m_resources.try_emplace(std::make_pair(tag, type), value);

	if (result.second)
	{
		m_sortedResources[type].emplace(result.first->first.tag, value);
	}

	return result.second;
}

bool ResourceManager::Contains(std::string_view tag, ResourceType type) const
{
	return m_resources.contains(std::make_pair(tag, type));
}

//-------------------------------------------------------------------------------------------------

std::shared_ptr<Bitmap> ResourceManager::CreateBitmap() const
{
	return std::make_shared<Bitmap>();
}

std::shared_ptr<Bitmap> ResourceManager::CreateBitmap(std::string_view fileName, Archive* archive, bool cubeMap, bool rawData)
{
	if (auto pBitmap = Get<eqg::Bitmap>(fileName))
	{
		return pBitmap;
	}

	auto pBitmap = CreateBitmap();

	if (!pBitmap->Init(fileName, archive, cubeMap, rawData))
	{
		return nullptr;
	}

	Add(pBitmap);

	return pBitmap;
}

std::shared_ptr<SimpleModelDefinition> ResourceManager::CreateSimpleModelDefinition() const
{
	return std::make_shared<SimpleModelDefinition>();
}

std::shared_ptr<SimpleModel> ResourceManager::CreateSimpleModel() const
{
	return std::make_shared<SimpleModel>();
}

std::shared_ptr<HierarchicalModelDefinition> ResourceManager::CreateHierarchicalModelDefinition() const
{
	return std::make_shared<HierarchicalModelDefinition>();
}

std::shared_ptr<HierarchicalModel> ResourceManager::CreateHierarchicalModel() const
{
	return std::make_shared<HierarchicalModel>();
}

std::shared_ptr<BlitSpriteDefinition> ResourceManager::CreateBlitSpriteDefinition() const
{
	return std::make_shared<BlitSpriteDefinition>();
}

std::shared_ptr<Animation> ResourceManager::CreateAnimation() const
{
	return std::make_shared<Animation>();
}

std::shared_ptr<LightDefinition> ResourceManager::CreateLightDefinition() const
{
	return std::make_shared<LightDefinition>();
}

std::shared_ptr<SimpleActor> ResourceManager::CreateSimpleActor(
	std::string_view actorTag,
	const std::shared_ptr<ActorDefinition>& simpleActorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float scale,
	ECollisionVolumeType collisionVolumeType,
	float boundingRadius,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	const std::span<uint32_t>& RGBs,
	std::string_view actorName)
{
	return std::make_shared<SimpleActor>(this, actorTag, simpleActorDef, position, orientation,
		scale, boundingRadius, collisionVolumeType, actorIndex, DMRGBTrackWLDData, RGBs, actorName);
}

std::shared_ptr<SimpleActor> ResourceManager::CreateSimpleActor(
	std::string_view actorTag,
	const std::shared_ptr<ActorDefinition>& simpleActorDef,
	int actorIndex,
	bool useDefaultBoundingRadius,
	std::string_view actorName)
{
	return std::make_shared<SimpleActor>(this, actorTag, simpleActorDef, actorIndex, useDefaultBoundingRadius,
		actorName);
}

std::shared_ptr<HierarchicalActor> ResourceManager::CreateHierarchicalActor(
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float scale,
	ECollisionVolumeType collisionVolumeType,
	float boundingRadius,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	const std::span<uint32_t>& RGBs,
	std::string_view actorName)
{
	return std::make_shared<HierarchicalActor>(this, actorTag, actorDef, position, orientation,
		boundingRadius, scale, collisionVolumeType, actorIndex, DMRGBTrackWLDData, RGBs, actorName);
}

std::shared_ptr<HierarchicalActor> ResourceManager::CreateHierarchicalActor(
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	int actorIndex,
	bool allSkinsActive,
	bool useDefaultBoundingRadius,
	bool sharedBoneGroups,
	Bone* pBone,
	std::string_view actorName)
{
	return std::make_shared<HierarchicalActor>(this, actorTag, actorDef, actorIndex, allSkinsActive,
		useDefaultBoundingRadius, sharedBoneGroups, pBone, actorName);
}

std::shared_ptr<ParticleActor> ResourceManager::CreateParticleActor(std::string_view actorTag,
	const ActorDefinitionPtr& actorDef, int actorIndex, bool allSkinsActive, Bone* pBone)
{
	return std::make_shared<ParticleActor>(this, actorTag, actorDef, actorIndex, allSkinsActive, pBone);
}

std::shared_ptr<PointLight> ResourceManager::CreatePointLight(std::string_view name,
	const std::shared_ptr<LightDefinition>& lightDefinition, const glm::vec3& position, float radius)
{
	return std::make_shared<PointLight>(this, name, lightDefinition, position, radius);
}

std::shared_ptr<Terrain> ResourceManager::CreateTerrain() const
{
	return std::make_shared<Terrain>();
}

//-------------------------------------------------------------------------------------------------

bool ResourceManager::LoadTexture(Bitmap* bitmap, Archive* archive)
{
	if (!LoadBitmapData(bitmap, archive))
	{
		return false;
	}

	// Load the texture from the bitmap into a bgfx texture
	return bitmap->LoadTexture();
}

bool ResourceManager::LoadBitmapData(Bitmap* bitmap, Archive* archive)
{
	// Loads the raw bitmap data from the given archive for the provided bitmap.

	std::vector<char> buffer;
	if (archive != nullptr)
	{
		if (!archive->Get(bitmap->GetFileName(), buffer))
		{
			EQG_LOG_WARN("Failed to load '{}': Not found in archive '{}'", bitmap->GetFileName(), archive->GetFileName());
			return false;
		}
	}
	else
	{
		// Read the path from disk
		if (!ReadFile(bitmap->GetFileName(), buffer))
		{
			EQG_LOG_WARN("Failed to load '{}': Not found on disk", bitmap->GetFileName());
			return false;
		}
	}

	BufferReader reader(buffer);

	char* magic = reader.peek<char>();
	std::unique_ptr<char[]> rawDataCopy;
	uint32_t rawDataSize = 0;

	if (strncmp(magic, "BM", 2) == 0)
	{
		BufferReader readerCopy(reader);

		// Handle BMP
		BITMAPFILEHEADER* bmpHeader = readerCopy.read_ptr<BITMAPFILEHEADER>();
		BITMAPINFOHEADER* bmpInfo = readerCopy.read_ptr<BITMAPINFOHEADER>();

		bitmap->SetSize(bmpInfo->biWidth, abs(bmpInfo->biHeight));
		bitmap->SetSourceSize(bmpInfo->biWidth, bmpInfo->biHeight);

		//uint32_t dataOffset = bmpHeader->bfOffBits;
		uint32_t dataOffset = 0;
		char* rawData = buffer.data() + dataOffset;
		rawDataSize = (uint32_t)buffer.size() - dataOffset;
		rawDataCopy = std::make_unique<char[]>(rawDataSize);
		memcpy(rawDataCopy.get(), rawData, rawDataSize);
	}
	else if (strncmp(magic, "DDS", 3) == 0)
	{
		// Handle DDS

		BufferReader readerCopy(reader);

		readerCopy.skip<uint32_t>();

		// This could be a DDSURFACEDESC or a DDSURFACEDESC2. Read the smaller of the two types,
		// but ideally we should detect which version first.
		DDSURFACEDESC* ddsHeader = readerCopy.read_ptr<DDSURFACEDESC>();

		bitmap->SetSize(ddsHeader->dwWidth, ddsHeader->dwHeight);
		bitmap->SetSourceSize(ddsHeader->dwWidth, ddsHeader->dwHeight);

		// some of these EQ dds files report having zero mip maps, which causes the texture loader
		// to basically load nothing.
		if (ddsHeader->dwMipMapCount == 0)
			ddsHeader->dwMipMapCount = 1;

		//uint32_t dataOffset = sizeof(uint32_t) + ddsHeader->dwSize;
		uint32_t dataOffset = 0;
		char* rawData = buffer.data() + dataOffset;
		rawDataSize = (uint32_t)buffer.size() - dataOffset;
		rawDataCopy = std::make_unique<char[]>(rawDataSize);
		memcpy(rawDataCopy.get(), rawData, rawDataSize);
	}
	else
	{
		EQG_LOG_ERROR("Unrecognized bitmap format in {}", bitmap->GetFileName());
		return false;
	}

	if (rawDataCopy && rawDataSize)
	{
		bitmap->SetRawData(std::move(rawDataCopy), rawDataSize);
	}

	return true;
}

bool ResourceManager::ReadFile(std::string_view filePath, std::vector<char>& buffer)
{
	std::filesystem::path p = std::filesystem::path(m_dataPath) / filePath;
	std::error_code ec;

	if (!std::filesystem::exists(p, ec))
	{
		return false;
	}

	FILE* f = _fsopen(p.string().c_str(), "rb", _SH_DENYNO);
	if (f)
	{
		fseek(f, 0, SEEK_END);
		size_t sz = ftell(f);
		fseek(f, 0, SEEK_SET);

		buffer.resize(sz);
		size_t res = fread(&buffer[0], 1, sz, f);
		if (res != sz)
		{
			return false;
		}

		fclose(f);
		return true;
	}

	return false;
}

std::unique_ptr<uint8_t[]> ResourceManager::ReadFile(std::string_view filePath, uint32_t& size)
{
	size = 0;
	std::filesystem::path p = std::filesystem::path(m_dataPath) / filePath;
	std::error_code ec;
	if (!std::filesystem::exists(p, ec))
	{
		return nullptr;
	}

	FILE* f = _fsopen(p.string().c_str(), "rb", _SH_DENYNO);
	if (!f)
	{
		return nullptr;
	}

	fseek(f, 0, SEEK_END);
	size_t sz = ftell(f);
	fseek(f, 0, SEEK_SET);

	auto buffer = std::make_unique<uint8_t[]>(sz);
	size_t res = fread(buffer.get(), 1, sz, f);
	if (res != sz)
	{
		size = 0;
		return nullptr;
	}

	fclose(f);

	size = static_cast<uint32_t>(sz);
	return buffer;
}

void ResourceManager::SetConstantAmbientColor(uint32_t constantAmbientColor)
{
	m_constantAmbientColor = {
		static_cast<float>(constantAmbientColor & 0x00ff0000 >> 16) / 255,
		static_cast<float>(constantAmbientColor & 0x0000ff00 >>  8) / 255,
		static_cast<float>(constantAmbientColor & 0x000000ff >>  0) / 255,
		static_cast<float>(constantAmbientColor & 0xff000000 >> 24) / 255,
	};
}

} // namespace eqg
