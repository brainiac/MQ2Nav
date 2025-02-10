
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

class ActorInstance;
class HierarchicalModelDefinition;
class ResourceManager;
class SimpleModelDefinition;
class Terrain;

class WLDLoader
{
public:
	// TODO: loading flags:
	// - luclin animations

	WLDLoader(ResourceManager* resourceMgr);
	~WLDLoader();

	// Typically, the archive would be <zonename>.s3d and the file would be <zonename>.wld
	bool Init(Archive* archive, const std::string& fileName, int loadFlags = 0);

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

	std::vector<std::shared_ptr<ActorInstance>> m_actors;

private:
	bool ParseBitmapsAndMaterials(
		std::pair<uint32_t, uint32_t> bitmapRange,
		std::pair<uint32_t, uint32_t> materialRange,
		std::pair<uint32_t, uint32_t> materialPaletteRange,
		std::string_view npcTag = {});
	bool ParseBitmap(uint32_t objectIndex);
	bool ParseMaterial(uint32_t objectIndex);
	bool ParseMaterialPalette(uint32_t objectIndex);
	bool ParseBlitSprites(std::pair<uint32_t, uint32_t> blitSpriteRange, std::string_view npcTag = {});
	bool ParseBlitSprite(uint32_t objectIndex);
	bool ParseTextureDataDefinition(uint32_t objectIndex, STextureDataDefinition& dataDef);
	bool ParseTextureDataDefinitionFromSimpleSpriteDef(uint32_t objectIndex, STextureDataDefinition& dataDef);
	bool ParseTextureDataDefinitionFromSimpleSpriteInst(uint32_t objectIndex, STextureDataDefinition& dataDef);
	bool ParseParticleClouds(std::pair<uint32_t, uint32_t> pcloudRange, std::string_view npcTag = {});
	bool ParseParticleCloud(uint32_t objectIndex, float scale);
	bool ParseTrack(uint32_t objectIndex);
	bool ParseAnimations(std::pair<uint32_t, uint32_t> trackRange, std::string_view npcTag = {});
	bool ParseAnimation(uint32_t objectIndex, std::string_view animTag, uint32_t& nextObjectIndex);
	bool ParseSimpleModel(uint32_t objectIndex, std::shared_ptr<SimpleModelDefinition>& outModel);
	bool ParseHierarchicalModel(uint32_t objectIndex, std::shared_ptr<HierarchicalModelDefinition>& outModel);
	bool ParseDMSpriteDef2(uint32_t objectIndex, std::unique_ptr<SDMSpriteDef2WLDData>& outData);
	bool ParseActorDefinition(uint32_t objectIndex);
	bool ParseActorInstance(uint32_t objectIndex);
	bool ParseDMRGBTrack(uint32_t objectIndex, std::unique_ptr<SDMRGBTrackWLDData>& outData);
	bool ParseTerrain(std::pair<uint32_t, uint32_t> regionRange, std::pair<uint32_t, uint32_t> zoneRange,
		int worldTreeIndex, int constantAmbientIndex);
	bool ParseRegion(uint32_t objectIndex, STerrainWLDData& terrainData);
	bool ParseArea(uint32_t objectIndex, uint32_t areaNum, STerrainWLDData& terrain);
	bool ParseWorldTree(uint32_t objectIndex, STerrainWLDData& terrain);
	bool ParseConstantAmbient(uint32_t objectIndex, STerrainWLDData& terrain);

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
	bool                       m_luclinAnims = false;
	bool                       m_itemAnims = true;
	bool                       m_newAnims = false;
	bool                       m_skipSocials = false;
	bool                       m_optimizeAnims = false;
	uint32_t                   m_constantAmbient = 0;
};

} // namespace eqg
