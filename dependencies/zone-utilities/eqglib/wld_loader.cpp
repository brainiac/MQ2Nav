
#include "pch.h"
#include "wld_loader.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_geometry.h"
#include "eqg_material.h"
#include "eqg_resource_manager.h"
#include "log_internal.h"
#include "wld_structs.h"

using namespace eqg::s3d;

namespace eqg {

static const uint8_t decoder_array[] = { 0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A };

std::string_view decode_s3d_string(char* str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		str[i] ^= decoder_array[i % 8];
	}

	return std::string_view(str, len);
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

bool WLDLoader::Init(Archive* archive, const std::string& wld_name)
{
	if (archive == nullptr || wld_name.empty())
	{
		return false;
	}

	m_archive = archive;
	m_fileName = wld_name;

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

	wld_header header;
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
	memset(m_objects.data(), 0, sizeof(S3DFileObject) * (m_numObjects + 1));

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
		obj.type = static_cast<S3DObjectType>(type);

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
	///                 ParseHierarchicalModelv            WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE
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
	/// ParsePClouds agian?
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
	
	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_BMINFO_TYPE)
		{
			if (!ParseBitmap(i))
			{
				SPDLOG_ERROR("Failed to parse bitmap: {}", m_objects[i].tag);
				return false;
			}
		}
	}

	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_MATERIALDEFINITION_TYPE)
		{
			if (!ParseMaterial(i))
			{
				SPDLOG_ERROR("Failed to parse material: {}", m_objects[i].tag);
				return false;
			}
		}
	}

	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_MATERIALPALETTE_TYPE)
		{
			if (!ParseMaterialPalette(i))
			{
				SPDLOG_ERROR("Failed to parse material palette: {}", m_objects[i].tag);
				return false;
			}
		}
	}

	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		if (m_objects[i].type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			if (!ParseTrack(i))
			{
				SPDLOG_ERROR("Failed to track instance: {}", m_objects[i].tag);
				return false;
			}
		}
	}


	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		S3DFileObject& obj = m_objects[i];

		switch (obj.type)
		{
		case WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment10(this, &obj);
			break;
		}
		case WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment11(this, &obj);
			break;
		}
		case WLD_OBJ_TRACKDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment12(this, &obj);
			break;
		}
		case WLD_OBJ_TRACKINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment13(this, &obj);
			break;
		}
		case WLD_OBJ_ACTORDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment14(this, &obj);
			break;
		}
		case WLD_OBJ_ACTORINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment15(this, &obj);
			break;
		}
		case WLD_OBJ_LIGHTDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment1B(this, &obj);
			break;
		}
		case WLD_OBJ_LIGHTINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment1C(this, &obj);
			break;
		}
		case WLD_OBJ_WORLDTREE_TYPE: {
			obj.parsed_data = new WLDFragment21(this, &obj);
			break;
		}
		case WLD_OBJ_REGION_TYPE: {
			obj.parsed_data = new WLDFragment22(this, &obj);
			break;
		}
		case WLD_OBJ_POINTLIGHT_TYPE: {
			obj.parsed_data = new WLDFragment28(this, &obj);
			break;
		}
		case WLD_OBJ_ZONE_TYPE: {
			obj.parsed_data = new WLDFragment29(this, &obj);
			break;
		}
		case WLD_OBJ_DMSPRITEINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment2D(this, &obj);
			break;
		}
		case WLD_OBJ_DMSPRITEDEFINITION2_TYPE: {
			obj.parsed_data = new WLDFragment36(this, &obj);
			break;
		}

		case WLD_OBJ_BMINFO_TYPE:
		case WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE:
		case WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE:
		case WLD_OBJ_MATERIALDEFINITION_TYPE:
		case WLD_OBJ_MATERIALPALETTE_TYPE:
			break;

		default:
			obj.parsed_data = new WLDFragment(&obj);
			break;
		}
	}

	return true;
}

bool WLDLoader::ParseObject(std::string_view tag)
{
	return false;
}

bool WLDLoader::ParseBitmap(uint32_t objectIndex)
{
	S3DFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_BMINFO_TYPE)
	{
		return false;
	}

	SPDLOG_TRACE("Parsing bitmap: {} ({})", wldObj.tag, objectIndex);

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
			for (uint32_t i = 1; i < pWLDBMInfo->num_mip_levels; ++i)
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
			SPDLOG_ERROR("Too many mip levels in bitmap {}!", wldObj.tag);
			return false;
		}
	}

	auto parsedBitmaps = std::make_unique<ParsedBMInfo>(&wldObj);
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
		if (auto existingBM = m_resourceMgr->Get<EQGBitmap>(wldData.fileName))
		{
			parsedBitmaps->bitmaps[bitmapIndex] = existingBM;
			continue;
		}

		std::shared_ptr<EQGBitmap> newBitmap = std::make_shared<EQGBitmap>();
		if (!newBitmap->InitFromWLDData(&wldData, m_archive, m_resourceMgr))
		{
			SPDLOG_ERROR("Failed to create bitmap {}!", wldData.fileName);
			return false;
		}

		newBitmap->SetType(type);

		if (!m_resourceMgr->Add(wldData.fileName, newBitmap))
		{
			SPDLOG_ERROR("Failed to add bitmap {} to resource manager!", wldData.fileName);
			return false;
		}

		parsedBitmaps->bitmaps[bitmapIndex] = newBitmap;
	}

	wldObj.parsed_data = parsedBitmaps.release();
	return true;
}

bool WLDLoader::ParseMaterial(uint32_t objectIndex)
{
	S3DFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_MATERIALDEFINITION_TYPE)
	{
		return false;
	}

	// Check if it already exists
	if (m_resourceMgr->Contains(wldObj.tag, ResourceType::Material))
	{
		return true;
	}

	SPDLOG_TRACE("Parsing material: {} ({})", wldObj.tag, objectIndex);

	BufferReader reader(wldObj.data, wldObj.size);
	auto* pWLDMaterialDef = reader.read_ptr<WLD_OBJ_MATERIALDEFINITION>();

	S3DFileObject& simpleSpriteInstanceObj = GetObjectFromID(pWLDMaterialDef->sprite_or_bminfo, objectIndex);
	WLD_OBJ_SIMPLESPRITEINSTANCE* pWLDSimpleSpriteInst = (WLD_OBJ_SIMPLESPRITEINSTANCE*)simpleSpriteInstanceObj.data;

	ParsedSimpleSpriteDef* pParsedSpriteDef = nullptr;
	std::shared_ptr<ParsedBMInfo> pParsedBMInfoForPalette;

	if (pWLDSimpleSpriteInst != nullptr)
	{
		if (simpleSpriteInstanceObj.type != WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE)
		{
			SPDLOG_ERROR("Material's sprite_or_bminfo is not a SimpleSpriteInstance: {}", wldObj.tag);
			return false;
		}

		S3DFileObject& simpleSpriteDefinitionObj = GetObjectFromID(pWLDSimpleSpriteInst->definition_id, objectIndex);
		if (simpleSpriteDefinitionObj.type != WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE)
		{
			SPDLOG_ERROR("SimpleSpriteInstance's definition isn't a SimpleSpriteDefinition!", simpleSpriteInstanceObj.tag);
			return false;
		}

		BufferReader spriteDefReader(simpleSpriteDefinitionObj.data, simpleSpriteDefinitionObj.size);

		// Only need to do this once.
		if (simpleSpriteDefinitionObj.parsed_data == nullptr)
		{
			// Parse the data for the SIMPLESPRITEDEFINITION.
			WLD_OBJ_SIMPLESPRITEDEFINITION* pWLDSpriteDef = spriteDefReader.read_ptr<WLD_OBJ_SIMPLESPRITEDEFINITION>();
			pParsedSpriteDef = new ParsedSimpleSpriteDef(&simpleSpriteDefinitionObj);
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
					auto bitmap = m_resourceMgr->Get<EQGBitmap>(GetString(bmID));
					if (!bitmap)
					{
						SPDLOG_ERROR("Failed to find bitmap {} (id={})", GetString(bmID), bmID);
						delete pParsedSpriteDef;
						return false;
					}

					pParsedBMInfo = std::make_shared<ParsedBMInfo>(&simpleSpriteDefinitionObj);
					pParsedBMInfo->bitmaps.push_back(bitmap);
				}
				else
				{
					// An object that we can look up by ID.
					S3DFileObject& bmObj = m_objects[bmID];
					if (bmObj.type != WLD_OBJ_BMINFO_TYPE)
					{
						SPDLOG_ERROR("Bitmap {} (id: {}) is not a BMINFO object!?", bmObj.tag, bmID);
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
		SPDLOG_ERROR("Failed to create material {}!", wldObj.tag);
		return false;
	}

	if (!m_resourceMgr->Add(wldObj.tag, newMaterial))
	{
		SPDLOG_ERROR("Failed to add new material to resource manager: {}", wldObj.tag);
		return false;
	}

	return true;
}

bool WLDLoader::ParseMaterialPalette(uint32_t objectIndex)
{
	S3DFileObject& wldObj = m_objects[objectIndex];
	if (wldObj.type != WLD_OBJ_MATERIALPALETTE_TYPE)
	{
		return false;
	}

	ParsedMaterialPalette* parsedPalette;

	// Check if it already exists
	if (auto existingMP = m_resourceMgr->Get<MaterialPalette>(wldObj.tag))
	{
		// re-create a ParsedPalette from this existing object.
		parsedPalette = new ParsedMaterialPalette(&wldObj);
		parsedPalette->palette = existingMP;
		wldObj.parsed_data = parsedPalette;

		return true;
	}

	SPDLOG_TRACE("Parsing material palette: {} ({})", wldObj.tag, objectIndex);

	BufferReader reader(wldObj.data, wldObj.size);

	if (wldObj.parsed_data == nullptr)
	{
		parsedPalette = new ParsedMaterialPalette(&wldObj);

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
					SPDLOG_ERROR("Failed to find material {} (id={})", GetString(materialID), materialID);
					delete parsedPalette;
					return false;
				}
				parsedPalette->materials[i] = material;
			}
			else
			{
				S3DFileObject& materialObj = m_objects[materialID];
				if (materialObj.type != WLD_OBJ_MATERIALDEFINITION_TYPE)
				{
					SPDLOG_ERROR("Material {} (id: {}) is not a MATERIALDEFINITION object!?", materialObj.tag, materialID);
					delete parsedPalette;
					return false;
				}

				auto material = m_resourceMgr->Get<Material>(materialObj.tag);
				if (!material)
				{
					SPDLOG_ERROR("Failed to find material {} (id={})", materialObj.tag, materialID);
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
		SPDLOG_ERROR("Failed to create material palette {}!", wldObj.tag);
		return false;
	}

	if (!m_resourceMgr->Add(wldObj.tag, palette))
	{
		SPDLOG_ERROR("Failed to add new material palette to resource manager: {}", wldObj.tag);
		return false;
	}

	parsedPalette->palette = palette;

	return true;
}

bool WLDLoader::ParseTrack(uint32_t objectIndex)
{
	S3DFileObject& wldObj = m_objects[objectIndex];

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
		SPDLOG_ERROR("Track instance {} has no tag!", objectIndex);
		return false;
	}

	SPDLOG_TRACE("Parsing track: {} ({})", wldObj.tag, objectIndex);

	// Create the track and set some defaults.
	std::shared_ptr<STrack> track = std::make_shared<STrack>();
	track->tag = wldObj.tag;
	track->sleepTime = 100;
	track->speed = 1.0f;
	track->reverse = false;
	track->interpolate = false;
	track->numFrames = 0;
	track->attachedToModel = !(::isalpha(wldObj.tag[0]) && ::isdigit(wldObj.tag[1]) && ::isdigit(wldObj.tag[2]));

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
	S3DFileObject& trackDefObj = GetObjectFromID(pTrackInst->track_id, objectIndex);
	if (trackDefObj.type != WLD_OBJ_TRACKDEFINITION_TYPE)
	{
		SPDLOG_ERROR("Track instance {} ({}) has invalid track definition!", wldObj.tag, objectIndex);
		return false;
	}

	if (trackDefObj.parsed_data != nullptr)
	{
		SPDLOG_ERROR("Parsed track def {} ({}) twice!", trackDefObj.tag, pTrackInst->track_id);
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

	ParsedTrackInstance* parsedTrackInst = new ParsedTrackInstance(&wldObj);
	parsedTrackInst->track = track;
	wldObj.parsed_data = parsedTrackInst;

	ParsedTrackDefinition* parsedTrackDef = new ParsedTrackDefinition(&trackDefObj);
	parsedTrackDef->track = track;
	wldObj.parsed_data = parsedTrackDef;

	return true;
}

bool WLDLoader::ParseSimpleModel(uint32_t objectIndex, std::shared_ptr<SimpleModelDefinition>& outModel)
{
	S3DFileObject& wldObj = m_objects[objectIndex];
	outModel.reset();

	if (wldObj.type != WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		return false;

	BufferReader reader(wldObj.data, wldObj.size);
	WLD_OBJ_DMSPRITEINSTANCE* pDMSpriteInst = reader.read_ptr<WLD_OBJ_DMSPRITEINSTANCE>();

	uint32_t dmSpriteDefIndex = GetObjectIndexFromID(pDMSpriteInst->definition_id);
	if (dmSpriteDefIndex <= 0)
	{
		SPDLOG_ERROR("Invalid DMSpriteDef index {} from DMSpriteInstance {} ({})",
			dmSpriteDefIndex, wldObj.tag, objectIndex);
		return false;
	}

	S3DFileObject& dmSpriteDefObj = m_objects[dmSpriteDefIndex];

	// DMSpriteDefinitions are always DMSPRITEDEF2's.
	if (dmSpriteDefObj.type != WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
	{
		SPDLOG_ERROR("definition {} is not a DMSPRITEDEF2 (it is {}) for DMSpriteInstance {} ({}) ",
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
			auto parsedData = new ParsedDMSpriteDefinition2(&wldObj);
			parsedData->simpleModelDefinition = simpleModelDef;

			dmSpriteDefObj.parsed_data = parsedData;
		}

		return true;
	}

	std::unique_ptr<SDMSpriteDef2WLDData> dmSpriteDefData;
	if (!ParseDMSpriteDef2(dmSpriteDefIndex, dmSpriteDefData))
	{
		SPDLOG_ERROR("Failed to parse DMSpriteDef2 for DMSpriteInstance {} ({})", dmSpriteDefObj.tag, objectIndex);
		return false;
	}

	auto simpleModelDef = m_resourceMgr->CreateSimpleModelDefinition();
	if (!simpleModelDef->InitFromWLDData(dmSpriteDefObj.tag, dmSpriteDefData.get()))
	{
		SPDLOG_ERROR("Failed to create SimpleModelDefinition {} from DMSpriteDef2 {} ({})",
			wldObj.tag, dmSpriteDefObj.tag, dmSpriteDefIndex);
		return false;
	}

	m_resourceMgr->Add(wldObj.tag, simpleModelDef);

	outModel = simpleModelDef;

	auto parsedData = new ParsedDMSpriteDefinition2(&wldObj);
	parsedData->simpleModelDefinition = simpleModelDef;

	dmSpriteDefObj.parsed_data = parsedData;
	return true;
}

bool WLDLoader::ParseDMSpriteDef2(uint32_t objectIndex, std::unique_ptr<SDMSpriteDef2WLDData>& pSpriteDef)
{
	S3DFileObject& wldObj = m_objects[objectIndex];

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
	S3DFileObject& materialObj = m_objects[GetObjectIndexFromID(pWLDObj->material_palette_id)];
	if (materialObj.type == WLD_OBJ_MATERIALPALETTE_TYPE)
	{
		pSpriteDef->materialPalette = static_cast<ParsedMaterialPalette*>(materialObj.parsed_data)->palette;
	}
	else
	{
		SPDLOG_ERROR("Material palette does not point to a material palette!");
		return false;
	}

	if (pWLDObj->dm_track_id)
	{
		S3DFileObject& trackInstObj = m_objects[GetObjectIndexFromID(pWLDObj->dm_track_id)];
		if (trackInstObj.type == WLD_OBJ_TRACKINSTANCE_TYPE)
		{
			pSpriteDef->trackInstance = (WLD_OBJ_TRACKINSTANCE*)trackInstObj.data;
			S3DFileObject& trackDefObj = m_objects[GetObjectIndexFromID(pSpriteDef->trackInstance->track_id)];

			pSpriteDef->trackDefinition = (WLD_OBJ_DMTRACKDEFINITION2*)trackDefObj.data;
		}
		else
		{
			SPDLOG_ERROR("Track instance in DMSpriteDef2 does not point to a track instance!");
		}
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
		pSpriteDef->centerOffset = *(glm::vec3*)&pWLDObj->center_offset;
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


} //  namespace eqg
