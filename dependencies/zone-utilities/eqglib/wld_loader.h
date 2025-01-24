
#pragma once

#include "archive.h"
#include "wld_fragment.h"
#include "wld_structs.h"

#include <vector>
#include <stdint.h>
#include <string>

#ifdef GetObject
#undef GetObject
#endif

namespace eqg {

std::string_view decode_s3d_string(char* str, size_t len);

class HierarchicalModelDefinition;
class ResourceManager;
class SimpleModelDefinition;

class WLDLoader
{
public:
	WLDLoader(ResourceManager* resourceMgr);
	~WLDLoader();

	// Typically, the archive would be <zonename>.s3d and the file would be <zonename>.wld
	bool Init(Archive* archive, const std::string& fileName);

	// Parse everything
	bool ParseAll();

	// Parse only specified object. Used for loading models
	bool ParseObject(std::string_view tag);

	void Reset();

	uint32_t GetObjectIndexFromID(int nID, uint32_t currID = (uint32_t)-1);

	WLDFileObject& GetObjectFromID(int nID, uint32_t currID = (uint32_t)-1)
	{
		return m_objects[GetObjectIndexFromID(nID, currID)];
	}

	uint32_t GetNumObjects() const { return (uint32_t)m_objects.size(); }
	WLDFileObject& GetObject(uint32_t index) { return m_objects[index]; }

	std::vector<WLDFileObject>& GetObjectList() { return m_objects; }

	std::string_view GetString(int nID) const
	{
		return &m_stringPool[-nID];
	}

	bool IsOldVersion() const { return m_oldVersion; }
	bool IsValid() const { return m_valid; }

	const std::string& GetFileName() const { return m_fileName; }

	uint32_t GetConstantAmbient() const { return m_constantAmbient; }

	std::vector<ActorInstancePtr> m_actors;

private:
	bool ParseBitmapsAndMaterials(
		std::pair<uint32_t, uint32_t> bitmapRange,
		std::pair<uint32_t, uint32_t> materialRange,
		std::pair<uint32_t, uint32_t> materialPaletteRange,
		std::string_view npcTag);
	bool ParseBitmap(uint32_t objectIndex);
	bool ParseMaterial(uint32_t objectIndex);
	bool ParseMaterialPalette(uint32_t objectIndex);
	bool ParseBlitSprites(std::pair<uint32_t, uint32_t> blitSpriteRange, std::string_view npcTag);
	bool ParseBlitSprite(uint32_t objectIndex);
	bool ParseTextureDataDefinition(uint32_t objectIndex, STextureDataDefinition& dataDef);
	bool ParseTextureDataDefinitionFromSimpleSpriteDef(uint32_t objectIndex, STextureDataDefinition& dataDef);
	bool ParseTextureDataDefinitionFromSimpleSpriteInst(uint32_t objectIndex, STextureDataDefinition& dataDef);

	bool ParseTrack(uint32_t objectIndex);
	bool ParseSimpleModel(uint32_t objectIndex, std::shared_ptr<SimpleModelDefinition>& outModel);
	bool ParseHierarchicalModel(uint32_t objectIndex, std::shared_ptr<HierarchicalModelDefinition>& outModel);
	bool ParseDMSpriteDef2(uint32_t objectIndex, std::unique_ptr<SDMSpriteDef2WLDData>& outData);
	bool ParseActorDefinition(uint32_t objectIndex);
	bool ParseActorInstance(uint32_t objectIndex);
	bool ParseDMRGBTrack(uint32_t objectIndex, std::unique_ptr<SDMRGBTrackWLDData>& outData);
	bool ParseTerrain(uint32_t objectIndex);

	Archive*                   m_archive = nullptr;
	ResourceManager*           m_resourceMgr = nullptr;
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
	std::vector<WLDFileObject> m_objects;
	bool                       m_oldVersion = false;
	bool                       m_valid = false;
	uint32_t                   m_constantAmbient = 0;
};

} // namespace eqg
