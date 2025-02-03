#include "pch.h"
#include "eqg_resource_manager.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_animation.h"
#include "eqg_geometry.h"
#include "eqg_material.h"
#include "log_internal.h"

#include <ddraw.h> // for DDSURFACEDESC2

namespace eqg {

ResourceManager::ResourceManager(ResourceManager* parent)
	: m_parent(parent)
{
	// Prepopulate the sorted resource mapping
	for (int i = 0; i < (int)ResourceType::Max; ++i)
	{
		m_sortedResources[(ResourceType)i] = std::map<std::string_view, std::shared_ptr<Resource>>();
	}
}

ResourceManager::~ResourceManager()
{
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

//-------------------------------------------------------------------------------------------------

bool ResourceManager::LoadTexture(Bitmap* bitmap, Archive* archive)
{
	return LoadBitmapData(bitmap, archive);
}

bool ResourceManager::LoadBitmapData(Bitmap* bitmap, Archive* archive)
{
	// Loads the raw bitmap data from the given archive for the provided bitmap.

	std::vector<char> buffer;
	if (!archive->Get(bitmap->GetFileName(), buffer))
	{
		EQG_LOG_ERROR("Failed to load {} from {}", bitmap->GetFileName(), archive->GetFileName());
		return false;
	}

	BufferReader reader(buffer);

	char* magic = reader.peek<char>();
	std::unique_ptr<char[]> rawDataCopy;
	uint32_t rawDataSize = 0;

	if (strncmp(magic, "BM", 2) == 0)
	{
		// Handle BMP
		BITMAPFILEHEADER* bmpHeader = reader.read_ptr<BITMAPFILEHEADER>();
		BITMAPINFOHEADER* bmpInfo = reader.read_ptr<BITMAPINFOHEADER>();

		bitmap->SetSize(bmpInfo->biWidth, abs(bmpInfo->biHeight));
		bitmap->SetSourceSize(bmpInfo->biWidth, bmpInfo->biHeight);

		char* rawData = buffer.data() + bmpHeader->bfOffBits;
		rawDataSize = (uint32_t)buffer.size() - bmpHeader->bfOffBits;
		rawDataCopy = std::make_unique<char[]>(rawDataSize);
		memcpy(rawDataCopy.get(), rawData, rawDataSize);
	}
	else if (strncmp(magic, "DDS", 3) == 0)
	{
		// Handle DDS

		reader.skip<uint32_t>();

		// This could be a DDSURFACEDESC or a DDSURFACEDESC2. Read the smaller of the two types,
		// but ideally we should detect which version first.
		DDSURFACEDESC* ddsHeader = reader.read_ptr<DDSURFACEDESC>();

		bitmap->SetSize(ddsHeader->dwWidth, ddsHeader->dwHeight);
		bitmap->SetSourceSize(ddsHeader->dwWidth, ddsHeader->dwHeight);

		uint32_t dataOffset = sizeof(uint32_t) + ddsHeader->dwSize;
		char* rawData = buffer.data() + sizeof(uint32_t) + ddsHeader->dwSize;
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

} // namespace eqg
