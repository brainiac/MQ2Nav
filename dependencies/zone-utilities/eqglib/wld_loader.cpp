
#include "pch.h"
#include "wld_loader.h"

#include <algorithm>

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_animation.h"
#include "eqg_geometry.h"
#include "eqg_light.h"
#include "eqg_material.h"
#include "eqg_particles.h"
#include "eqg_resource_manager.h"
#include "eqg_terrain.h"
#include "log_internal.h"
#include "wld_types.h"

#include <ranges>

namespace eqg {

constexpr float EQ_TO_RAD = glm::pi<float>() / 256.0f;

static const uint8_t decoder_array[] = { 0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A };

std::string_view decode_s3d_string(char* str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		str[i] ^= decoder_array[i % 8];
	}

	return std::string_view(str, len - 1);
}

std::string_view ObjectTypeToString(WLDObjectType type)
{
	switch (type)
	{
	case WLD_NONE: return "NONE";
	case WLD_OBJ_DEFAULTPALETTEFILE_TYPE: return "DEFAULTPALETTEFILE";
	case WLD_OBJ_WORLD_USERDATA_TYPE: return "WORLD_USERDATA";
	case WLD_OBJ_BMINFO_TYPE: return "BMINFO";
	case WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE: return "SIMPLESPRITEDEFINITION";
	case WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE: return "SIMPLESPRITEINSTANCE";
	case WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE: return "HIERARCHICALSPRITEDEFINITION";
	case WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE: return "HIERARCHICALSPRITEINSTANCE";
	case WLD_OBJ_TRACKDEFINITION_TYPE: return "TRACKDEFINITION";
	case WLD_OBJ_TRACKINSTANCE_TYPE: return "TRACKINSTANCE";
	case WLD_OBJ_ACTORDEFINITION_TYPE: return "ACTORDEFINITION";
	case WLD_OBJ_ACTORINSTANCE_TYPE: return "ACTORINSTANCE";
	case WLD_OBJ_LIGHTDEFINITION_TYPE: return "LIGHTDEFINITION";
	case WLD_OBJ_LIGHTINSTANCE_TYPE: return "LIGHTINSTANCE";
	case WLD_OBJ_WORLDTREE_TYPE: return "WORLDTREE";
	case WLD_OBJ_REGION_TYPE: return "REGION";
	case WLD_OBJ_BLITSPRITEDEFINITION_TYPE: return "BLITSPRITEDEFINITION";
	case WLD_OBJ_BLITSPRITEINSTANCE_TYPE: return "BLITSPRITEINSTANCE";
	case WLD_OBJ_POINTLIGHT_TYPE: return "POINTLIGHT";
	case WLD_OBJ_ZONE_TYPE: return "ZONE";
	case WLD_OBJ_DMSPRITEDEFINITION_TYPE: return "DMSPRITEDEFINITION";
	case WLD_OBJ_DMSPRITEINSTANCE_TYPE: return "DMSPRITEINSTANCE";
	case WLD_OBJ_MATERIALDEFINITION_TYPE: return "MATERIALDEFINITION";
	case WLD_OBJ_MATERIALPALETTE_TYPE: return "MATERIALPALETTE";
	case WLD_OBJ_DMRGBTRACKDEFINITION_TYPE: return "DMRGBTRACKDEFINITION";
	case WLD_OBJ_DMRGBTRACKINSTANCE_TYPE: return "DMRGBTRACKINSTANCE";
	case WLD_OBJ_PCLOUDDEFINITION_TYPE: return "PCLOUDDEFINITION";
	case WLD_OBJ_CONSTANTAMBIENT_TYPE: return "CONSTANTAMBIENT";
	case WLD_OBJ_DMSPRITEDEFINITION2_TYPE: return "DMSPRITEDEFINITION2";
	case WLD_OBJ_LAST_TYPE: return "LAST";
	default: return "UNKNOWN";
	}
}

WLDLoader::WLDLoader(ResourceManager* resourceMgr)
	: m_resourceMgr(resourceMgr)
{
}

WLDLoader::~WLDLoader()
{
	Reset();
}

void WLDLoader::Reset()
{
	if (m_stringPool)
	{
		delete[] m_stringPool;
	}

	for (auto& obj : m_objects)
	{
		if (obj.parsed_data != nullptr)
		{
			delete obj.parsed_data;
			obj.parsed_data = nullptr;
		}
	}

	m_objects.clear();
}

bool WLDLoader::Init(Archive* archive, const std::string& wld_name, int loadFlags)
{
	if (archive == nullptr || wld_name.empty())
	{
		return false;
	}

	m_archive = archive;
	m_fileName = wld_name;

	if (loadFlags & LoadFlag_ItemAnims)
		m_itemAnims = true;
	if (loadFlags & LoadFlag_LuclinAnims)
		m_luclinAnims = true;
	if (loadFlags & LoadFlag_SkipSocials)
		m_skipSocials = true;
	if (loadFlags & LoadFlag_SkipOldAnims)
		m_newAnims = true;
	if (loadFlags & LoadFlag_OptimizeAnims)
		m_optimizeAnims = true;

	if (!archive->Exists(wld_name))
	{
		EQG_LOG_TRACE("wld file {} does not exist in archive {}.", wld_name, archive->GetFileName());

		return false; // no error log if file doesn't exist.
	}

	// This will whole the entire contents of the .wld file
	m_wldFileContents = m_archive->Get(wld_name, m_wldFileSize);
	if (!m_wldFileContents)
	{
		EQG_LOG_WARN("Unable to open wld file {}.", wld_name);
		return false;
	}

	BufferReader reader(m_wldFileContents, m_wldFileSize);

	SWLDHeader header;
	if (!reader.read(header))
	{
		EQG_LOG_WARN("Unable to read wld header.");
		return false;
	}

	if (header.magic != '\x02\x3dPT')
	{
		EQG_LOG_ERROR("Header magic of {:x} did not match expected", header.magic);
		return false;
	}

	m_version = header.version;
	m_numObjects = header.fragments;
	m_numRegions = header.unk1;
	m_maxObjectSize = header.unk2;
	m_stringPoolSize = header.hash_length;
	m_numStrings = header.unk3;
	m_oldVersion = header.version == 0x00015500;

	if (m_stringPoolSize)
	{
		m_stringPool = new char[m_stringPoolSize];
		if (!reader.read(m_stringPool, m_stringPoolSize))
		{
			EQG_LOG_WARN("Unable to read string pool.");
			return false;
		}

		decode_s3d_string(m_stringPool, m_stringPoolSize);
	}

	EQG_LOG_TRACE("Parsing WLD objects.");

	m_objects.resize(m_numObjects + 1);
	memset(m_objects.data(), 0, sizeof(WLDFileObject) * (m_numObjects + 1));

	uint32_t index = 0;
	for (; index < m_numObjects; ++index)
	{
		uint32_t objectSize, type;
		if (!reader.read(objectSize))
		{
			EQG_LOG_WARN("Unable to read object size ({}).", index);
			return false;
		}

		if (!reader.read(type))
		{
			EQG_LOG_WARN("Unable to read object type ({}).", index);
			return false;
		}

		auto& obj = m_objects[index + 1];
		obj.size = objectSize;
		obj.type = static_cast<WLDObjectType>(type);

		// Read the raw data from the buffer
		obj.data = m_wldFileContents.get() + reader.pos();
		if (!reader.skip(objectSize))
		{
			EQG_LOG_WARN("Unable to read object data ({}).", index);
			return false;
		}

		// Read the tag
		if (type == WLD_OBJ_DEFAULTPALETTEFILE_TYPE
			|| type == WLD_OBJ_WORLD_USERDATA_TYPE
			|| type == WLD_OBJ_CONSTANTAMBIENT_TYPE)
		{
			// These types do not have a tag.

			if (type == WLD_OBJ_CONSTANTAMBIENT_TYPE)
			{
				// This doesn't adhere to the typical structure. It is very simple so just read it
				// directly here.
				m_constantAmbient = *reinterpret_cast<uint32_t*>(obj.data);
			}
		}
		else
		{
			BufferReader tagBuffer(obj.data, obj.size);

			int tagID;
			if (!tagBuffer.read(tagID))
			{
				EQG_LOG_WARN("Unable to read tag ID ({}).", index);
				return false;
			}

			if (tagID < 0)
			{
				obj.tag = &m_stringPool[-tagID];
			}
		}
	}

	if (index < m_numObjects)
	{
		EQG_LOG_WARN("Failed to read all objects ({} < {}).", index, m_numObjects);
		return false;
	}

	m_valid = true;
	return true;
}

uint32_t WLDLoader::GetObjectIndexFromID(int nID, uint32_t currID)
{
	if (nID >= 0)
	{
		// positive IDs are object IDs.
		return static_cast<uint32_t>(nID);
	}

	// negative IDs mean string tag ids.
	std::string_view tag = GetString(nID);

	// if a current Id is provided, scan backwards from that index.
	if (currID != (uint32_t)-1)
	{
		for (uint32_t i = currID; i > 0; --i)
		{
			if (m_objects[i].tag == tag)
			{
				return i;
			}
		}

		return 0;
	}

	// otherwise, scan the entire list.
	for (uint32_t i = 1; i <= m_numObjects; ++i)
	{
		if (m_objects[i].tag == tag)
		{
			return i;
		}
	}

	return 0;
}

class ObjectCountHelper
{
public:
	struct ObjectTypeCounts
	{
		uint32_t count = 0;
		uint32_t firstIndex = 0;
		uint32_t lastIndex = 0;

		std::pair<uint32_t, uint32_t> range() const
		{
			return std::make_pair(firstIndex, lastIndex + 1);
		}
	};

	ObjectCountHelper(std::vector<WLDFileObject>& objects, uint32_t numObjects)
		: objectCounts(WLD_OBJ_LAST_TYPE)
	{
		// Count the number of each object type and the ranges of their indices
		for (uint32_t i = 1; i <= numObjects; ++i)
		{
			WLDFileObject& obj = objects[i];

			objectCounts[obj.type].count++;
			objectCounts[obj.type].lastIndex = i;

			if (objectCounts[obj.type].firstIndex == 0)
			{
				objectCounts[obj.type].firstIndex = i;
			}
		}
	}

	void LogObjectCounts() const
	{
		for (uint32_t type = 1; type < (uint32_t)objectCounts.size(); ++type)
		{
			uint32_t count = objectCounts[type].count;
			if (count == 0)
				continue;

			std::string_view typeName = ObjectTypeToString(static_cast<WLDObjectType>(type));
			EQG_LOG_TRACE("- {} ({}) = {}", typeName, type, count);
		}
	}

	uint32_t count(WLDObjectType type) const { return objectCounts[type].count; }
	std::pair<uint32_t, uint32_t> range(WLDObjectType type) const { return objectCounts[type].range(); }

private:
	std::vector<ObjectTypeCounts> objectCounts;
};

static bool IsAnimationCode(std::string_view code)
{
	return code.length() >= 3 && ::isalpha(code[0]) && ::isdigit(code[1]) && ::isdigit(code[2]);
}

bool WLDLoader::ParseAll()
{
	/// Actual loading sequence:
	///
	/// ParseBitmapsAndMaterials
	///     ParseBitmap                                    WLD_OBJ_BMINFO_TYPE
	///         CEQGBitMap::InitFromWLDData -> Add to resource manager
	///     ParseMaterial                                  WLD_OBJ_MATERIALDEFINITION_TYPE
	///         read WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE
	///             read WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE
	///                 Create material with InitFromWLDData -> Add to resource manager
	///     ParseMaterialPalette                           WLD_OBJ_MATERIALPALETTE_TYPE
	///         create palette with InitFromWLDData -> Add to resource manager
	///
	/// ParseBlitSprites
	///
	/// ParsePClouds                                       WLD_OBJ_PCLOUDDEFINITION_TYPE
	///     ParsePCloudDef
	///
	/// ParseTrack                                         WLD_OBJ_TRACKINSTANCE_TYPE
	///     Create STrack from instance with transform from definition. store on WLD object
	///
	/// ParseAnimations
	///     Find tracks that are animations:
	///         ParseAnimation
	///             Create animation with InitFromWLDData -> Add to resource manager
	///
	/// ParseActorDefinitions                              WLD_OBJ_ACTORDEFINITION_TYPE
	///     sprite index -> model type:
	///         ParseSimpleModel                           WLD_OBJ_DMSPRITEINSTANCE_TYPE
	///             ParseDMSpriteDef2                      WLD_OBJ_DMSPRITEDEFINITION2_TYPE
	///             create simple model definition with InitFromWLDData -> Add to resource manager
	///         ParseHierarchicalModel                     WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE
	///             read definition data                   WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE
	///             read tracks
	///             recurse (its hierarchical, duh)
	///                 ParseSimpleModel                   WLD_OBJ_DMSPRITEINSTANCE_TYPE
	///                 ParseHierarchicalModel             WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE
	///                 ParsePCloudDef                     WLD_OBJ_PCLOUDDEFINITION_TYPE
	///             parse skins
	///                 ParseDMSpriteDef2
	///             create hierarchical model definition with InitFromWLDData -> Add to resource manager
	///     create actor definition -> Add to resource manager
	///
	/// ParseActorInstances                                WLD_OBJ_ACTORINSTANCE_TYPE
	///     ParseDMRGBTrack                                WLD_OBJ_DMRGBTRACKINSTANCE_TYPE
	///         parse definition                           WLD_OBJ_DMRGBTRACKDEFINITION_TYPE
	///            store on actor instance
	///     get definition from resource manager
	///     instance tag: ZoneActor_%05d
	///     create hierarchical actor or simple actor, store on object
	///
	/// ParsePClouds again?
	///
	/// ParseTerrain
	///     find all regions and parse them:
	///         ParseRegion                                WLD_OBJ_REGION_TYPE
	///             ParseDMSpriteDef2                      WLD_OBJ_DMSPRITEDEFINITION2_TYPE
	///         gather counts of all region geometry, send to CTerrain->PreInit
	///     find all areas
	///         ParseArea                                  WLD_OBJ_ZONE_TYPE
	///             add areas to terrain
	///     add regions to terrain
	///     ParseWorldTree                                 WLD_OBJ_WORLDTREE_TYPE
	///     add world tree to terrain
	///     ParseConstantAmbient                           WLD_OBJ_CONSTANTAMBIENT_TYPE
	///         Set constant ambient to scene
	///
	/// ParseLights                                        WLD_OBJ_POINTLIGHT_TYPE
	///     get light instance                             WLD_OBJ_LIGHTINSTANCE_TYPE
	///         get light definition                       WLD_OBJ_LIGHTDEFINITION_TYPE
	///             add light definition to resource manager
	///         add light to scene
	///
	
	ObjectCountHelper objectCounts(m_objects, m_numObjects);

	EQG_LOG_DEBUG("Loading {}", m_fileName);
	objectCounts.LogObjectCounts();

	if (!ParseBitmapsAndMaterials(
		objectCounts.range(WLD_OBJ_BMINFO_TYPE),
		objectCounts.range(WLD_OBJ_MATERIALDEFINITION_TYPE),
		objectCounts.range(WLD_OBJ_MATERIALPALETTE_TYPE)))
	{
		return false;
	}

	if (!ParseBlitSprites(objectCounts.range(WLD_OBJ_BLITSPRITEDEFINITION_TYPE)))
		return false;

	if (!ParseParticleClouds(objectCounts.range(WLD_OBJ_PCLOUDDEFINITION_TYPE)))
		return false;

	bool hasAnimationCode = false;
	
	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			if (!ParseTrack(i))
			{
				EQG_LOG_ERROR("Failed to track instance: {}", m_objects[i].tag);
				return false;
			}

			if (!hasAnimationCode && IsAnimationCode(m_objects[i].tag))
				hasAnimationCode = true;
		}
	}

	if (hasAnimationCode)
	{
		if (!ParseAnimations(objectCounts.range(WLD_OBJ_TRACKINSTANCE_TYPE)))
			return false;
	}

	int constantAmbientIndex = 0;
	int worldTreeIndex = 0;

	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_CONSTANTAMBIENT_TYPE)
			constantAmbientIndex = i;
		if (m_objects[i].type == WLD_OBJ_WORLDTREE_TYPE)
			worldTreeIndex = i;

		else if (m_objects[i].type == WLD_OBJ_ACTORDEFINITION_TYPE)
		{
			if (!ParseActorDefinition(i))
			{
				EQG_LOG_ERROR("Failed to parse actor definition: {} ({})", m_objects[i].tag, i);
				return false;
			}
		}
	}

	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_ACTORINSTANCE_TYPE)
		{
			if (!ParseActorInstance(i))
			{
				EQG_LOG_WARN("Failed to parse actor instance: {} ({})", m_objects[i].tag, i);
			}
		}
	}

	if (objectCounts.count(WLD_OBJ_REGION_TYPE) != 0)
	{
		if (!ParseTerrain(objectCounts.range(WLD_OBJ_REGION_TYPE), objectCounts.range(WLD_OBJ_ZONE_TYPE), worldTreeIndex, constantAmbientIndex))
		{
			EQG_LOG_WARN("Failed to parse terrain");
			return false;
		}
	}

	auto lightRange = objectCounts.range(WLD_OBJ_POINTLIGHT_TYPE);
	for (uint32_t i = lightRange.first; i < lightRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_POINTLIGHT_TYPE)
		{
			if (!ParseLight(i))
			{
				EQG_LOG_WARN("Failed to parse light: {} ({})", m_objects[i].tag, i);
				return false;
			}
		}
	}

	return true;
}

bool WLDLoader::ParseObject(std::string_view tag)
{
	return false;
}

bool WLDLoader::ParseBitmapsAndMaterials(
	std::pair<uint32_t, uint32_t> bitmapRange,
	std::pair<uint32_t, uint32_t> materialRange,
	std::pair<uint32_t, uint32_t> materialPaletteRange,
	std::string_view npcTag)
{
	for (uint32_t i = bitmapRange.first; i < bitmapRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_BMINFO_TYPE)
		{
			// If NPC tag is provided, only process if the first three characters match.
			if (npcTag.empty() || ci_starts_with(m_objects[i].tag, npcTag.substr(0, 3)))
			{
				if (!ParseBitmap(i))
				{
					EQG_LOG_ERROR("Failed to parse bitmap: {}", m_objects[i].tag);
					return false;
				}
			}
		}
	}

	for (uint32_t i = materialRange.first; i < materialRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_MATERIALDEFINITION_TYPE)
		{
			// If NPC tag is provided, only process if the first three characters match.
			if (npcTag.empty() || ci_starts_with(m_objects[i].tag, npcTag.substr(0, 3)))
			{
				if (!ParseMaterial(i))
				{
					EQG_LOG_ERROR("Failed to parse material: {}", m_objects[i].tag);
					return false;
				}
			}
		}
	}

	for (uint32_t i = materialPaletteRange.first; i < materialPaletteRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_MATERIALPALETTE_TYPE)
		{
			// If NPC tag is provided, only process if the first three characters match.
			if (npcTag.empty() || ci_starts_with(m_objects[i].tag, npcTag.substr(0, 3)))
			{
				if (!ParseMaterialPalette(i))
				{
					EQG_LOG_ERROR("Failed to parse material palette: {}", m_objects[i].tag);
					return false;
				}
			}
		}
	}

	return true;
}

bool WLDLoader::ParseBitmap(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_BMINFO_TYPE)
	{
		return false;
	}

	EQG_LOG_TRACE("Parsing bitmap: {} ({})", wldObj.tag, objectIndex);

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_BMINFO* pWLDBMInfo = reader.read_ptr<WLD_OBJ_BMINFO>();
	constexpr size_t MAX_MIP_LEVELS = 16;
	std::string_view fileNames[MAX_MIP_LEVELS];
	float scale = 1.0f;
	EBitmapType type = eBitmapTypeNormal;

	uint16_t fileNameLength;
	if (!reader.read(fileNameLength))
		return false;
	char* fileName;
	if (!reader.read_array(fileName, fileNameLength))
		return false;

	fileNames[0] = decode_s3d_string(fileName, fileNameLength);
	bool hasPaletteMain = false;

	if (pWLDBMInfo->num_mip_levels > 0)
	{
		if (pWLDBMInfo->num_mip_levels < MAX_MIP_LEVELS)
		{
			for (uint32_t i = 1; i <= pWLDBMInfo->num_mip_levels; ++i)
			{
				if (!reader.read(fileNameLength))
					return false;
				if (!reader.read_array(fileName, fileNameLength))
					return false;

				fileNames[i] = decode_s3d_string(fileName, fileNameLength);
			}

			if (pWLDBMInfo->num_mip_levels == 1)
			{
				if (auto idx = fileNames[1].find("_LAYER"); idx != std::string_view::npos)
				{
					fileNames[1] = fileNames[1].substr(0, idx);
					type = eBitmapTypeLayer;
				}
				else if (idx = fileNames[1].find("_DETAIL"); idx != std::string_view::npos)
				{
					// Formatted like _DETAIL_<num>
					std::string_view detailNum = fileNames[1].substr(idx + 8);
					scale = str_to_float(detailNum, 1.0f);

					fileNames[1] = fileNames[1].substr(0, idx);

					type = eBitmapTypeSingleDetail;
				}
			}
			else if (pWLDBMInfo->num_mip_levels > 1)
			{
				type = eBitmapTypePaletteDetailMain;
				hasPaletteMain = true;
			}
		}
		else
		{
			EQG_LOG_ERROR("Too many mip levels in bitmap {}!", wldObj.tag);
			return false;
		}
	}

	auto parsedBitmaps = std::make_unique<ParsedBMInfo>();
	parsedBitmaps->bitmaps.resize(pWLDBMInfo->num_mip_levels + 1);

	for (uint32_t bitmapIndex = 0; bitmapIndex < pWLDBMInfo->num_mip_levels + 1; ++bitmapIndex)
	{
		SBitmapWLDData wldData;
		wldData.fileName = fileNames[bitmapIndex];
		wldData.detailScale = scale;
		wldData.grassDensity = 0;
		wldData.createTexture = true;
		wldData.objectIndex = (uint32_t)-1;

		if (hasPaletteMain)
		{
			wldData.objectIndex = objectIndex;

			if (bitmapIndex == 1)
			{
				type = eBitmapTypePaletteDetailPalette;
				wldData.createTexture = false;
			}
			else if (bitmapIndex > 1)
			{
				type = eBitmapTypePaletteDetailDetail;

				auto parts = split_view(fileNames[bitmapIndex], ',');

				// Blindly assumes that this has 4 parts, because the eqg code didn't check for it...
				wldData.detailScale = str_to_float(parts[1], 1.0f);
				wldData.grassDensity = str_to_int(parts[2], 0);
				wldData.fileName = parts[3];
				fileNames[bitmapIndex] = parts[3];
			}
		}

		// Have we already made a bitmap for this one?
		if (auto existingBM = m_resourceMgr->Get<Bitmap>(wldData.fileName))
		{
			parsedBitmaps->bitmaps[bitmapIndex] = existingBM;
			continue;
		}

		std::shared_ptr<Bitmap> newBitmap = m_resourceMgr->CreateBitmap();
		if (!newBitmap->InitFromWLDData(&wldData, m_archive))
		{
			EQG_LOG_ERROR("Failed to create bitmap {}!", wldData.fileName);
			return false;
		}

		newBitmap->SetType(type);

		if (!m_resourceMgr->Add(wldData.fileName, newBitmap))
		{
			EQG_LOG_ERROR("Failed to add bitmap {} to resource manager!", wldData.fileName);
			return false;
		}

		parsedBitmaps->bitmaps[bitmapIndex] = newBitmap;
	}

	wldObj.parsed_data = parsedBitmaps.release();
	return true;
}

bool WLDLoader::ParseMaterial(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_MATERIALDEFINITION_TYPE)
	{
		return false;
	}

	// Check if it already exists
	if (m_resourceMgr->Contains(wldObj.tag, ResourceType::Material))
	{
		return true;
	}

	EQG_LOG_TRACE("Parsing material: {} ({})", wldObj.tag, objectIndex);

	BufferReader reader(wldObj.data, wldObj.size);
	auto* pWLDMaterialDef = reader.read_ptr<WLD_OBJ_MATERIALDEFINITION>();

	WLDFileObject& simpleSpriteInstanceObj = GetObjectFromID(pWLDMaterialDef->sprite_or_bminfo, objectIndex);
	WLD_OBJ_SIMPLESPRITEINSTANCE* pWLDSimpleSpriteInst = (WLD_OBJ_SIMPLESPRITEINSTANCE*)simpleSpriteInstanceObj.data;

	ParsedSimpleSpriteDef* pParsedSpriteDef = nullptr;
	std::shared_ptr<ParsedBMInfo> pParsedBMInfoForPalette;

	if (pWLDSimpleSpriteInst != nullptr)
	{
		if (simpleSpriteInstanceObj.type != WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE)
		{
			EQG_LOG_ERROR("Material's sprite_or_bminfo is not a SimpleSpriteInstance: {}", wldObj.tag);
			return false;
		}

		WLDFileObject& simpleSpriteDefinitionObj = GetObjectFromID(pWLDSimpleSpriteInst->definition_id, objectIndex);
		if (simpleSpriteDefinitionObj.type != WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE)
		{
			EQG_LOG_ERROR("SimpleSpriteInstance's definition isn't a SimpleSpriteDefinition!", simpleSpriteInstanceObj.tag);
			return false;
		}

		BufferReader spriteDefReader(simpleSpriteDefinitionObj.data, simpleSpriteDefinitionObj.size);

		// Only need to do this once.
		if (simpleSpriteDefinitionObj.parsed_data == nullptr)
		{
			// Parse the data for the SIMPLESPRITEDEFINITION.
			WLD_OBJ_SIMPLESPRITEDEFINITION* pWLDSpriteDef = spriteDefReader.read_ptr<WLD_OBJ_SIMPLESPRITEDEFINITION>();
			pParsedSpriteDef = new ParsedSimpleSpriteDef();
			pParsedSpriteDef->definition = pWLDSpriteDef;

			if (pWLDSpriteDef->flags & WLD_OBJ_SPROPT_HAVECURRENTFRAME)
				spriteDefReader.skip<uint32_t>();
			if (pWLDSpriteDef->flags & WLD_OBJ_SPROPT_HAVESLEEP)
				spriteDefReader.skip<uint32_t>();

			// We are reading from an array of Ids and turning them into bitmap references. We already should have
			// parsed all the bitmaps, so we can just look them up here.
			for (uint32_t i = 0; i < pWLDSpriteDef->num_frames; ++i)
			{
				std::shared_ptr<ParsedBMInfo> pParsedBMInfo;

				int bmID = spriteDefReader.read<int>();
				if (bmID < 0)
				{
					// An object that we can look up by name.
					auto bitmap = m_resourceMgr->Get<Bitmap>(GetString(bmID));
					if (!bitmap)
					{
						EQG_LOG_ERROR("Failed to find bitmap {} (id={})", GetString(bmID), bmID);
						delete pParsedSpriteDef;
						return false;
					}

					pParsedBMInfo = std::make_shared<ParsedBMInfo>();
					pParsedBMInfo->bitmaps.push_back(bitmap);
				}
				else
				{
					// An object that we can look up by ID.
					WLDFileObject& bmObj = m_objects[bmID];
					if (bmObj.type != WLD_OBJ_BMINFO_TYPE)
					{
						EQG_LOG_ERROR("Bitmap {} (id: {}) is not a BMINFO object!?", bmObj.tag, bmID);
						delete pParsedSpriteDef;
						return false;
					}

					// Copy into a new ParsedBMInfo object that our ParsedSimpleSpriteDef owns.
					pParsedBMInfo = std::make_shared<ParsedBMInfo>(*(ParsedBMInfo*)bmObj.parsed_data);
				}

				// Add this new ParsedBMInfo to our ParsedSpriteDef. If this is the main palette then also save that.
				pParsedSpriteDef->parsedBitmaps.push_back(pParsedBMInfo);
				if (pParsedBMInfo->bitmaps[0]->GetType() == eBitmapTypePaletteDetailMain
					&& pParsedBMInfo->bitmaps[0]->GetObjectIndex() != (uint32_t)-1)
				{
					pParsedBMInfoForPalette = pParsedBMInfo;
				}
			}

			simpleSpriteDefinitionObj.parsed_data = pParsedSpriteDef;
		}
		else
		{
			pParsedSpriteDef = (ParsedSimpleSpriteDef*)simpleSpriteDefinitionObj.parsed_data;
		}
	}

	// create the material
	auto newMaterial = std::make_shared<Material>();
	if (!newMaterial->InitFromWLDData(wldObj.tag, pWLDMaterialDef, pWLDSimpleSpriteInst, pParsedSpriteDef,
		pParsedBMInfoForPalette.get()))
	{
		EQG_LOG_ERROR("Failed to create material {}!", wldObj.tag);
		return false;
	}

	if (!m_resourceMgr->Add(wldObj.tag, newMaterial))
	{
		EQG_LOG_ERROR("Failed to add new material to resource manager: {}", wldObj.tag);
		return false;
	}

	return true;
}

bool WLDLoader::ParseMaterialPalette(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_MATERIALPALETTE_TYPE)
	{
		return false;
	}

	ParsedMaterialPalette* parsedPalette;

	// Check if it already exists
	if (auto existingMP = m_resourceMgr->Get<MaterialPalette>(wldObj.tag))
	{
		// re-create a ParsedPalette from this existing object.
		parsedPalette = new ParsedMaterialPalette();
		parsedPalette->palette = existingMP;
		wldObj.parsed_data = parsedPalette;

		return true;
	}

	EQG_LOG_TRACE("Parsing material palette: {} ({})", wldObj.tag, objectIndex);

	BufferReader reader(wldObj.data, wldObj.size);

	if (wldObj.parsed_data == nullptr)
	{
		parsedPalette = new ParsedMaterialPalette();

		parsedPalette->matPalette = reader.read_ptr<WLD_OBJ_MATERIALPALETTE>();
		parsedPalette->materials.resize(parsedPalette->matPalette->num_entries);

		for (size_t i = 0; i < parsedPalette->materials.size(); ++i)
		{
			int materialID = reader.read<int>();

			if (materialID < 0)
			{
				auto material = m_resourceMgr->Get<Material>(GetString(materialID));
				if (!material)
				{
					EQG_LOG_ERROR("Failed to find material {} (id={})", GetString(materialID), materialID);
					delete parsedPalette;
					return false;
				}
				parsedPalette->materials[i] = material;
			}
			else
			{
				WLDFileObject& materialObj = m_objects[materialID];
				if (materialObj.type != WLD_OBJ_MATERIALDEFINITION_TYPE)
				{
					EQG_LOG_ERROR("Material {} (id: {}) is not a MATERIALDEFINITION object!?", materialObj.tag, materialID);
					delete parsedPalette;
					return false;
				}

				auto material = m_resourceMgr->Get<Material>(materialObj.tag);
				if (!material)
				{
					EQG_LOG_ERROR("Failed to find material {} (id={})", materialObj.tag, materialID);
					delete parsedPalette;
					return false;
				}

				parsedPalette->materials[i] = material;
			}
		}

		wldObj.parsed_data = parsedPalette;
	}
	else
	{
		parsedPalette = (ParsedMaterialPalette*)wldObj.parsed_data;
	}

	std::shared_ptr<MaterialPalette> palette = std::make_shared<MaterialPalette>();

	if (!palette->InitFromWLDData(wldObj.tag, parsedPalette))
	{
		EQG_LOG_ERROR("Failed to create material palette {}!", wldObj.tag);
		return false;
	}

	if (!m_resourceMgr->Add(wldObj.tag, palette))
	{
		EQG_LOG_ERROR("Failed to add new material palette to resource manager: {}", wldObj.tag);
		return false;
	}

	parsedPalette->palette = palette;

	return true;
}

bool WLDLoader::ParseBlitSprites(std::pair<uint32_t, uint32_t> blitSpriteRange, std::string_view npcTag)
{
	for (uint32_t i = blitSpriteRange.first; i < blitSpriteRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_BLITSPRITEDEFINITION_TYPE)
		{
			// If NPC tag is provided, only process if the first three characters match.
			if (npcTag.empty() || ci_starts_with(m_objects[i].tag, npcTag.substr(0, 3)))
			{
				if (!ParseBlitSprite(i))
				{
					EQG_LOG_WARN("Failed to parse blitsprite: {}", m_objects[i].tag);
					//return false;
				}
			}
		}
	}

	return true;
}

bool WLDLoader::ParseBlitSprite(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (m_resourceMgr->Contains(wldObj.tag, ResourceType::BlitSpriteDefinition))
		return true;

	EQG_LOG_TRACE("Parsing blitsprite: {} ({})", wldObj.tag, objectIndex);
	WLD_OBJ_BLITSPRITEDEFINITION* pBlitSpriteDef = (WLD_OBJ_BLITSPRITEDEFINITION*)wldObj.data;

	STextureDataDefinition textureDataDef;

	if (!ParseTextureDataDefinition(GetObjectIndexFromID(pBlitSpriteDef->simple_sprite_id), textureDataDef))
	{
		return false;
	}

	textureDataDef.renderMethod = pBlitSpriteDef->render_method;
	auto pBlitSprite = m_resourceMgr->CreateBlitSpriteDefinition();

	if (!pBlitSprite->Init(wldObj.tag, textureDataDef))
	{
		EQG_LOG_ERROR("Failed to create blitsprite: {}", wldObj.tag);
		return false;
	}

	m_resourceMgr->Add(pBlitSprite);
	return true;
}

bool WLDLoader::ParseTextureDataDefinition(uint32_t objectIndex, STextureDataDefinition& dataDef)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type == WLD_OBJ_BLITSPRITEDEFINITION_TYPE)
	{
		if (auto pSpriteDef = m_resourceMgr->Get<BlitSpriteDefinition>(wldObj.tag))
		{
			pSpriteDef->CopyDefinition(dataDef);
			return true;
		}

		WLD_OBJ_BLITSPRITEDEFINITION* pBlitSpriteDef = (WLD_OBJ_BLITSPRITEDEFINITION*)wldObj.data;
		uint32_t simpleSpriteID = GetObjectIndexFromID(pBlitSpriteDef->simple_sprite_id, objectIndex);

		if (!ParseTextureDataDefinitionFromSimpleSpriteInst(simpleSpriteID, dataDef))
			return false;

		dataDef.renderMethod = pBlitSpriteDef->render_method;
	}
	else if (wldObj.type == WLD_OBJ_BMINFO_TYPE)
	{
		// This creates a texture directly from the parsed bitmap.
		ParsedBMInfo* parsedBMInfo = (ParsedBMInfo*)wldObj.parsed_data;

		auto bitmap = parsedBMInfo->bitmaps[0];

		dataDef.sourceTextures.push_back(bitmap);
		dataDef.width = bitmap->GetWidth();
		dataDef.height = bitmap->GetHeight();
		dataDef.numFrames = 1;
		dataDef.columns = 1;
		dataDef.rows = 1;
	}
	else if (wldObj.type == WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE)
	{
		if (!ParseTextureDataDefinitionFromSimpleSpriteDef(objectIndex, dataDef))
			return false;
	}
	else if (wldObj.type == WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE)
	{
		if (!ParseTextureDataDefinitionFromSimpleSpriteInst(objectIndex, dataDef))
			return false;
	}
	else
	{
		return false;
	}

	dataDef.valid = true;
	return true;
}

bool WLDLoader::ParseTextureDataDefinitionFromSimpleSpriteDef(uint32_t objectIndex, STextureDataDefinition& dataDef)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	BufferReader reader(wldObj.data, wldObj.size);

	WLD_OBJ_SIMPLESPRITEDEFINITION* pSpriteDef = reader.read_ptr<WLD_OBJ_SIMPLESPRITEDEFINITION>();
	if (pSpriteDef->flags & WLD_OBJ_SPROPT_HAVECURRENTFRAME)
		dataDef.currentFrame = reader.read<uint32_t>();
	if (pSpriteDef->flags & WLD_OBJ_SPROPT_HAVESLEEP)
		dataDef.updateInterval = reader.read<uint32_t>();
	if (pSpriteDef->flags & WLD_OBJ_SPROPT_HAVESKIPFRAMES)
		dataDef.skipFrames = (pSpriteDef->flags & WLD_OBJ_SPROPT_SKIPFRAMES) != 0;

	std::vector<uint32_t> frameIDs;
	int width = -1;
	int height = -1;
	frameIDs.reserve(pSpriteDef->num_frames);
	for (uint32_t i = 0; i < pSpriteDef->num_frames; ++i)
	{
		uint32_t bmID = GetObjectIndexFromID(reader.read<uint32_t>());
		if (bmID == 0)
			return false;
		frameIDs.push_back(bmID);

		ParsedBMInfo* parsedBMInfo = (ParsedBMInfo*)m_objects[bmID].parsed_data;
		if (parsedBMInfo == nullptr)
			return false;

		if (width == -1 || height == -1)
		{
			width = parsedBMInfo->bitmaps[0]->GetWidth();
			height = parsedBMInfo->bitmaps[0]->GetHeight();
		}
	}

	if (!frameIDs.empty())
	{
		ParsedBMInfo* parsedBMInfo = (ParsedBMInfo*)m_objects[frameIDs[0]].parsed_data;
		auto firstBitmap = parsedBMInfo->bitmaps[0];

		dataDef.width = firstBitmap->GetWidth();
		dataDef.height = firstBitmap->GetHeight();
		dataDef.numFrames = (uint32_t)parsedBMInfo->bitmaps.size();

		if (dataDef.numFrames == 1)
		{
			dataDef.columns = 1;
			dataDef.rows = 1;
		}
		else if (dataDef.numFrames <= 4)
		{
			dataDef.columns = 2;
			dataDef.columns = 2;
		}
		else if (dataDef.numFrames <= 8)
		{
			dataDef.columns = 4;
			dataDef.rows = 2;
		}
		else if (dataDef.numFrames <= 16)
		{
			dataDef.columns = 4;
			dataDef.rows = 4;
		}
		else
		{
			return false;
		}

		// TODO: Combine the frames from the bitmap into a single texture.
		dataDef.sourceTextures = parsedBMInfo->bitmaps;
	}

	return true;
}

bool WLDLoader::ParseTextureDataDefinitionFromSimpleSpriteInst(uint32_t objectIndex, STextureDataDefinition& dataDef)
{
	WLD_OBJ_SIMPLESPRITEINSTANCE* pSpriteInst = (WLD_OBJ_SIMPLESPRITEINSTANCE*)m_objects[objectIndex].data;
	uint32_t spriteDefID = GetObjectIndexFromID(pSpriteInst->definition_id);

	if (spriteDefID == 0)
		return false;
	if (m_objects[spriteDefID].type != WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE)
		return false;

	if (pSpriteInst->flags & WLD_OBJ_SPROPT_HAVESKIPFRAMES)
		dataDef.skipFrames = (pSpriteInst->flags & WLD_OBJ_SPROPT_SKIPFRAMES) != 0;

	if (!ParseTextureDataDefinitionFromSimpleSpriteDef(spriteDefID, dataDef))
		return false;

	return true;
}

bool WLDLoader::ParseParticleClouds(std::pair<uint32_t, uint32_t> pcloudRange, std::string_view npcTag)
{
	for (uint32_t i = pcloudRange.first; i < pcloudRange.second; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_PCLOUDDEFINITION_TYPE)
		{
			// If NPC tag is provided, only process if the first three characters match.
			if (npcTag.empty() || ci_starts_with(m_objects[i].tag, npcTag.substr(0, 3)))
			{
				if (!ParseParticleCloud(i, 1.0f))
				{
					EQG_LOG_WARN("Failed to parse pcloud: {}", m_objects[i].tag);
					//return false;
				}
			}
		}
	}

	return true;
}

bool WLDLoader::ParseParticleCloud(uint32_t objectIndex, float scale)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_PCLOUDDEFINITION_TYPE)
	{
		return false;
	}

	if (auto pExistingPCloud = m_resourceMgr->Get<ParticleCloudDefinition>(wldObj.tag))
	{
		if (wldObj.parsed_data == nullptr)
		{
			ParsedPCloudDefinition* parsedData = new ParsedPCloudDefinition();
			parsedData->particleCloudDef = pExistingPCloud;
			wldObj.parsed_data = parsedData;
		}

		return true;
	}

	// Already parsed?
	if (wldObj.parsed_data != nullptr)
		return true;

	EQG_LOG_TRACE("Parsing pcloud: {} ({})", wldObj.tag, objectIndex);
	BufferReader reader(wldObj.data, wldObj.size);

	WLD_OBJ_PCLOUDDEFINITION* pWLDCloudDef = reader.read_ptr<WLD_OBJ_PCLOUDDEFINITION>();

	BOUNDINGBOX* pSpawnBoundingBox = nullptr;
	if (pWLDCloudDef->flags & WLD_OBJ_PCLOUDOPT_HASSPAWNBOX)
	{
		pSpawnBoundingBox = reader.read_ptr<BOUNDINGBOX>();
	}

	BOUNDINGBOX* pBoundingBox = nullptr;
	if (pWLDCloudDef->flags & WLD_OBJ_PCLOUDOPT_HASBBOX)
	{
		pBoundingBox = reader.read_ptr<BOUNDINGBOX>();
	}

	STextureDataDefinition spriteTextureDef;
	if (pWLDCloudDef->flags & WLD_OBJ_PCLOUDOPT_HASSPRITEDEF)
	{
		int spriteDefID = reader.read<int>();
		
		if (!ParseTextureDataDefinition(spriteDefID, spriteTextureDef))
		{
			EQG_LOG_ERROR("Failed to parse sprite definition {} for pcloud: {} ({})", spriteDefID, wldObj.tag, objectIndex);
			return false;
		}
	}

	SParticleCloudDefData pcloudDef;
	pcloudDef.type = pWLDCloudDef->particle_type;
	pcloudDef.flags = pWLDCloudDef->flags;
	pcloudDef.size = pWLDCloudDef->size;
	pcloudDef.scalarGravity = pWLDCloudDef->gravity_multiplier;
	pcloudDef.gravity = pWLDCloudDef->gravity;
	if (pSpawnBoundingBox)
	{
		pcloudDef.bbMin = pSpawnBoundingBox->min;
		pcloudDef.bbMax = pSpawnBoundingBox->max;
	}
	else
	{
		pcloudDef.bbMin = glm::vec3(0.0f);
		pcloudDef.bbMax = glm::vec3(0.0f);
	}
	pcloudDef.spawnType = pWLDCloudDef->spawn_type;
	pcloudDef.spawnTime = pWLDCloudDef->duration;
	pcloudDef.spawnMin = pcloudDef.bbMin;
	pcloudDef.spawnMax = pcloudDef.bbMax;
	pcloudDef.spawnNormal = pWLDCloudDef->spawn_velocity;
	pcloudDef.spawnRadius = pWLDCloudDef->spawn_radius;
	pcloudDef.spawnAngle = pWLDCloudDef->spawn_angle;
	pcloudDef.spawnLifespan = pWLDCloudDef->lifespan;
	pcloudDef.spawnVelocity = pWLDCloudDef->spawn_velocity_multiplier;
	pcloudDef.spawnRate = pWLDCloudDef->spawn_rate;
	pcloudDef.spawnScale = pWLDCloudDef->spawn_scale;
	pcloudDef.color = pWLDCloudDef->tint;
	pcloudDef.dagScale = scale;

	SEmitterDefData emitterDef;
	emitterDef.UpdateFromPCloudDef(wldObj.tag, pcloudDef, spriteTextureDef);

	auto pBlitSprite = m_resourceMgr->CreateBlitSpriteDefinition();
	if (!pBlitSprite->Init(wldObj.tag, spriteTextureDef))
	{
		EQG_LOG_ERROR("Failed to create blitsprite: {}", wldObj.tag);
		return false;
	}

	auto pPCloudDef = std::make_shared<ParticleCloudDefinition>();
	if (!pPCloudDef->Init(emitterDef, pBlitSprite))
	{
		EQG_LOG_ERROR("Failed to create particle cloud definition: {}", wldObj.tag);
		return false;
	}

	m_resourceMgr->Add(pPCloudDef);

	ParsedPCloudDefinition* parsedData = new ParsedPCloudDefinition();
	parsedData->particleCloudDef = pPCloudDef;
	wldObj.parsed_data = parsedData;

	return true;
}

bool WLDLoader::ParseTrack(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type != WLD_OBJ_TRACKINSTANCE_TYPE)
	{
		return false;
	}

	if (wldObj.parsed_data != nullptr)
	{
		return true;
	}

	if (wldObj.tag.empty())
	{
		EQG_LOG_ERROR("Track instance {} has no tag!", objectIndex);
		return false;
	}

	EQG_LOG_TRACE("Parsing track: {} ({})", wldObj.tag, objectIndex);

	// Create the track and set some defaults.
	std::shared_ptr<STrack> track = std::make_shared<STrack>();
	track->tag = wldObj.tag;
	track->sleepTime = 100;
	track->speed = 1.0f;
	track->reverse = false;
	track->interpolate = false;
	track->numFrames = 0;
	track->attachedToModel = !IsAnimationCode(wldObj.tag);

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_TRACKINSTANCE* pTrackInst = reader.read_ptr<WLD_OBJ_TRACKINSTANCE>();

	if (pTrackInst->flags & WLD_OBJ_TRKOPT_HAVESLEEP)
	{
		track->sleepTime = reader.read<uint32_t>();
	}

	if (pTrackInst->flags & WLD_OBJ_TRKOPT_REVERSE)
	{
		track->reverse = true;
	}

	if (pTrackInst->flags & WLD_OBJ_TRKOPT_INTERPOLATE)
	{
		track->interpolate = true;
	}

	// Look up track definition.
	WLDFileObject& trackDefObj = GetObjectFromID(pTrackInst->track_id, objectIndex);
	if (trackDefObj.type != WLD_OBJ_TRACKDEFINITION_TYPE)
	{
		EQG_LOG_ERROR("Track instance {} ({}) has invalid track definition!", wldObj.tag, objectIndex);
		return false;
	}

	if (trackDefObj.parsed_data != nullptr)
	{
		EQG_LOG_ERROR("Parsed track def {} ({}) twice!", trackDefObj.tag, pTrackInst->track_id);
		return false;
	}

	// Parse the track definition.
	BufferReader trackDefReader(trackDefObj.data, trackDefObj.size);
	WLD_OBJ_TRACKDEFINITION* pTrackDef = trackDefReader.read_ptr<WLD_OBJ_TRACKDEFINITION>();

	track->numFrames = pTrackDef->num_frames;

	// Read transforms from the track def
	track->frameTransforms.resize(track->numFrames);

	constexpr float ORIENTATION_SCALE = 1.0f / 16384.0f;
	constexpr float PIVOT_SCALE = 1.0f / 256.0f;

	for (uint32_t i = 0; i < track->numFrames; ++i)
	{
		EQG_S3D_PFRAMETRANSFORM* packedTransform = trackDefReader.read_ptr<EQG_S3D_PFRAMETRANSFORM>();
		SFrameTransform& transform = track->frameTransforms[i];

		transform.rotation = glm::quat(
			-static_cast<float>(packedTransform->rot_q3) * ORIENTATION_SCALE,
			static_cast<float>(packedTransform->rot_q0) * ORIENTATION_SCALE,
			static_cast<float>(packedTransform->rot_q1) * ORIENTATION_SCALE,
			static_cast<float>(packedTransform->rot_q2) * ORIENTATION_SCALE
		);
		transform.pivot = glm::vec3(
			static_cast<float>(packedTransform->pivot_x) * PIVOT_SCALE,
			static_cast<float>(packedTransform->pivot_y) * PIVOT_SCALE,
			static_cast<float>(packedTransform->pivot_z) * PIVOT_SCALE
		);
		transform.scale = static_cast<float>(packedTransform->scale) * PIVOT_SCALE;
	}

	// TODO: Something about luclin animations

	// TODO: Something about IT61 and  IT157

	ParsedTrackInstance* parsedTrackInst = new ParsedTrackInstance();
	parsedTrackInst->track = track;
	wldObj.parsed_data = parsedTrackInst;

	ParsedTrackDefinition* parsedTrackDef = new ParsedTrackDefinition();
	parsedTrackDef->track = track;
	wldObj.parsed_data = parsedTrackDef;

	return true;
}

bool WLDLoader::ParseAnimations(std::pair<uint32_t, uint32_t> trackRange, std::string_view npcTag)
{
	for (uint32_t objectIndex = trackRange.first; objectIndex < trackRange.second; ++objectIndex)
	{
		if (m_objects[objectIndex].type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			WLDFileObject& wldObj = m_objects[objectIndex];

			if (wldObj.type != WLD_OBJ_TRACKINSTANCE_TYPE)
			{
				EQG_LOG_ERROR("Object {} is not a track instance!", objectIndex);
				return false;
			}

			if (wldObj.parsed_data == nullptr)
			{
				EQG_LOG_ERROR("Track instance {} has not been parsed!", objectIndex);
				return false;
			}

			if (wldObj.tag.empty())
			{
				EQG_LOG_WARN("Track instance {} has no tag!", objectIndex);
				continue;
			}

			std::shared_ptr<STrack> pTrack = static_cast<ParsedTrackInstance*>(wldObj.parsed_data)->track;

			if (!pTrack->attachedToModel && IsAnimationCode(wldObj.tag))
			{
				EQG_LOG_TRACE("Parsing animation: {} ({})", wldObj.tag, objectIndex);
				std::string_view animTag;

				if (wldObj.tag.size() >= 6 && wldObj.tag[3] == 'I' && wldObj.tag[4] == 'T' && ::isdigit(wldObj.tag[5]))
				{
					// animation code looks something like AB3IT1
					// where AB3 checked by IsAnimationCode, and IT1 is checked above.
					// this is an item animation.
					uint32_t tagSize = 0;

					for (uint32_t pos = 6; pos < wldObj.tag.size() - 5; ++pos)
					{
						char ch = wldObj.tag[pos];

						if (!::isdigit(ch))
						{
							tagSize = pos;
							break;
						}
					}

					if (tagSize == 0)
					{
						EQG_LOG_ERROR("Failed to compute size of item tag length from item animation tag {} ({})",
							wldObj.tag, objectIndex);
						return false;
					}

					animTag = wldObj.tag.substr(0, tagSize);
				}
				else if (m_luclinAnims)
				{
					if (wldObj.tag.starts_with(wldObj.tag.substr(4, 3))
						|| wldObj.tag.starts_with(wldObj.tag.substr(3, 3)))
					{
						continue;
					}

					animTag = wldObj.tag.substr(0, 7);

					if (animTag.length() > 6 && animTag[6] == '_')
					{
						animTag = animTag.substr(0, 6);
					}
				}
				else
				{
					if (wldObj.tag.starts_with(wldObj.tag.substr(3, 3)))
					{
						continue;
					}

					animTag = wldObj.tag.substr(0, 6);
				}

				uint32_t nextObjectIndex = objectIndex + 1;

				if (!ParseAnimation(objectIndex, animTag, nextObjectIndex))
				{
					EQG_LOG_ERROR("Failed to parse animation {} ({}) \"{}\"", wldObj.tag, objectIndex, animTag);
					return false;
				}

				objectIndex = nextObjectIndex - 1;
			}
		}
	}

	return true;
}

static bool IsSpecialCaseAnim(std::string_view animTag, std::string_view trackAnimTag)
{
	if (animTag.size() < 6 || trackAnimTag.size() < 6)
		return false;

	std::string_view animCode = animTag.substr(3, 3);
	std::string_view tAnimTag = trackAnimTag.substr(3);

	// Handle a bunch of special cases in the animation tags
	// Not sure if these checks are for equality or just for prefix. Effectively they are the same.

	switch (animCode[0])
	{
	case 'H':
		if (animCode == "HSM" || animCode == "HSF" || animCode == "HSN")
		{
			if (tAnimTag.starts_with("POINT0") || tAnimTag.starts_with("RIDER"))
			{
				return true;
			}
		}
		break;

	case 'R':
		if (animCode == "REM")
		{
			if (tAnimTag.starts_with("L_POINT") || tAnimTag.starts_with("R_POINT") || tAnimTag.starts_with("HEAD_POINT"))
			{
				return true;
			}
		}
		break;

	case 'N':
		if (animCode == "NET")
		{
			if (tAnimTag.starts_with("POINT0"))
			{
				return true;
			}
		}
		break;

	case 'S':
		if (animCode == "SER")
		{
			if (tAnimTag.starts_with("HEAD_POINT")
				|| tAnimTag.starts_with("L_POINT")
				|| tAnimTag.starts_with("R_POINT")
				|| tAnimTag.starts_with("TUNIC_POINT"))
			{
				return true;
			}
		}
		break;

	default: break;
	}

	return false;
}

static bool IsSkippedAnimation(std::string_view animTag, bool newAnims, bool skipSocials)
{
	if (animTag.size() < 4)
		return false;

	// Anim code format:
	// style 1 ("new anims"):
	//   TNNVRRG
	// style 2
	//   TNNRRG
	//
	// T = type
	// NN = anim number
	// V = variant (new anims only)
	// RR = race code
	// G = gender code

	int animNum = -1;
	if (animTag[1] >= '0' && animTag[1] <= '9')
	{
		animNum = animTag[1] - '0';

		if (animTag[2] >= '0' && animTag[2] <= '9')
			animNum = animNum * 10 + animTag[2] - '0';
	}

	if (animNum == -1)
		return false;

	char animType = animTag[0];
	char variant = 0;
	std::string_view race;

	if (newAnims)
	{
		if (animNum > 0)
		{
			variant = animTag[3];
			race = animTag.substr(4, 2);
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (animNum > 0)
		{
			race = animTag.substr(3, 2);
		}
		else
		{
			return false;
		}
	}

	switch (animType)
	{
	case 'L':
		if (animNum == 11 || animNum == 12)
			return true;

		break;

	case 'S': // Social
		if (skipSocials)
		{
			if (animNum > 0 && animNum < 99)
				return true;
		}
		break;

	case 'T':
		if (newAnims)
		{
			switch (animNum)
			{
			case 1:
			case 2:
			case 3:
				switch (race[0])
				{
				case 'G':
					if (race[1] == 'N') // GN
						return true;
					break;
				case 'H':
					if (race[1] == 'O') // HO
						return true;
					break;
				case 'D':
					if (race[1] == 'W') // DW
						return true;
					break;
				case 'B':
					if (race[1] == 'A') // BA
						return true;
					break;
				case 'I':
					if (race[1] == 'K') // IK
						return true;
					break;
				default: break;
				}
				break;

			case 7:
			case 8:
			case 9:
				switch (race[0])
				{
				case 'H':
					switch (race[1])
					{
					case 'O': return true; // HO
					case 'I': return true; // HI
					case 'L': return true; // HL
					default: break;
					}
					break;
				case 'D':
					switch (race[1])
					{
					case 'A': return true; // DA
					case 'W': return true; // DW
					default: break;
					}
					break;
				case 'B':
					if (race[1] == 'A') // BA
						return true;
					break;
				case 'K':
					if (race[1] == 'E') // KE
						return true;
					break;
				case 'E':
					if (race[1] == 'L') // EL
						return true;
					break;
				case 'G':
					if (race[1] == 'N') // GN
						return true;
					break;
					default: break;
				}
				break;
			default: break;
			}
		}
		break;

	case 'C':
		if (newAnims)
		{
			switch (animNum)
			{
			case 2:
			case 3:
			case 4:
			case 5:
			case 9:
				if (variant == 'A')
					return true;
				break;

			case 11:
				switch (race[0])
				{
				case 'H':
					switch (race[1])
					{
					case 'O': return true; // HO
					case 'I': return true; // HI
					case 'L': return true; // HL
					default: break;
					}
					break;
				case 'D':
					switch (race[1])
					{
					case 'A': return true; // DA
					case 'W': return true; // DW
					default: break;
					}
					break;
				case 'B':
					if (race[1] == 'A') // BA
						return true;
					break;
				case 'K':
					if (race[1] == 'E') // KE
						return true;
					break;
				case 'E':
					if (race[1] == 'L') // EL
						return true;
					break;
				case 'G':
					if (race[1] == 'N') // GN
						return true;
					break;
				default: break;
				}
				break;
			default: break;
			}
		}
		break;

	default: break;
	}

	return false;
}

bool WLDLoader::ParseAnimation(uint32_t objectIndex, std::string_view animTag, uint32_t& nextObjectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	uint32_t trackCount = 0;
	uint32_t lastTrackIndex = objectIndex;

	// Count number of tracks related to this animation that we need to process.

	for (; lastTrackIndex <= m_numObjects; ++lastTrackIndex)
	{
		WLDFileObject& currObj = m_objects[lastTrackIndex];

		if (currObj.type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			if (currObj.tag.starts_with(animTag))
			{
				++trackCount;
			}
			else if (IsSpecialCaseAnim(animTag, currObj.tag))
			{
				++trackCount;
			}
			else
			{
				++lastTrackIndex;
				break;
			}
		}
		else if (currObj.type != WLD_OBJ_TRACKDEFINITION_TYPE)
		{
			++lastTrackIndex;
			break;
		}
	}

	nextObjectIndex = lastTrackIndex > m_numObjects ? lastTrackIndex : lastTrackIndex - 1;

	// We can't leave early (except in error) before this point, because
	// we need to ensure the next object index is correct, so we can skip over
	// these tracks that we looked at.

	if (animTag.substr(3, 3) == "VPM"
		&& wldObj.tag.substr(6).starts_with("EYE_L"))
	{
		return true;
	}

	// Already parsed this animation
	if (auto pExistingAnim = m_resourceMgr->Get<Animation>(animTag))
	{
		return true;
	}

	std::vector<std::shared_ptr<STrack>> tracks;

	uint32_t numSkippedTracks = 0;

	// Now process `trackCount` tracks in the range that we calculated previously.

	for (uint32_t trackIndex = objectIndex; trackIndex < lastTrackIndex; ++trackIndex)
	{
		WLDFileObject& trackObj = m_objects[trackIndex];

		if (trackObj.type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			if (tracks.size() + numSkippedTracks >= trackCount)
				break;
			
			if (IsSpecialCaseAnim(animTag, trackObj.tag))
			{
				++numSkippedTracks;
			}
			else
			{
				tracks.push_back(static_cast<ParsedTrackInstance*>(trackObj.parsed_data)->track);
			}
		}
	}

	if (tracks.size() < 2)
	{
		return true;
	}

	if (IsSkippedAnimation(animTag, m_newAnims, m_skipSocials))
	{
		EQG_LOG_DEBUG("Skipping anim: {}", animTag);
		return true;
	}

	std::shared_ptr<Animation> pAnimation = m_resourceMgr->CreateAnimation();

	if (!pAnimation->InitFromWLDData(animTag, tracks, m_optimizeAnims ? 1 : 0))
	{
		EQG_LOG_ERROR("Failed to initialize animation {} from WLD Data", animTag);
		return false;
	}

	m_resourceMgr->Add(pAnimation);

	return true;
}

bool WLDLoader::ParseActorDefinition(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type != WLD_OBJ_ACTORDEFINITION_TYPE)
		return false;

	if (auto pExistingActorDef = m_resourceMgr->Get<ActorDefinition>(wldObj.tag))
	{
		if (wldObj.parsed_data == nullptr)
		{
			ParsedActorDefinition* parsedData = new ParsedActorDefinition();
			parsedData->actorDefinition = pExistingActorDef;
			wldObj.parsed_data = parsedData;
		}

		return true;
	}

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_ACTORDEFINITION* pWLDActorDef = reader.read_ptr<WLD_OBJ_ACTORDEFINITION>();

	if (pWLDActorDef->flags & WLD_OBJ_ACTOROPT_HAVECURRENTACTION)
		reader.skip<uint32_t>();
	if (pWLDActorDef->flags & WLD_OBJ_ACTOROPT_HAVELOCATION)
		reader.skip<uint32_t>(7);

	if (pWLDActorDef->num_actions > 1)
	{
		EQG_LOG_ERROR("ActorDef {} ({}) has too many actions!", wldObj.tag, objectIndex);
		return false;
	}

	if (pWLDActorDef->num_actions == 1)
	{
		uint32_t numLODs = reader.read<uint32_t>();
		if (numLODs > 1)
		{
			EQG_LOG_ERROR("ActorDef {} ({}) has too many LODs!", wldObj.tag, objectIndex);
			return false;
		}

		if (numLODs == 1)
		{
			reader.skip<uint32_t>(2);
		}
	}

	if (pWLDActorDef->num_sprites > 1)
	{
		EQG_LOG_ERROR("ActorDef {} ({}) has too many sprites!", wldObj.tag, objectIndex);
		return false;
	}

	SimpleModelDefinitionPtr simpleModelDef;
	HierarchicalModelDefinitionPtr hierarchicalModelDef;

	if (pWLDActorDef->num_sprites == 1)
	{
		uint32_t spriteIndex = reader.read<uint32_t>();

		WLDFileObject& spriteObj = m_objects[spriteIndex];

		if (spriteObj.type == WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		{
			if (!ParseSimpleModel(spriteIndex, simpleModelDef))
			{
				EQG_LOG_ERROR("Failed to parse simple model {} ({}) for ActorDef {} ({})",
					spriteObj.tag, spriteIndex, wldObj.tag, objectIndex);
				return false;
			}
		}
		else if (spriteObj.type == WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE)
		{
			if (!ParseHierarchicalModel(spriteIndex, hierarchicalModelDef))
			{
				EQG_LOG_ERROR("Failed to parse hierarchical model {} ({}) for ActorDef {} ({})",
					spriteObj.tag, spriteIndex, wldObj.tag, objectIndex);
				return false;
			}
		}
		else if (spriteObj.type == WLD_OBJ_BLITSPRITEINSTANCE_TYPE)
		{
			// Read the sprite instance
			BufferReader spriteReader(spriteObj.data, spriteObj.size);
			WLD_OBJ_BLITSPRITEINSTANCE* pSpriteInst = spriteReader.read_ptr<WLD_OBJ_BLITSPRITEINSTANCE>();

			// Read the sprite definition
			uint32_t spriteDefIndex = GetObjectIndexFromID(pSpriteInst->definition_id);
			WLDFileObject& spriteDefObj = m_objects[spriteDefIndex];
			//STextureDataDefinition dataDef;
			//ParseTextureDataDefinition(spriteDefIndex, dataDef);

			EQG_LOG_TRACE("Not processing BLITSPRITEDEFINITION {} in ActorDef {} ({})",
				spriteDefObj.tag, wldObj.tag, objectIndex);
		}
		else
		{
			EQG_LOG_WARN("Skipped parsing of sprite {} ({}) type={} in ActorDef {} ({})",
				m_objects[spriteIndex].tag, spriteIndex, (int)spriteObj.type, wldObj.tag, objectIndex);
		}
	}

	reader.skip<uint32_t>();

	ECollisionVolumeType collisionVolumeType = eCollisionVolumeNone;
	if (pWLDActorDef->flags & WLD_OBJ_ACTOROPT_SPRITEVOLUMEONLY)
	{
		collisionVolumeType = eCollisionVolumeModel;
	}

	std::string_view callbackTag;
	if (pWLDActorDef->callback_id < 0)
	{
		callbackTag = GetString(pWLDActorDef->callback_id);
	}

	std::shared_ptr<ActorDefinition> pNewActorDef;

	if (hierarchicalModelDef)
	{
		pNewActorDef = std::make_shared<ActorDefinition>(wldObj.tag, hierarchicalModelDef);
	}
	else
	{
		// note: Takes in a null simple model if we didn't create one above.
		pNewActorDef = std::make_shared<ActorDefinition>(wldObj.tag, simpleModelDef);
	}

	if (pNewActorDef)
	{
		if (!callbackTag.empty())
		{
			pNewActorDef->SetCallbackTag(callbackTag);
		}

		pNewActorDef->SetCollisionVolumeType(collisionVolumeType);

		m_resourceMgr->Add(pNewActorDef);
	}

	ParsedActorDefinition* parsedActorDef = new ParsedActorDefinition;
	parsedActorDef->actorDefinition = pNewActorDef;

	wldObj.parsed_data = parsedActorDef;

	return true;
}

bool WLDLoader::ParseActorInstance(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type != WLD_OBJ_ACTORINSTANCE_TYPE)
		return false;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_ACTORINSTANCE* pWLDActorInst = reader.read_ptr<WLD_OBJ_ACTORINSTANCE>();

	ECollisionVolumeType collisionVolumeType = eCollisionVolumeNone;
	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_SPRITEVOLUMEONLY)
	{
		collisionVolumeType = eCollisionVolumeModel;
	}

	bool boundingBox = (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_USEBOUNDINGBOX) != 0;
	float boundingRadius = 0.0f;
	float scaleFactor = 0.0f;
	SLocation* location = nullptr;
	std::unique_ptr<SDMRGBTrackWLDData> pDMRGBTrackData;

	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_HAVECURRENTACTION)
		reader.skip<uint32_t>();
	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_HAVELOCATION)
		location = reader.read_ptr<SLocation>();
	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_HAVEBOUNDINGRADIUS)
		boundingRadius = reader.read<float>();
	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_HAVESCALEFACTOR)
		scaleFactor = reader.read<float>();

	if (pWLDActorInst->flags & WLD_OBJ_ACTOROPT_HAVEDMRGBTRACK)
	{
		uint32_t DMRGBTrackIndex = GetObjectIndexFromID(reader.read<uint32_t>());

		if (!ParseDMRGBTrack(DMRGBTrackIndex, pDMRGBTrackData))
			return false;
	}

	// Find actor definition
	if (pWLDActorInst->actor_def_id >= 0)
	{
		return false;
	}

	std::string_view actorDefTag = GetString(pWLDActorInst->actor_def_id);
	ActorDefinitionPtr actorDef = m_resourceMgr->Get<ActorDefinition>(actorDefTag);

	std::string actorTag = wldObj.tag.empty() ? fmt::format("ZoneActor_{:05}", objectIndex) : std::string(wldObj.tag);

	// Convert positional information.
	glm::vec3 position = glm::vec3(location->x, location->y, location->z);
	glm::vec3 orientation = glm::vec3(
		location->roll * EQ_TO_RAD,
		-location->pitch * EQ_TO_RAD,
		location->heading * EQ_TO_RAD
	);

	std::shared_ptr<Actor> actor;

	if (actorDef->GetSimpleModelDefinition())
	{
		actor = m_resourceMgr->CreateSimpleActor(
			actorTag,
			actorDef,
			position,
			orientation,
			scaleFactor,
			collisionVolumeType,
			boundingRadius,
			objectIndex,
			pDMRGBTrackData.get());
	}
	else if (actorDef->GetHierarchicalModelDefinition())
	{
		actor = std::make_shared<HierarchicalActor>(
			m_resourceMgr,
			actorTag,
			actorDef,
			position,
			orientation,
			scaleFactor,
			boundingRadius,
			collisionVolumeType,
			objectIndex,
			pDMRGBTrackData.get());
	}

	if (!actor)
	{
		EQG_LOG_ERROR("Failed to create ActorInstance for {} ({})", wldObj.tag, objectIndex);
		return true;
	}

	m_actors.push_back(actor);

	auto parsedActorInstance = new ParsedActorInstance();
	parsedActorInstance->actorInstance = actor;

	wldObj.parsed_data = parsedActorInstance;
	return true;
}

bool WLDLoader::ParseDMRGBTrack(uint32_t objectIndex, std::unique_ptr<SDMRGBTrackWLDData>& outData)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	outData.reset();

	if (wldObj.type != WLD_OBJ_DMRGBTRACKINSTANCE_TYPE)
		return false;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_DMRGBTRACKINSTANCE* pDMRGBTrackInst = reader.read_ptr<WLD_OBJ_DMRGBTRACKINSTANCE>();

	uint32_t DMRGBTrackDefID = GetObjectIndexFromID(pDMRGBTrackInst->definition_id);
	if (DMRGBTrackDefID <= 0)
	{
		EQG_LOG_ERROR("Invalid DMRGBTrackDef index {} from DMRGBTrackInstance {} ({})",
			DMRGBTrackDefID, wldObj.tag, objectIndex);
		return false;
	}

	WLDFileObject& defWldObj = m_objects[DMRGBTrackDefID];
	if (defWldObj.type != WLD_OBJ_DMRGBTRACKDEFINITION_TYPE)
	{
		EQG_LOG_ERROR("definition {} ({}) is not a DMRGBTrackDefinition for DMRGBTrackInstance {} ({})",
			defWldObj.tag, DMRGBTrackDefID, wldObj.tag, objectIndex);
		return false;
	}

	BufferReader defReader(defWldObj.data, defWldObj.size);
	WLD_OBJ_DMRGBTRACKDEFINITION* pDMRGBTrackDef = defReader.read_ptr<WLD_OBJ_DMRGBTRACKDEFINITION>();

	outData = std::make_unique<SDMRGBTrackWLDData>();
	outData->numRGBs = pDMRGBTrackDef->num_vertices;
	outData->RGBs = defReader.read_array<uint32_t>(outData->numRGBs);
	outData->hasAlphas = (pDMRGBTrackDef->flags & 0x1);

	return true;
}

bool WLDLoader::ParseSimpleModel(uint32_t objectIndex, std::shared_ptr<SimpleModelDefinition>& outModel)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	outModel.reset();

	if (wldObj.type != WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		return false;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_DMSPRITEINSTANCE* pDMSpriteInst = reader.read_ptr<WLD_OBJ_DMSPRITEINSTANCE>();

	uint32_t dmSpriteDefIndex = GetObjectIndexFromID(pDMSpriteInst->definition_id);
	if (dmSpriteDefIndex <= 0)
	{
		EQG_LOG_ERROR("Invalid DMSpriteDef index {} from DMSpriteInstance {} ({})",
			dmSpriteDefIndex, wldObj.tag, objectIndex);
		return false;
	}

	WLDFileObject& dmSpriteDefObj = m_objects[dmSpriteDefIndex];

	// DMSpriteDefinitions are always DMSPRITEDEF2's.
	if (dmSpriteDefObj.type != WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
	{
		EQG_LOG_ERROR("definition {} is not a DMSPRITEDEF2 (it is {}) for DMSpriteInstance {} ({}) ",
			dmSpriteDefIndex, (int)dmSpriteDefObj.type, wldObj.tag, objectIndex);
		return false;
	}

	// Check if we already have this object
	if (auto simpleModelDef = m_resourceMgr->Get<SimpleModelDefinition>(dmSpriteDefObj.tag))
	{
		outModel = simpleModelDef;

		if (dmSpriteDefObj.parsed_data == nullptr)
		{
			// Fill in the parsed_data, as some other object may need it.
			auto parsedData = new ParsedDMSpriteDefinition2();
			parsedData->simpleModelDefinition = simpleModelDef;

			dmSpriteDefObj.parsed_data = parsedData;
		}

		return true;
	}

	std::unique_ptr<SDMSpriteDef2WLDData> dmSpriteDefData;
	if (!ParseDMSpriteDef2(dmSpriteDefIndex, dmSpriteDefData))
	{
		EQG_LOG_ERROR("Failed to parse DMSpriteDef2 for DMSpriteInstance {} ({})", dmSpriteDefObj.tag, objectIndex);
		return false;
	}

	auto simpleModelDef = m_resourceMgr->CreateSimpleModelDefinition();
	if (!simpleModelDef->InitFromWLDData(dmSpriteDefObj.tag, dmSpriteDefData.get()))
	{
		EQG_LOG_ERROR("Failed to create SimpleModelDefinition {} from DMSpriteDef2 {} ({})",
			wldObj.tag, dmSpriteDefObj.tag, dmSpriteDefIndex);
		return false;
	}

	m_resourceMgr->Add(wldObj.tag, simpleModelDef);

	outModel = simpleModelDef;

	auto parsedData = new ParsedDMSpriteDefinition2();
	parsedData->simpleModelDefinition = simpleModelDef;

	dmSpriteDefObj.parsed_data = parsedData;
	return true;
}

bool WLDLoader::ParseHierarchicalModel(uint32_t objectIndex, std::shared_ptr<HierarchicalModelDefinition>& outModel)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type != WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE)
		return false;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_HIERARCHICALSPRITEINSTANCE* pHierSpriteInst = reader.read_ptr<WLD_OBJ_HIERARCHICALSPRITEINSTANCE>();

	WLDFileObject& defObj = GetObjectFromID(pHierSpriteInst->definition_id);
	if (defObj.type != WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE)
	{
		EQG_LOG_ERROR("Invalid HierarchicalSpriteDef {} ({}) from HierarchicalSpriteInstance {} ({})",
			defObj.tag, pHierSpriteInst->definition_id, wldObj.tag, objectIndex);
		return false;
	}

	if (auto pExistingDef = m_resourceMgr->Get<HierarchicalModelDefinition>(defObj.tag))
	{
		if (defObj.parsed_data == nullptr)
		{
			auto parsedData = new ParsedHierarchicalModelDef();
			parsedData->hierarchicalModelDef = pExistingDef;
			defObj.parsed_data = parsedData;
		}

		outModel = pExistingDef;
		return true;
	}

	BufferReader defReader(defObj.data, defObj.size);
	WLD_OBJ_HIERARCHICALSPRITEDEFINITION* pHierSpriteDef = defReader.read_ptr<WLD_OBJ_HIERARCHICALSPRITEDEFINITION>();

	std::unique_ptr<SHSpriteDefWLDData> hSpriteWLDData = std::make_unique<SHSpriteDefWLDData>();

	if (pHierSpriteDef->flags & WLD_OBJ_SPROPT_DAGCOLLISIONS)
	{
	}
	else if (pHierSpriteDef->collision_volume_id != 0)
	{
		// TODO
	}

	if (pHierSpriteDef->flags & WLD_OBJ_SPROPT_HAVECENTEROFFSET)
	{
		hSpriteWLDData->centerOffset = defReader.read<glm::vec3>();
	}
	if (pHierSpriteDef->flags & WLD_OBJ_SPROPT_HAVEBOUNDINGRADIUS)
	{
		hSpriteWLDData->boundingRadius = defReader.read<float>();
	}

	uint32_t numDags = pHierSpriteDef->num_dags;
	hSpriteWLDData->dags.resize(numDags);

	// Handle dags
	for (uint32_t dagIndex = 0; dagIndex < numDags; ++dagIndex)
	{
		WLDDATA_DAG* pWldDag = defReader.read_ptr<WLDDATA_DAG>();
		SDagWLDData& dagData = hSpriteWLDData->dags[dagIndex];

		if (pWldDag->num_sub_dags != 0)
		{
			dagData.subDags.resize(pWldDag->num_sub_dags);
			for (uint32_t subDagIndex = 0; subDagIndex < pWldDag->num_sub_dags; ++subDagIndex)
			{
				uint32_t subDagNum = defReader.read<uint32_t>();
				dagData.subDags[subDagIndex] = &hSpriteWLDData->dags[subDagNum];
			}
		}

		if (pWldDag->track_id != 0)
		{
			WLDFileObject& trackInstObj = GetObjectFromID(pWldDag->track_id);
			if (trackInstObj.type != WLD_OBJ_TRACKINSTANCE_TYPE)
			{
				EQG_LOG_ERROR("Dag {} has invalid track instance {}", dagIndex, pWldDag->track_id);
				return false;
			}

			if (trackInstObj.parsed_data == nullptr)
			{
				EQG_LOG_ERROR("track id {} is not parsed", pWldDag->track_id);
				return false;
			}

			ParsedTrackInstance* parsedTrackInst = (ParsedTrackInstance*)trackInstObj.parsed_data;
			dagData.track = parsedTrackInst->track;
			dagData.track->frameTransforms[0].rotation.w = -dagData.track->frameTransforms[0].rotation.w;
		}

		if (dagData.track != nullptr && !dagData.track->tag.empty())
		{
			dagData.tag = dagData.track->tag;
		}
		else
		{
			dagData.tag = pWldDag->tag < 0 ? GetString(pWldDag->tag) : "";
		}

		if (ci_starts_with(dagData.tag, "PKLMPBN502A")) // ???
		{
			dagData.track->frameTransforms[0].pivot.x = -11.0f;
			dagData.track->frameTransforms[0].pivot.z = 25.0f;
		}

		if (pWldDag->sprite_id != 0)
		{
			uint32_t spriteIndex = GetObjectIndexFromID(pWldDag->sprite_id);

			if (m_objects[spriteIndex].type == WLD_OBJ_DMSPRITEINSTANCE_TYPE)
			{
				SimpleModelDefinitionPtr simpleModelDef;
				if (!ParseSimpleModel(spriteIndex, simpleModelDef))
				{
					EQG_LOG_ERROR("Failed to parse simple model {} ({}) while parsing hierchical model {} ({})",
						m_objects[spriteIndex].tag, spriteIndex, wldObj.tag, objectIndex);
					return false;
				}

				dagData.attachedActor = std::make_shared<ActorDefinition>("", simpleModelDef);
			}
			else if (m_objects[spriteIndex].type == WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE)
			{
				// Lol, here we goooooo....
				HierarchicalModelDefinitionPtr hierModelDef;
				if (!ParseHierarchicalModel(spriteIndex, hierModelDef))
				{
					EQG_LOG_ERROR("Failed to parse hierarchical model {} ({}) while parsing hierchical model {} ({})",
						m_objects[spriteIndex].tag, spriteIndex, wldObj.tag, objectIndex);
					return false;
				}

				dagData.attachedActor = std::make_shared<ActorDefinition>("", hierModelDef);
			}
			else if (m_objects[spriteIndex].type == WLD_OBJ_PCLOUDDEFINITION_TYPE)
			{
				float scale = dagData.track ? dagData.track->frameTransforms[0].scale : 1.0f;

				if (!ParseParticleCloud(spriteIndex, scale))
				{
					EQG_LOG_ERROR("Failed to parse particle cloud {} ({}) while parsing hierchical model {} ({})",
						m_objects[spriteIndex].tag, spriteIndex, wldObj.tag, objectIndex);
					return false;
				}

				if (ParsedPCloudDefinition* parsedData = (ParsedPCloudDefinition*)m_objects[spriteIndex].parsed_data)
				{
					std::shared_ptr<ParticleCloudDefinition> pCloudDef = parsedData->particleCloudDef;

					dagData.attachedActor = std::make_shared<ActorDefinition>("", pCloudDef);
				}
			}
			else
			{
				EQG_LOG_ERROR("Dag {} has invalid sprite type for {} ({}) in Hierarchical model {} ({})",
					dagIndex, m_objects[spriteIndex].tag, spriteIndex, wldObj.tag, objectIndex);
				return false;
			}
		}
	}

	// Handle skins
	if (pHierSpriteDef->flags & WLD_OBJ_SPROPT_HAVEATTACHEDSKINS)
	{
		uint32_t numAttachedSkinsActual = defReader.read<uint32_t>();
		hSpriteWLDData->activeSkin = (uint32_t)-1;
		hSpriteWLDData->numAttachedSkins = numAttachedSkinsActual;

		uint32_t attachedSkinIndex = GetObjectIndexFromID(defReader.read<uint32_t>());

		if (m_objects[attachedSkinIndex].type != WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		{
			EQG_LOG_ERROR("Attached skin {} ({}) is not a DMSPRITEINSTANCE for HierarchicalSpriteDef {} ({})",
				m_objects[attachedSkinIndex].tag, attachedSkinIndex, wldObj.tag, objectIndex);
			return false;
		}

		WLD_OBJ_DMSPRITEINSTANCE* pDMSpriteInst = (WLD_OBJ_DMSPRITEINSTANCE*)m_objects[attachedSkinIndex].data;
		uint32_t DMSpriteDefIndex = GetObjectIndexFromID(pDMSpriteInst->definition_id);

		WLDFileObject& DMSpriteDefObj = m_objects[DMSpriteDefIndex];
		auto tag = DMSpriteDefObj.tag;

		int firstIndex = DMSpriteDefIndex;
		int lastIndex = firstIndex;

		if ((tag.size() >= 3 && tag[0] == 'I' && tag[1] == 'T' && ::isdigit(tag[2]))
			|| ci_starts_with(DMSpriteDefObj.tag, "POKLAMP502")
			|| ci_starts_with(DMSpriteDefObj.tag, "CDTHRONE"))
		{
			lastIndex = firstIndex + numAttachedSkinsActual;
			hSpriteWLDData->activeSkin = 0;
		}
		else
		{
			// I guess we're searching for skins because we don't trust numAttachedSkins!
			for (lastIndex = firstIndex; lastIndex < firstIndex + 20; ++lastIndex)
			{
				if (lastIndex > (int)m_numObjects || m_objects[lastIndex].type != WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
					break;
			}

			int origFirstIndex = firstIndex;
			for (; firstIndex > origFirstIndex - 20; --firstIndex)
			{
				if (firstIndex < 0 || m_objects[firstIndex].type != WLD_OBJ_DMSPRITEDEFINITION2_TYPE || m_objects[firstIndex].parsed_data != nullptr)
				{
					++firstIndex;
					break;
				}

				++hSpriteWLDData->activeSkin;
			}

			firstIndex = std::min(firstIndex, lastIndex);
		}

		numAttachedSkinsActual = std::max((uint32_t)lastIndex - (uint32_t)firstIndex, numAttachedSkinsActual);

		if (numAttachedSkinsActual > 0)
		{
			hSpriteWLDData->attachedSkins.resize(numAttachedSkinsActual);

			for (uint32_t skinIndex = 0; skinIndex < numAttachedSkinsActual; ++skinIndex)
			{
				std::unique_ptr<SDMSpriteDef2WLDData> pDMSpriteDef2;

				if (!ParseDMSpriteDef2(firstIndex + skinIndex, pDMSpriteDef2))
				{
					EQG_LOG_ERROR("Failed to parse DMSpriteDef2 for attached skin {} ({}) in HierarchicalSpriteDef {} ({}) "
						"numAttachedSkins={} firstIndex={} lastIndex={} actual={}",
						m_objects[firstIndex + skinIndex].tag, firstIndex + skinIndex, wldObj.tag, objectIndex,
						numAttachedSkinsActual, firstIndex, lastIndex, hSpriteWLDData->numAttachedSkins);
					return false;
				}

				if (pDMSpriteDef2)
				{
					if (!hSpriteWLDData->materialPalette)
					{
						hSpriteWLDData->materialPalette = pDMSpriteDef2->materialPalette;
					}

					if (pDMSpriteDef2->tag.empty())
					{
						pDMSpriteDef2->tag = m_objects[firstIndex + skinIndex].tag;
					}

					if (!pDMSpriteDef2->tag.empty())
					{
						if (ci_starts_with(pDMSpriteDef2->tag, "PIFHE01")
							|| ci_starts_with(pDMSpriteDef2->tag, "FAFHE01"))
						{
							const_cast<char*>(pDMSpriteDef2->tag.data())[3] = 'W'; // neat!
						}
					}
				}

				hSpriteWLDData->attachedSkins[skinIndex] = std::move(pDMSpriteDef2);

				// I guess we are skipping reading the indices from the data?
				if (skinIndex < hSpriteWLDData->numAttachedSkins)
					defReader.skip<uint32_t>();
			}
		}

		hSpriteWLDData->skeletonDagIndices = defReader.read_array<int>(hSpriteWLDData->numAttachedSkins);
	}

	HierarchicalModelDefinitionPtr pHierarchicalModelDef = std::make_shared<HierarchicalModelDefinition>();
	if (!pHierarchicalModelDef->InitFromWLDData(defObj.tag, hSpriteWLDData.get()))
	{
		EQG_LOG_ERROR("Failed to create HierarchicalModelDefinition from HierarchicalSpriteDef {} ({} -- instance {} ({}))",
			defObj.tag, pHierSpriteInst->definition_id, wldObj.tag, objectIndex);
		return false;
	}

	if (!m_resourceMgr->Add(pHierarchicalModelDef))
	{
		EQG_LOG_ERROR("Failed to add HierarchicalModelDefinition {} to resource manager!", defObj.tag);
		return false;
	}

	auto parsedData = new ParsedHierarchicalModelDef();
	parsedData->hierarchicalModelDef = pHierarchicalModelDef;

	defObj.parsed_data = parsedData;

	outModel = pHierarchicalModelDef;
	return true;
}

bool WLDLoader::ParseDMSpriteDef2(uint32_t objectIndex, std::unique_ptr<SDMSpriteDef2WLDData>& pSpriteDef)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type == WLD_OBJ_DMSPRITEDEFINITION_TYPE)
	{
		pSpriteDef.reset();
		return true; // Old type seems to be completely ignored
	}

	if (wldObj.type != WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
	{
		return false; // Everything else that isn't a dmspritedef2 is a fail
	}

	pSpriteDef = std::make_unique<SDMSpriteDef2WLDData>();
	memset(pSpriteDef.get(), 0, sizeof(SDMSpriteDef2WLDData));
	pSpriteDef->tag = wldObj.tag;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_DMSPRITEDEFINITION2* pWLDObj = reader.read_ptr<WLD_OBJ_DMSPRITEDEFINITION2>();
	pSpriteDef->vertexScaleFactor = 1.0f / (float)(1 << pWLDObj->scale);

	if (pWLDObj->num_vertices)
	{
		pSpriteDef->numVertices = pWLDObj->num_vertices;
		if (!reader.read_array(pSpriteDef->vertices, pSpriteDef->numVertices))
			return false;
	}

	if (pWLDObj->num_uvs)
	{
		pSpriteDef->numUVs = pWLDObj->num_uvs;
		
		if (IsOldVersion())
		{
			pSpriteDef->uvsUsingOldForm = true;
			if (!reader.read_array(pSpriteDef->uvsOldForm, pSpriteDef->numUVs))
				return false;
		}
		else
		{
			pSpriteDef->uvsUsingOldForm = false;
			if (!reader.read_array(pSpriteDef->uvs, pSpriteDef->numUVs))
				return false;
		}
	}

	if (pWLDObj->num_vertex_normals)
	{
		pSpriteDef->numVertexNormals = pWLDObj->num_vertex_normals;
		if (!reader.read_array(pSpriteDef->vertexNormals, pSpriteDef->numVertexNormals))
			return false;
	}

	if (pWLDObj->num_rgb_colors)
	{
		pSpriteDef->numRGBs = pWLDObj->num_rgb_colors;
		if (!reader.read_array(pSpriteDef->rgbData, pSpriteDef->numRGBs))
			return false;
	}

	if (pWLDObj->num_faces)
	{
		pSpriteDef->numFaces = pWLDObj->num_faces;
		if (!reader.read_array(pSpriteDef->faces, pSpriteDef->numFaces))
			return false;
	}

	if (pWLDObj->num_skin_groups)
	{
		pSpriteDef->numSkinGroups = pWLDObj->num_skin_groups;
		if (!reader.read_array(pSpriteDef->skinGroups, pSpriteDef->numSkinGroups))
			return false;
	}

	if (pWLDObj->num_fmaterial_groups)
	{
		pSpriteDef->numFaceMaterialGroups = pWLDObj->num_fmaterial_groups;
		if (!reader.read_array(pSpriteDef->faceMaterialGroups, pSpriteDef->numFaceMaterialGroups))
			return false;
	}

	if (pWLDObj->num_vmaterial_groups)
	{
		pSpriteDef->numVertexMaterialGroups = pWLDObj->num_vmaterial_groups;
		if (!reader.read_array(pSpriteDef->vertexMaterialGroups, pSpriteDef->numVertexMaterialGroups))
			return false;
	}

	// MeshOps at the end are ignored/unused

	// TODO: Clean up interface to other objects
	WLDFileObject& materialObj = m_objects[GetObjectIndexFromID(pWLDObj->material_palette_id)];
	if (materialObj.type == WLD_OBJ_MATERIALPALETTE_TYPE)
	{
		pSpriteDef->materialPalette = static_cast<ParsedMaterialPalette*>(materialObj.parsed_data)->palette;
	}
	else
	{
		EQG_LOG_ERROR("Material palette does not point to a material palette!");
		return false;
	}

	if (pWLDObj->dm_track_id)
	{
		WLDFileObject& trackInstObj = m_objects[GetObjectIndexFromID(pWLDObj->dm_track_id)];
		pSpriteDef->trackInstance = (WLD_OBJ_TRACKINSTANCE*)trackInstObj.data;

		WLDFileObject& trackDefObj = m_objects[GetObjectIndexFromID(pSpriteDef->trackInstance->track_id)];
		pSpriteDef->trackDefinition = (WLD_OBJ_DMTRACKDEFINITION2*)trackDefObj.data;
	}

	// The only piece of collision info that is used is whether this is set or not.
	if ((pWLDObj->flags & WLD_OBJ_SPROPT_SPRITEDEFPOLYHEDRON) != 0)
	{
		// ignored
	}
	else if (pWLDObj->collision_volume_id == 0)
	{
		// This is checked later when creating a simple model, but we can do it here and simplify a bit.
		if (!ci_starts_with(m_archive->GetFileName(), "gequip"))
		{
			pSpriteDef->noCollision = true;
		}
	}

	if (pWLDObj->flags & WLD_OBJ_SPROPT_HAVECENTEROFFSET)
	{
		pSpriteDef->centerOffset = pWLDObj->center_offset;
	}

	if (pWLDObj->flags & WLD_OBJ_SPROPT_HAVEBOUNDINGRADIUS)
	{
		pSpriteDef->boundingRadius = pWLDObj->bounding_radius;
	}
	else
	{
		pSpriteDef->boundingRadius = 1.0f;
	}

	return true;
}

bool WLDLoader::ParseTerrain(
	std::pair<uint32_t, uint32_t> regionRange,
	std::pair<uint32_t, uint32_t> zoneRange,
	int worldTreeIndex, int constantAmbientIndex)
{
	STerrainWLDData terrainData;
	terrainData.worldTree = std::make_shared<SWorldTreeWLDData>();

	for (uint32_t regionIndex = regionRange.first; regionIndex < regionRange.second; ++regionIndex)
	{
		if (m_objects[regionIndex].type == WLD_OBJ_REGION_TYPE)
		{
			if (!ParseRegion(regionIndex, terrainData))
			{
				EQG_LOG_ERROR("Failed to parse terrain region '{}'", m_objects[regionIndex].tag);
				return false;
			}
		}
	}

	std::shared_ptr<Terrain> pTerrain = m_resourceMgr->InitTerrain();

	// Parse areas, send them to the terrain
	for (uint32_t objectIndex = zoneRange.first; objectIndex < zoneRange.second; ++objectIndex)
	{
		if (m_objects[objectIndex].type == WLD_OBJ_ZONE_TYPE)
		{
			if (!ParseArea(objectIndex, objectIndex - zoneRange.first, terrainData))
			{
				EQG_LOG_ERROR("Failed to parse terrain area '{}'", m_objects[objectIndex].tag);
				return false;
			}
		}
	}

	if (!ParseWorldTree(worldTreeIndex, terrainData))
	{
		EQG_LOG_ERROR("Failed to parse world tree");
		return false;
	}

	if (!ParseConstantAmbient(constantAmbientIndex, terrainData))
	{
		EQG_LOG_ERROR("Failed to parse constant ambient color");
		return false;
	}

	if (!pTerrain->InitFromWLDData(terrainData))
	{
		EQG_LOG_ERROR("Failed to init terrain from wld data");
		return false;
	}

	return true;
}

bool WLDLoader::ParseRegion(uint32_t objectIndex, STerrainWLDData& terrainData)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	BufferReader reader(wldObj.data, wldObj.size);

	SRegionWLDData& region = terrainData.regions.emplace_back();
	WLD_OBJ_REGION* header = reader.read_ptr<WLD_OBJ_REGION>();

	region.tag = wldObj.tag;
	region.ambientLightIndex = GetObjectIndexFromID(header->ambient_light_id);

	if (header->num_vis_nodes)
	{
		reader.skip<glm::vec3>(header->num_vis_nodes);
		reader.skip<uint32_t>(header->num_vis_nodes * 4);
	}

	if (header->flags & WLD_OBJ_REGOPT_ENCODEDVISIBILITY)
		region.visibilityType = 1;
	if (header->flags & WLD_OBJ_REGOPT_ENCODEDVISIBILITY2)
		region.visibilityType = 2;

	region.range = reader.read<uint16_t>();
	region.encodedVisibility = reader.peek<uint8_t>();

	// Skipping over visibility data
	if (region.visibilityType != 2)
		reader.skip<uint16_t>(region.range);
	else
		reader.skip<uint8_t>(region.range);

	if (header->flags & WLD_OBJ_REGOPT_HAVESPHERE)
	{
		reader.read(region.sphereCenter);
		reader.read(region.sphereRadius);
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVEREVERBVOLUME)
		reader.read(region.reverbVolume);
	if (header->flags & WLD_OBJ_REGOPT_HAVEREVERBOFFSET)
		reader.read(region.reverbOffset);

	// Skipping over userdata
	if (uint32_t udSize = reader.read<uint32_t>())
		reader.skip(udSize);
	
	if (header->flags & WLD_OBJ_REGOPT_HAVEREGIONDMSPRITE)
		region.regionSpriteIndex = GetObjectIndexFromID(reader.read<uint32_t>());
	if (header->flags & WLD_OBJ_REGOPT_HAVEREGIONDMSPRITEDEF)
	{
		region.regionSpriteIndex = GetObjectIndexFromID(reader.read<uint32_t>());
		region.regionSpriteDef = true;
	}

	if (region.regionSpriteDef)
	{
		if (!ParseDMSpriteDef2(region.regionSpriteIndex, region.regionSprite))
		{
			EQG_LOG_ERROR("Failed to parse DMSpriteDef2 ({}) for region {} ({})", region.regionSpriteIndex, wldObj.tag, objectIndex);
			return false;
		}
	}

	return true;
}

bool WLDLoader::ParseArea(uint32_t objectIndex, uint32_t areaNum, STerrainWLDData& terrain)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	BufferReader reader(wldObj.data, wldObj.size);

	WLD_OBJ_ZONE* header = reader.read_ptr<WLD_OBJ_ZONE>();
	SAreaWLDData& wldData = terrain.areas.emplace_back();
	wldData.tag = wldObj.tag;
	wldData.numRegions = header->num_regions;
	wldData.regions = reader.read_array<uint32_t>(wldData.numRegions);
	wldData.areaNum = areaNum;
	
	if (uint32_t userDataSize = reader.read<uint32_t>())
		wldData.userData = decode_s3d_string(reader.read_array<char>(userDataSize), userDataSize);

	return true;
}

bool WLDLoader::ParseWorldTree(uint32_t objectIndex, STerrainWLDData& terrain)
{
	WLDFileObject& wldObj = m_objects[objectIndex];
	BufferReader reader(wldObj.data, wldObj.size);

	WLD_OBJ_WORLDTREE* header = reader.read_ptr<WLD_OBJ_WORLDTREE>();

	SWorldTreeWLDData& worldTree = *terrain.worldTree;
	worldTree.tag = wldObj.tag;
	worldTree.nodes.resize(header->num_world_nodes);
	for (uint32_t i = 0; i < header->num_world_nodes; ++i)
	{
		WLD_OBJ_WORLDTREE_NODE* data = reader.read_ptr<WLD_OBJ_WORLDTREE_NODE>();
		SWorldTreeNodeWLDData& node = worldTree.nodes[i];

		node.plane = data->plane;
		node.region = data->region;
		node.front = data->node[0];
		node.back = data->node[1];
	}

	return true;
}

bool WLDLoader::ParseConstantAmbient(uint32_t objectIndex, STerrainWLDData& terrain)
{
	terrain.constantAmbientColor = 0;

	if (objectIndex != (uint32_t)-1 && m_objects[objectIndex].type == WLD_OBJ_CONSTANTAMBIENT_TYPE)
	{
		WLDFileObject& wldObj = m_objects[objectIndex];
		BufferReader reader(wldObj.data, wldObj.size);

		if (!reader.read(terrain.constantAmbientColor))
			return false;
	}

	return true;
}

bool WLDLoader::ParseLight(uint32_t objectIndex)
{
	WLDFileObject& wldObj = m_objects[objectIndex];

	if (wldObj.type != WLD_OBJ_POINTLIGHT_TYPE)
	{
		return false;
	}

	// Check if light definition already exists
	if (m_resourceMgr->Contains(wldObj.tag, ResourceType::LightDefinition))
	{
		return true;
	}

	WLD_OBJ_POINTLIGHT* pHeader = (WLD_OBJ_POINTLIGHT*)wldObj.data;

	uint32_t lightInstanceIdx = GetObjectIndexFromID(pHeader->light_id);
	if (lightInstanceIdx == 0)
	{
		EQG_LOG_ERROR("Light {} ({}) has invalid light id", wldObj.tag, objectIndex);
		return false;
	}

	WLD_OBJ_LIGHTINSTANCE* pLightInst = (WLD_OBJ_LIGHTINSTANCE*)m_objects[lightInstanceIdx].data;
	uint32_t lightDefinitionIdx = GetObjectIndexFromID(pLightInst->definition_id);
	if (lightDefinitionIdx == 0)
	{
		EQG_LOG_ERROR("Light {} ({}) has invalid light definition id", wldObj.tag, objectIndex);
		return false;
	}

	BufferReader reader(m_objects[lightDefinitionIdx].data, m_objects[lightDefinitionIdx].size);
	WLD_OBJ_LIGHTDEFINITION* pLightDef = reader.read_ptr<WLD_OBJ_LIGHTDEFINITION>();

	std::string_view tag = m_objects[lightDefinitionIdx].tag;

	bool skipFrames = false;
	if (pLightDef->flags & WLD_OBJ_LIGHTOPT_SKIPFRAMES)
	{
		skipFrames = true;
	}

	int currentFrame = 0;
	if (pLightDef->flags & WLD_OBJ_LIGHTOPT_HAVECURRENTFRAME)
	{
		currentFrame = reader.read<int>();
	}

	int updateInterval;
	if (pLightDef->flags & WLD_OBJ_LIGHTOPT_HAVESLEEP)
	{
		updateInterval = reader.read<int>();
	}
	else
	{
		updateInterval = 1;
	}

	float* intensityFrames = reader.read_array<float>(pLightDef->num_frames);

	glm::vec3* colorFrames = nullptr;

	if (pLightDef->flags & WLD_OBJ_LIGHTOPT_HAVECOLORS)
	{
		colorFrames = reader.read_array<glm::vec3>(pLightDef->num_frames);
	}

	std::shared_ptr<LightDefinition> lightDef = m_resourceMgr->CreateLightDefinition();

	if (!lightDef->Init(tag, pLightDef->num_frames, intensityFrames, colorFrames, currentFrame,
		updateInterval, skipFrames))
	{
		EQG_LOG_ERROR("Failed to create LightDefinition {} from LightDef {} ({})", wldObj.tag, tag, lightDefinitionIdx);
		return false;
	}

	m_resourceMgr->Add(wldObj.tag, lightDef);

	std::shared_ptr<PointLight> pointLight = m_resourceMgr->CreatePointLight(lightDef, pHeader->pos, pHeader->radius);
	m_pointLights.push_back(pointLight);

	return true;
}

} //  namespace eqg
