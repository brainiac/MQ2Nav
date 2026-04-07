#include "pch.h"
#include "eqg_material.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_resource_manager.h"
#include "eqg_structs.h"
#include "log_internal.h"
#include "wld_types.h"

namespace eqg {

//-------------------------------------------------------------------------------------------------

static std::array<uint32_t, 42> s_customRenderMethods =
{
	/* 00 */ 0x00000000,
	/* 01 */ 0x00200557,
	/* 02 */ 0x0020054B,
	/* 03 */ 0x00000057,
	/* 04 */ 0x0000004B,
	/* 05 */ 0x01280557,
	/* 06 */ 0x0124054B,
	/* 07 */ 0x002005D7,
	/* 08 */ 0x000005CB,
	/* 09 */ 0x01240557,
	/* 10 */ 0x012C0557,
	/* 11 */ 0x013B0557,
	/* 12 */ 0x00000507,
	/* 13 */ 0x40000407,
	/* 14 */ 0x40000473,
	/* 15 */ 0x41080407,
	/* 16 */ 0x411B0407,
	/* 17 */ 0x00000507,
	/* 18 */ 0x00000507,
	/* 19 */ 0x000005D7,
	/* 20 */ 0x00200473,
	/* 21 */ 0x00200473,
	/* 22 */ 0x002004F3,
	/* 23 */ 0x011B0507,
	/* 24 */ 0x01080507,
	/* 25 */ 0x00000587,
	/* 26 */ 0x00000000,
	/* 27 */ 0x00000000,
	/* 28 */ 0x00000000,
	/* 29 */ 0x00000000,
	/* 30 */ 0x00000000,
	/* 31 */ 0x40000557,
	/* 32 */ 0x4000054B,
	/* 33 */ 0x40000057,
	/* 34 */ 0x4000004B,
	/* 35 */ 0x41080557,
	/* 36 */ 0x4104054B,
	/* 37 */ 0x400005D7,
	/* 38 */ 0x400005CB,
	/* 39 */ 0x41040557,
	/* 40 */ 0x410C0557,
	/* 41 */ 0x411B0557,
};

uint32_t GetCustomRenderMethod(uint32_t customIndex)
{
	if (customIndex < s_customRenderMethods.size())
		return s_customRenderMethods[customIndex];

	return 0;
}

const char* MaterialTypeToString(EMaterialType type)
{
	switch (type)
	{
	case MaterialType_Normal: return "Normal";
	case MaterialType_SingleDetail: return "SingleDetail";
	case MaterialType_PaletteDetail: return "PaletteDetail";
	case MaterialType_LuclinLayer: return "LuclinLayer";
	case MaterialType_LuclinLayerT1: return "LuclinLayerT1";
	case MaterialType_OpaqueC1: return "OpaqueC1";
	case MaterialType_OpaqueCG1: return "OpaqueCG1";
	case MaterialType_OpaqueCE1: return "OpaqueCE1";
	case MaterialType_OpaqueCB1: return "OpaqueCB1";
	case MaterialType_OpaqueCBS1: return "OpaqueCBS1";
	case MaterialType_OpaqueCBS1_VSB: return "OpaqueCBS1_VSB";
	case MaterialType_OpaqueCBS_2UV: return "OpaqueCBS_2UV";
	case MaterialType_OpaqueCBSG1: return "OpaqueCBSG1";
	case MaterialType_OpaqueCBSGE1: return "OpaqueCBSGE1";
	case MaterialType_OpaqueC1_2UV: return "OpaqueC1_2UV";
	case MaterialType_OpaqueCB1_2UV: return "OpaqueCB1_2UV";
	case MaterialType_OpaqueCBSG1_2UV: return "OpaqueCBSG1_2UV";
	case MaterialType_OpaqueCBST2_2UV: return "OpaqueCBST2_2UV";
	case MaterialType_OpaqueTerrain: return "OpaqueTerrain";
	case MaterialType_OpaqueLava: return "OpaqueLava";
	case MaterialType_OpaqueLava2: return "OpaqueLava2";
	case MaterialType_OpaqueBasic: return "OpaqueBasic";
	case MaterialType_OpaqueBlend: return "OpaqueBlend";
	case MaterialType_OpaqueBlendNoBump: return "OpaqueBlendNoBump";
	case MaterialType_OpaqueFull: return "OpaqueFull";
	case MaterialType_OpaqueFull_2UV: return "OpaqueFull_2UV";
	case MaterialType_OpaqueBump: return "OpaqueBump";
	case MaterialType_OpaqueBump_2UV: return "OpaqueBump_2UV";
	case MaterialType_OpaqueSB: return "OpaqueSB";
	case MaterialType_OpaqueSB_2UV: return "OpaqueSB_2UV";
	case MaterialType_OpaqueGB: return "OpaqueGB";
	case MaterialType_OpaqueGB_2UV: return "OpaqueGB_2UV";
	case MaterialType_OpaqueRB: return "OpaqueRB";
	case MaterialType_OpaqueRB_2UV: return "OpaqueRB_2UV";
	case MaterialType_ChromaC1: return "ChromaC1";
	case MaterialType_ChromaCG1: return "ChromaCG1";
	case MaterialType_ChromaCE1: return "ChromaCE1";
	case MaterialType_ChromaCB1: return "ChromaCB1";
	case MaterialType_ChromaCBS1: return "ChromaCBS1";
	case MaterialType_ChromaCBS1_VSB: return "ChromaCBS1_VSB";
	case MaterialType_ChromaCBSG1: return "ChromaCBSG1";
	case MaterialType_ChromaCBSGE1: return "ChromaCBSGE1";
	case MaterialType_ChromaBasic: return "ChromaBasic";
	case MaterialType_ChromaBump: return "ChromaBump";
	case MaterialType_AlphaC1: return "AlphaC1";
	case MaterialType_AlphaCG1: return "AlphaCG1";
	case MaterialType_AlphaCE1: return "AlphaCE1";
	case MaterialType_AlphaCB1: return "AlphaCB1";
	case MaterialType_AlphaCBS1: return "AlphaCBS1";
	case MaterialType_AlphaCBSG1: return "AlphaCBSG1";
	case MaterialType_AlphaCBSGE1: return "AlphaCBSGE1";
	case MaterialType_AlphaBasic: return "AlphaBasic";
	case MaterialType_AlphaBump: return "AlphaBump";
	case MaterialType_AlphaWater: return "AlphaWater";
	case MaterialType_AlphaWaterFall: return "AlphaWaterFall";
	case MaterialType_AlphaLavaH: return "AlphaLavaH";
	case MaterialType_AddAlphaC1: return "AddAlphaC1";
	case MaterialType_AddAlphaCG1: return "AddAlphaCG1";
	case MaterialType_AddAlphaCE1: return "AddAlphaCE1";
	case MaterialType_AddAlphaCB1: return "AddAlphaCB1";
	case MaterialType_AddAlphaCBS1: return "AddAlphaCBS1";
	case MaterialType_AddAlphaCBSG1: return "AddAlphaCBSG1";
	case MaterialType_AddAlphaCBSGE1: return "AddAlphaCBSGE1";
	default: return "Unknown";
	}
}

const char* RenderMaterialToString(ERenderMaterial type)
{
	switch (type)
	{
	case RenderMaterial_Opaque: return "Opaque";
	case RenderMaterial_Chroma: return "Chroma";
	case RenderMaterial_AlphaSingleDetail: return "AlphaSingleDetail";
	case RenderMaterial_AlphaPaletteDetail: return "AlphaPaletteDetail";
	case RenderMaterial_AlphaBatchAdditive: return "AlphaBatchAdditive";
	case RenderMaterial_AlphaBatch: return "AlphaBatch";
	case RenderMaterial_OpaqueC1: return "OpaqueC1";
	case RenderMaterial_OpaqueCG1: return "OpaqueCG1";
	case RenderMaterial_OpaqueCE1: return "OpaqueCE1";
	case RenderMaterial_OpaqueCB1: return "OpaqueCB1";
	case RenderMaterial_OpaqueCBS1: return "OpaqueCBS1";
	case RenderMaterial_OpaqueCBS1_VSB: return "OpaqueCBS1_VSB";
	case RenderMaterial_OpaqueCBS_2UV: return "OpaqueCBS_2UV";
	case RenderMaterial_OpaqueCBSG1: return "OpaqueCBSG1";
	case RenderMaterial_OpaqueCBSGE1: return "OpaqueCBSGE1";
	case RenderMaterial_OpaqueC1_2UV: return "OpaqueC1_2UV";
	case RenderMaterial_OpaqueCB1_2UV: return "OpaqueCB1_2UV";
	case RenderMaterial_OpaqueCBSG1_2UV: return "OpaqueCBSG1_2UV";
	case RenderMaterial_OpaqueCBST2_2UV: return "OpaqueCBST2_2UV";
	case RenderMaterial_OpaqueTerrain: return "OpaqueTerrain";
	case RenderMaterial_OpaqueLava: return "OpaqueLava";
	case RenderMaterial_OpaqueLava2: return "OpaqueLava2";
	case RenderMaterial_OpaqueBasic: return "OpaqueBasic";
	case RenderMaterial_OpaqueBlend: return "OpaqueBlend";
	case RenderMaterial_OpaqueBlendNoBump: return "OpaqueBlendNoBump";
	case RenderMaterial_OpaqueFull: return "OpaqueFull";
	case RenderMaterial_OpaqueFull_2UV: return "OpaqueFull_2UV";
	case RenderMaterial_OpaqueBump: return "OpaqueBump";
	case RenderMaterial_OpaqueBump_2UV: return "OpaqueBump_2UV";
	case RenderMaterial_OpaqueSB: return "OpaqueSB";
	case RenderMaterial_OpaqueSB_2UV: return "OpaqueSB_2UV";
	case RenderMaterial_OpaqueGB: return "OpaqueGB";
	case RenderMaterial_OpaqueGB_2UV: return "OpaqueGB_2UV";
	case RenderMaterial_OpaqueRB: return "OpaqueRB";
	case RenderMaterial_OpaqueRB_2UV: return "OpaqueRB_2UV";
	case RenderMaterial_ChromaC1: return "ChromaC1";
	case RenderMaterial_ChromaCG1: return "ChromaCG1";
	case RenderMaterial_ChromaCE1: return "ChromaCE1";
	case RenderMaterial_ChromaCB1: return "ChromaCB1";
	case RenderMaterial_ChromaCBS1: return "ChromaCBS1";
	case RenderMaterial_ChromaCBSG1: return "ChromaCBSG1";
	case RenderMaterial_ChromaCBSGE1: return "ChromaCBSGE1";
	case RenderMaterial_ChromaBasic: return "ChromaBasic";
	case RenderMaterial_ChromaBump: return "ChromaBump";
	case RenderMaterial_AlphaC1: return "AlphaC1";
	case RenderMaterial_AlphaCG1: return "AlphaCG1";
	case RenderMaterial_AlphaCE1: return "AlphaCE1";
	case RenderMaterial_AlphaCB1: return "AlphaCB1";
	case RenderMaterial_AlphaCBS1: return "AlphaCBS1";
	case RenderMaterial_AlphaCBSG1: return "AlphaCBSG1";
	case RenderMaterial_AlphaCBSGE1: return "AlphaCBSGE1";
	case RenderMaterial_AlphaBasic: return "AlphaBasic";
	case RenderMaterial_AlphaBump: return "AlphaBump";
	case RenderMaterial_AlphaWater: return "AlphaWater";
	case RenderMaterial_AlphaWaterFall: return "AlphaWaterFall";
	case RenderMaterial_AlphaLavaH: return "AlphaLavaH";
	case RenderMaterial_AddAlphaC1: return "AddAlphaC1";
	case RenderMaterial_AddAlphaCG1: return "AddAlphaCG1";
	case RenderMaterial_AddAlphaCE1: return "AddAlphaCE1";
	case RenderMaterial_AddAlphaCB1: return "AddAlphaCB1";
	case RenderMaterial_AddAlphaCBS1: return "AddAlphaCBS1";
	case RenderMaterial_AddAlphaCBSG1: return "AddAlphaCBSG1";
	case RenderMaterial_AddAlphaCBSGE1: return "AddAlphaCBSGE1";
	default: return "Unknown";
	}
}

//-------------------------------------------------------------------------------------------------

Bitmap::Bitmap()
	: Resource(ResourceType::Bitmap)
{
}

Bitmap::~Bitmap()
{
}

bool Bitmap::InitFromWLDData(SBitmapWLDData* wldData, Archive* archive)
{
	m_fileName = wldData->fileName;
	m_detailScale = wldData->detailScale;
	m_grassDensity = wldData->grassDensity;
	m_objectIndex = wldData->objectIndex;

	ResourceManager* resourceMgr = ResourceManager::Get();

	if (wldData->createTexture)
	{
		if (!resourceMgr->LoadTexture(this, archive))
		{
			EQG_LOG_ERROR("Failed to create texture for {}", m_fileName);
			return false;
		}
		m_hasTexture = true;
	}
	else
	{
		if (!resourceMgr->LoadBitmapData(this, archive))
		{
			EQG_LOG_ERROR("Failed to load bitmap data for {}", m_fileName);
			return false;
		}
	}

	return true;
}

bool Bitmap::Init(std::string_view fileName, Archive* archive, bool cubeMap, bool rawData)
{
	m_fileName = fileName;
	m_detailScale = 1.0f;
	m_grassDensity = 0;
	m_cubeMap = cubeMap;
	m_storeRawData = rawData;
	m_flip = rawData;
	m_hasTexture = ResourceManager::Get()->LoadTexture(this, archive);

	return true;
}

//-------------------------------------------------------------------------------------------------

Material::Material()
	: Resource(ResourceType::Material)
{
}

Material::~Material()
{
}

static EMaterialType ParseEffectNameToMaterialType(const std::string& m_effectName, const std::string& archiveName)
{
	EMaterialType type = MaterialType_Normal;

	// Identify material type from effect name
	if (strstr(m_effectName.c_str(), "WaterFall") != nullptr)
		type = MaterialType_AlphaWaterFall;
	else if (strstr(m_effectName.c_str(), "Water") != nullptr)
	{
		type = MaterialType_AlphaWater;

		// Lol, hacks
		if (ci_equals(archiveName, "harbingers.eqg"))
		{
			type = MaterialType_AlphaLavaH;
		}
	}
	else if (strstr(m_effectName.c_str(), "Terrain") != nullptr)
		type = MaterialType_OpaqueTerrain;
	else if (strstr(m_effectName.c_str(), "Lava2") != nullptr)
		type = MaterialType_OpaqueLava2;
	else if (strstr(m_effectName.c_str(), "Lava") != nullptr)
		type = MaterialType_OpaqueLava;
	else if (strstr(m_effectName.c_str(), "AddAlpha") != nullptr)
	{
		if (strstr(m_effectName.c_str(), "CBSGE1") != nullptr)
			type = MaterialType_AddAlphaCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBGG1") != nullptr)
			type = MaterialType_AddAlphaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBSG1") != nullptr)
			type = MaterialType_AddAlphaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBS1") != nullptr)
			type = MaterialType_AddAlphaCBS1;
		else if (strstr(m_effectName.c_str(), "CB1") != nullptr)
			type = MaterialType_AddAlphaCB1;
		else if (strstr(m_effectName.c_str(), "CE1") != nullptr)
			type = MaterialType_AddAlphaCE1;
		else if (strstr(m_effectName.c_str(), "CG1") != nullptr)
			type = MaterialType_AddAlphaCG1;
		else
			type = MaterialType_AddAlphaC1;
	}
	else if (strstr(m_effectName.c_str(), "Alpha") != nullptr)
	{
		if (strstr(m_effectName.c_str(), "MPLBasic") != nullptr)
			type = MaterialType_AlphaBasic;
		else if (strstr(m_effectName.c_str(), "MPLBump") != nullptr)
			type = MaterialType_AlphaBump;
		else if (strstr(m_effectName.c_str(), "CBSGE1") != nullptr)
			type = MaterialType_AlphaCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBGG1") != nullptr)
			type = MaterialType_AlphaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBSG1") != nullptr)
			type = MaterialType_AlphaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBS1") != nullptr)
			type = MaterialType_AlphaCBS1;
		else if (strstr(m_effectName.c_str(), "CB1") != nullptr)
			type = MaterialType_AlphaCB1;
		else if (strstr(m_effectName.c_str(), "CE1") != nullptr)
			type = MaterialType_AlphaCE1;
		else if (strstr(m_effectName.c_str(), "CG1") != nullptr)
			type = MaterialType_AlphaCG1;
		else
			type = MaterialType_AlphaC1;
	}
	else if (strstr(m_effectName.c_str(), "Chroma") != nullptr)
	{
		if (strstr(m_effectName.c_str(), "MPLBasic") != nullptr)
			type = MaterialType_ChromaBasic;
		else if (strstr(m_effectName.c_str(), "MPLBump") != nullptr)
			type = MaterialType_ChromaBump;
		else if (strstr(m_effectName.c_str(), "CBSGE1") != nullptr)
			type = MaterialType_ChromaCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBGG1") != nullptr)
			type = MaterialType_ChromaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBSG1") != nullptr)
			type = MaterialType_ChromaCBSG1;
		else if (strstr(m_effectName.c_str(), "CBS1") != nullptr && strstr(m_effectName.c_str(), "VSB") == nullptr)
			type = MaterialType_ChromaCBS1;
		else if (strstr(m_effectName.c_str(), "VSB") != nullptr)
			type = MaterialType_ChromaCBS1_VSB;
		else if (strstr(m_effectName.c_str(), "CB1") != nullptr)
			type = MaterialType_ChromaCB1;
		else if (strstr(m_effectName.c_str(), "CE1") != nullptr)
			type = MaterialType_ChromaCE1;
		else if (strstr(m_effectName.c_str(), "CG1") != nullptr)
			type = MaterialType_ChromaCG1;
		else
			type = MaterialType_ChromaC1;
	}
	else
	{
		if (strstr(m_effectName.c_str(), "MPLBasic") != nullptr)
			type = MaterialType_OpaqueBasic;
		else if (strstr(m_effectName.c_str(), "MPLBlendNoBump") != nullptr)
			type = MaterialType_OpaqueBlendNoBump;
		else if (strstr(m_effectName.c_str(), "MPLBlend") != nullptr)
			type = MaterialType_OpaqueBlend;
		else if (strstr(m_effectName.c_str(), "MPLFull2UV") != nullptr)
			type = MaterialType_OpaqueFull_2UV;
		else if (strstr(m_effectName.c_str(), "MPLFull") != nullptr)
			type = MaterialType_OpaqueFull;
		else if (strstr(m_effectName.c_str(), "MPLBump2UV") != nullptr)
			type = MaterialType_OpaqueBump_2UV;
		else if (strstr(m_effectName.c_str(), "MPLBump") != nullptr)
			type = MaterialType_OpaqueBump;
		else if (strstr(m_effectName.c_str(), "MPLSB2UV") != nullptr)
			type = MaterialType_OpaqueSB_2UV;
		else if (strstr(m_effectName.c_str(), "MPLSB") != nullptr)
			type = MaterialType_OpaqueSB;
		else if (strstr(m_effectName.c_str(), "MPLGB2UV") != nullptr)
			type = MaterialType_OpaqueGB_2UV;
		else if (strstr(m_effectName.c_str(), "MPLGB") != nullptr)
			type = MaterialType_OpaqueGB;
		else if (strstr(m_effectName.c_str(), "MPLRB2UV") != nullptr)
			type = MaterialType_OpaqueRB_2UV;
		else if (strstr(m_effectName.c_str(), "MPLRB") != nullptr)
			type = MaterialType_OpaqueRB;
		else if (strstr(m_effectName.c_str(), "C1DTP") != nullptr)
			type = MaterialType_PaletteDetail;
		else if (strstr(m_effectName.c_str(), "CBSG1_2UV") != nullptr)
			type = MaterialType_OpaqueCBSG1_2UV;
		else if (strstr(m_effectName.c_str(), "CBST2_2UV") != nullptr)
			type = MaterialType_OpaqueCBST2_2UV;
		else if (strstr(m_effectName.c_str(), "CB1_2UV") != nullptr)
			type = MaterialType_OpaqueCB1_2UV;
		else if (strstr(m_effectName.c_str(), "C1_2UV") != nullptr)
			type = MaterialType_OpaqueC1_2UV;
		else if (strstr(m_effectName.c_str(), "CBGGE1") != nullptr)
			type = MaterialType_OpaqueCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBSGE1") != nullptr)
			type = MaterialType_OpaqueCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBSE1") != nullptr)
			type = MaterialType_OpaqueCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBE1") != nullptr)
			type = MaterialType_OpaqueCBSGE1;
		else if (strstr(m_effectName.c_str(), "CBGG1") != nullptr)
			type = MaterialType_OpaqueCBSG1;
		else if (strstr(m_effectName.c_str(), "CBSG1") != nullptr)
			type = MaterialType_OpaqueCBSG1;
		else if (strstr(m_effectName.c_str(), "CBS1") != nullptr && strstr(m_effectName.c_str(), "VSB") == nullptr)
			type = MaterialType_OpaqueCBS1;
		else if (strstr(m_effectName.c_str(), "VSB") != nullptr)
			type = MaterialType_OpaqueCBS1_VSB;
		else if (strstr(m_effectName.c_str(), "CBS_2UV") != nullptr)
			type = MaterialType_OpaqueCBS_2UV;
		else if (strstr(m_effectName.c_str(), "CB1") != nullptr)
			type = MaterialType_OpaqueCB1;
		else if (strstr(m_effectName.c_str(), "CE1") != nullptr)
			type = MaterialType_OpaqueCE1;
		else if (strstr(m_effectName.c_str(), "CG1") != nullptr)
			type = MaterialType_OpaqueCG1;
		else
			type = MaterialType_OpaqueC1;
	}

	return type;
}

static std::vector<std::string> ParseTextureDatafile(Archive* archive, std::string_view filename)
{
	char info_name[256];
	strncpy_s(info_name, filename.data(), filename.size());
	info_name[strlen(info_name) - 3] = 0;
	strcat_s(info_name, "txt");
	std::vector<char> anim_info_buffer;

	if (archive->Get(info_name, anim_info_buffer))
	{
		struct ReadBuf : std::streambuf
		{
			ReadBuf(std::vector<char>& buf)
			{
				setg(buf.data(), buf.data(), buf.data() + buf.size());
			}
		};
		ReadBuf rb(anim_info_buffer);
		std::istream istr(&rb);
		std::vector<std::string> lines;

		// Split anim_info_buffer into lines
		for (std::string line; std::getline(istr, line);)
			lines.push_back(line);
	
		return lines;
	}

	return {};
}

struct TextureExtraInfo
{
	std::string fileName;
	std::vector<std::string> lines;
};

void Material::InitFromEQMData(SEQMMaterial* eqm_material, SEQMFXParameter* eqm_fx_params, Archive* archive, const char* string_pool)
{
	ResourceManager* resourceMgr = ResourceManager::Get();

	// Initialize a material from EQG model data

	m_tag = string_pool + eqm_material->name_index;
	m_effectName = string_pool + eqm_material->effect_name_index;
	EMaterialType materialType = ParseEffectNameToMaterialType(m_effectName, archive->GetFileName());

	m_textureSet = std::make_unique<STextureSet>();
	m_textureSet->updateInterval = std::chrono::milliseconds(0);
	uint32_t renderMethod = 0x80000002;

	SetTextureSlot(m_tag);
	SetEQMRenderMaterial(renderMethod, materialType);

	std::vector<TextureExtraInfo> extraTextureInfo;

	uint32_t num_effect_params = eqm_material->num_params;
	int max_frames = 1;

	if (m_type != MaterialType_PaletteDetail && archive != nullptr)
	{
		for (uint32_t param_index = 0; param_index < num_effect_params; ++param_index)
		{
			if (eqm_fx_params[param_index].type == eEQMFXParameterTexture)
			{
				std::string_view filename = string_pool + eqm_fx_params[param_index].n_value;

				std::vector<std::string> lines = ParseTextureDatafile(archive, filename);

				// Parse lines;
				if (!lines.empty())
				{
					int num_frames = str_to_int(lines[0], 0);
					max_frames = std::max(num_frames, max_frames);
					if (lines.size() > 1)
						m_textureSet->updateInterval = std::chrono::milliseconds(str_to_int(lines[1], 0));

					// Save this off for later use.
					extraTextureInfo.emplace_back(std::string(filename), std::move(lines));
				}
			}
		}
	}

	m_textureSet->textures.resize(max_frames);

	if (m_type == MaterialType_PaletteDetail)
	{
		std::unique_ptr<DetailPaletteInfo> detailPalette = std::make_unique<DetailPaletteInfo>();
		detailPalette->material = this;

		uint32_t bitmapCount = 0;
		uint32_t detailCount = 0;

		for (uint32_t param_index = 0; param_index < num_effect_params; ++param_index)
		{
			SEQMFXParameter* in_param = eqm_fx_params + param_index;
			const char* paramName = string_pool + in_param->name_index;

			switch (in_param->type)
			{
			case eEQMFXParameterUnused:
				if (ci_starts_with(paramName, "e_fScale"))
				{
					int value = str_to_int(paramName + 8, 0);
					detailPalette->detailInfo[value].scaleFactor = static_cast<int>(in_param->f_value);
				}
				else if (ci_starts_with(paramName, "e_fGrassDensity"))
				{
					int value = str_to_int(paramName + 15, 0);
					detailPalette->detailInfo[value].grassDensity = static_cast<int>(in_param->f_value);
				}
				break;

			case eEQMFXParameterTexture:
			{
				const char* fileName = string_pool + in_param->n_value;
				std::shared_ptr<Bitmap> bitmap;

				if (!starts_with(fileName, "None"))
				{
					if (bitmapCount == 1)
					{
						bitmap = resourceMgr->CreateBitmap(fileName, archive, false, true);
					}
					else
					{
						bitmap = resourceMgr->CreateBitmap(fileName, archive);
					}
				}

				if (bitmap)
				{
					if (bitmapCount == 0)
					{
						m_textureSet->textures[0].flags = 0;
						m_textureSet->textures[0].filename = string_pool + eqm_material->effect_name_index;
						m_textureSet->textures[0].textures[0] = bitmap;
					}
					else if (bitmapCount == 1)
					{
						detailPalette->width = bitmap->GetWidth();
						detailPalette->height = bitmap->GetHeight();
						detailPalette->paletteData = bitmap->GetRawData();
						detailPalette->paletteFileName = fileName;
					}
					else
					{
						std::shared_ptr<Material> materialPtr = std::make_shared<Material>();
						materialPtr->InitFromBitmap(bitmap);

						int value = str_to_int(paramName + 15, 0);
						detailPalette->detailInfo[value].material = materialPtr;
						detailPalette->detailInfo[value].fileName = fileName;
						++detailCount;
					}
				}
			}
			++bitmapCount;
			break;
			}
		}

		detailPalette->numDetails = detailCount;
		m_detailPalette = std::move(detailPalette);
	}
	else
	{
		m_effectParams.resize(num_effect_params);
		uint32_t currentBitmap = 0;

		for (uint32_t fx_param = 0; fx_param < num_effect_params; ++fx_param)
		{
			SFXParameter& param = m_effectParams[fx_param];
			SEQMFXParameter* in_param = eqm_fx_params + fx_param;

			param.name = string_pool + in_param->name_index;

			switch (in_param->type)
			{
			case eEQMFXParameterUnused:
				if (param.name == "e_fRenderType")
				{
					param.type = FXParameterType_Unused;
				}
				else if (param.name == "e_fEnvMapStrength0")
				{
					param.type = FXParameterType_Float;
					param.f_value = in_param->f_value * 2.0f;
				}
				else
				{
					param.type = FXParameterType_Float;
					param.f_value = in_param->f_value;
				}
				break;

			case eEQMFXParameterInt:
				param.type = FXParameterType_Int;
				param.n_value = in_param->n_value;
				break;

			case eEQMFXParameterColor:
				param.type = FXParameterType_Color;
				param.n_value = in_param->n_value;
				break;

			case eEQMFXParameterTexture:
			{
				param.type = FXParameterType_Texture;

				// this is where we load the textures for the material
				std::string_view filename = string_pool + in_param->n_value;
				BitmapPtr bitmap;

				if (filename != "None")
				{
					bool isEnvironmentMap = find_substr(filename, "Environment") != -1;

					if (m_type == MaterialType_AlphaWater && isEnvironmentMap)
					{
						// something about sky/cubemap
					}
					else if (m_type == MaterialType_AlphaLavaH && isEnvironmentMap)
					{
						bitmap = resourceMgr->CreateBitmap(filename, archive, true);
					}
					else
					{
						bitmap = resourceMgr->CreateBitmap(filename, archive, false);
					}
				}

				for (uint32_t index = 0; index < m_textureSet->textures.size(); ++index)
				{
					if (currentBitmap == 0)
					{
						m_textureSet->textures[index].flags = 0;
						m_textureSet->textures[index].filename = eqm_material->effect_name_index;
					}

					if (bitmap)
					{
						m_textureSet->textures[index].textures[currentBitmap] = bitmap;
					}
					else
					{
						m_textureSet->textures[index].textures[currentBitmap].reset();
					}
				}

				if (bitmap)
				{
					for (const TextureExtraInfo& extraInfo : extraTextureInfo)
					{
						if (extraInfo.fileName == filename)
						{
							for (size_t line = 3; line < extraInfo.lines.size(); ++line)
							{
								int frame = static_cast<int>(line) - 2;
								std::string_view bitmapName = extraInfo.lines[frame];

								m_textureSet->textures[frame].textures[currentBitmap] = resourceMgr->CreateBitmap(trim(bitmapName), archive, false);
							}
						}
					}
				}

				param.index = currentBitmap++;
				break;
			}

			default:
				EQG_LOG_ERROR("Invalid effect parameter type: {}", static_cast<int>(in_param->type));
				break;
			}
		}
	}
}

uint32_t Material::GetRenderMethod() const
{
	uint32_t renderMethod = m_renderMethod;

	if (renderMethod & RM_CUSTOM_MASK)
	{
		renderMethod = GetCustomRenderMethod(renderMethod & ~RM_CUSTOM_MASK);
	}

	return renderMethod;
}

void Material::SetWLDRenderMaterial(uint32_t renderMethod, EMaterialType materialType)
{
	m_renderMethod = renderMethod;
	m_type = materialType;

	switch (m_type)
	{
	case MaterialType_Normal:
		{
			renderMethod = GetRenderMethod();

			if ((m_renderMethod & RM_CUSTOM_MASK) == 0
				&& (m_renderMethod & RM_TEXTURE_MASK) == 0
				&& (m_renderMethod & RM_TRANSLUCENCY_MASK) == 0)
			{
				renderMethod = 0;
			}

			if ((m_renderMethod & RM_FILL_MASK) == RM_FILL_TRANSPARENT)
			{
				m_transparent = true;
			}
			else if (renderMethod & RM_TRANSPARENCY_MASK)
			{
				m_renderMaterial = RenderMaterial_Chroma;
			}
			else if (renderMethod & RM_TRANSLUCENCY_MASK)
			{
				m_alpha = ((renderMethod & RM_TRANSLUCENCY_LEVEL_MASK) >> RM_TRANSLUCENCY_LEVEL_SHIFT) * 16;

				m_renderMaterial = (renderMethod & RM_ADDITIVE_LIGHT_MASK) ? RenderMaterial_AlphaBatchAdditive : RenderMaterial_AlphaBatch;
			}
			else
			{
				m_renderMaterial = RenderMaterial_Opaque;
			}
		}
		break;

	case MaterialType_PaletteDetail:
	case MaterialType_SingleDetail:
	case MaterialType_LuclinLayer:
	case MaterialType_LuclinLayerT1:
		m_renderMaterial = RenderMaterial_Opaque;
		break;

	default:
		EQG_LOG_INFO("Unsupported material type: {}", static_cast<int>(m_type));
	}
}

void Material::SetEQMRenderMaterial(uint32_t renderMethod, EMaterialType materialType)
{
	m_renderMethod = renderMethod;
	m_type = materialType;

	switch (m_type)
	{
	case MaterialType_Normal:
		{
			renderMethod = GetRenderMethod();

			if (renderMethod == RM_FILL_TRANSPARENT)
			{
				m_transparent = true;
			}
			else if (renderMethod & RM_TRANSPARENCY_MASK)
			{
				m_renderMaterial = RenderMaterial_Chroma;
			}
			else if (renderMethod & RM_TRANSLUCENCY_MASK)
			{
				m_alpha = ((renderMethod & RM_TRANSLUCENCY_LEVEL_MASK) >> RM_TRANSLUCENCY_LEVEL_SHIFT) * 16;

				m_renderMaterial = (renderMethod & RM_ADDITIVE_LIGHT_MASK) ? RenderMaterial_AlphaBatchAdditive : RenderMaterial_AlphaBatch;
			}
			else
			{
				m_renderMaterial = RenderMaterial_Opaque;
			}
		}
		break;

	case MaterialType_PaletteDetail:
	case MaterialType_SingleDetail:
		m_renderMaterial = RenderMaterial_Opaque;
		break;
	case MaterialType_OpaqueC1:
		m_renderMaterial = RenderMaterial_OpaqueC1;
		break;
	case MaterialType_OpaqueCG1:
		m_renderMaterial = RenderMaterial_OpaqueCG1;
		break;
	case MaterialType_OpaqueCE1:
		m_renderMaterial = RenderMaterial_OpaqueCE1;
		break;
	case MaterialType_OpaqueCB1:
		m_renderMaterial = RenderMaterial_OpaqueCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBS1:
		m_renderMaterial = RenderMaterial_OpaqueCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBS1_VSB:
		m_renderMaterial = RenderMaterial_OpaqueCBS1_VSB;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueCBS_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCBS_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueCBSG1:
		m_renderMaterial = RenderMaterial_OpaqueCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBSGE1:
		m_renderMaterial = RenderMaterial_OpaqueCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueC1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueC1_2UV;
		break;
	case MaterialType_OpaqueCB1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCB1_2UV;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBSG1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCBSG1_2UV;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBST2_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCBST2_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		m_hasVertexTint2 = true;
		break;
	case MaterialType_OpaqueTerrain:
		m_renderMaterial = RenderMaterial_OpaqueTerrain;
		break;
	case MaterialType_OpaqueLava:
		m_renderMaterial = RenderMaterial_OpaqueLava;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueLava2:
		m_renderMaterial = RenderMaterial_OpaqueLava2;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueBasic:
		m_renderMaterial = RenderMaterial_OpaqueBasic;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueBlend:
		m_renderMaterial = RenderMaterial_OpaqueBlend;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueBlendNoBump:
		m_renderMaterial = RenderMaterial_OpaqueBlendNoBump;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueFull:
		m_renderMaterial = RenderMaterial_OpaqueFull;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueFull_2UV:
		m_renderMaterial = RenderMaterial_OpaqueFull_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueBump:
		m_renderMaterial = RenderMaterial_OpaqueBump;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueBump_2UV:
		m_renderMaterial = RenderMaterial_OpaqueBump_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueSB:
		m_renderMaterial = RenderMaterial_OpaqueSB;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueSB_2UV:
		m_renderMaterial = RenderMaterial_OpaqueSB_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueGB:
		m_renderMaterial = RenderMaterial_OpaqueGB;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueGB_2UV:
		m_renderMaterial = RenderMaterial_OpaqueGB_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueRB:
		m_renderMaterial = RenderMaterial_OpaqueRB;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_OpaqueRB_2UV:
		m_renderMaterial = RenderMaterial_OpaqueRB_2UV;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_ChromaC1:
		m_renderMaterial = RenderMaterial_ChromaC1;
		break;
	case MaterialType_ChromaCG1:
		m_renderMaterial = RenderMaterial_ChromaCG1;
		break;
	case MaterialType_ChromaCE1:
		m_renderMaterial = RenderMaterial_ChromaCE1;
		break;
	case MaterialType_ChromaCB1:
		m_renderMaterial = RenderMaterial_ChromaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBS1:
		m_renderMaterial = RenderMaterial_ChromaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBSG1:
		m_renderMaterial = RenderMaterial_ChromaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBSGE1:
		m_renderMaterial = RenderMaterial_ChromaCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaBasic:
		m_renderMaterial = RenderMaterial_ChromaBasic;
		m_hasVertexTint = true;
		break;
	case MaterialType_ChromaBump:
		m_renderMaterial = RenderMaterial_ChromaBump;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_AlphaC1:
		m_renderMaterial = RenderMaterial_AlphaC1;
		break;
	case MaterialType_AlphaCG1:
		m_renderMaterial = RenderMaterial_AlphaCG1;
		break;
	case MaterialType_AlphaCE1:
		m_renderMaterial = RenderMaterial_AlphaCE1;
		break;
	case MaterialType_AlphaCB1:
		m_renderMaterial = RenderMaterial_AlphaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBS1:
		m_renderMaterial = RenderMaterial_AlphaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBSG1:
		m_renderMaterial = RenderMaterial_AlphaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBSGE1:
		m_renderMaterial = RenderMaterial_AlphaCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaBasic:
		m_renderMaterial = RenderMaterial_AlphaBasic;
		m_hasVertexTint = true;
		break;
	case MaterialType_AlphaBump:
		m_renderMaterial = RenderMaterial_AlphaBump;
		m_hasBumpMap = true;
		m_hasVertexTint = true;
		break;
	case MaterialType_AlphaWater:
		m_renderMaterial = RenderMaterial_AlphaWater;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaWaterFall:
		m_renderMaterial = RenderMaterial_AlphaWaterFall;
		break;
	case MaterialType_AlphaLavaH:
		m_renderMaterial = RenderMaterial_AlphaLavaH;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaC1:
		m_renderMaterial = RenderMaterial_AddAlphaC1;
		break;
	case MaterialType_AddAlphaCG1:
		m_renderMaterial = RenderMaterial_AddAlphaCG1;
		break;
	case MaterialType_AddAlphaCE1:
		m_renderMaterial = RenderMaterial_AddAlphaCE1;
		break;
	case MaterialType_AddAlphaCB1:
		m_renderMaterial = RenderMaterial_AddAlphaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBS1:
		m_renderMaterial = RenderMaterial_AddAlphaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBSG1:
		m_renderMaterial = RenderMaterial_AddAlphaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBSGE1:
		m_renderMaterial = RenderMaterial_AddAlphaCBSGE1;
		m_hasBumpMap = true;
		break;

	default:
		EQG_LOG_ERROR("Invalid Material Type: {}", static_cast<int>(m_type));
		break;
	}
}

bool Material::InitFromWLDData(
	std::string_view tag,
	WLD_OBJ_MATERIALDEFINITION* pWLDMaterialDef,
	WLD_OBJ_SIMPLESPRITEINSTANCE* pSimpleSpriteInst,
	ParsedSimpleSpriteDef* pParsedSimpleSpriteDef,
	ParsedBMInfo* pParsedBMPalette,
	const glm::vec2& uvShiftPerMs)
{
	m_tag = tag;
	SetTextureSlot(tag);

	if (pWLDMaterialDef->flags & WLD_OBJ_MATOPT_TWOSIDED)
	{
		m_twoSided = true;
	}
	if (pWLDMaterialDef->flags & WLD_OBJ_MATOPT_HASUVSHIFTPERMS)
	{
		m_uvShiftPerMS = uvShiftPerMs;
	}

	m_scaledAmbient = pWLDMaterialDef->scaled_ambient;
	m_constantAmbient = pWLDMaterialDef->brightness;

	EMaterialType type = MaterialType_Normal;

	if (pSimpleSpriteInst != nullptr)
	{
		m_textureSet = std::make_unique<STextureSet>();
		WLD_OBJ_SIMPLESPRITEDEFINITION* pSimpleSpriteDef = pParsedSimpleSpriteDef->definition;

		BufferReader spriteDefReader((const uint8_t*)(pSimpleSpriteDef + 1), 0x100000);

		if (pSimpleSpriteDef->flags & WLD_OBJ_SPROPT_HAVECURRENTFRAME)
		{
			spriteDefReader.read(m_textureSet->currentFrame);
		}
		if (pSimpleSpriteDef->flags & WLD_OBJ_SPROPT_HAVESLEEP)
		{
			uint32_t interval = 0;
			spriteDefReader.read(interval);
			m_textureSet->updateInterval = std::chrono::milliseconds(interval);
		}
		if (pSimpleSpriteDef->flags & WLD_OBJ_SPROPT_SKIPFRAMES)
		{
			m_textureSet->skipFrames = true;
		}

		m_textureSet->textures.resize(pSimpleSpriteDef->num_frames);
		const std::vector<std::shared_ptr<ParsedBMInfo>>& parsedBitmaps = pParsedSimpleSpriteDef->parsedBitmaps;

		for (uint32_t i = 0; i < pSimpleSpriteDef->num_frames; ++i)
		{
			auto parsedBitmap = parsedBitmaps[i]->bitmaps[0];
			m_textureSet->textures[i].flags = 0;
			m_textureSet->textures[i].filename = parsedBitmap->GetFileName();
			m_textureSet->textures[i].textures[0] = parsedBitmap;

			// If there are two, check the 2nd one.
			if (parsedBitmaps[i]->bitmaps.size() == 2
				&& (parsedBitmap->GetType() == eBitmapTypeLayer || parsedBitmaps[i]->bitmaps[1]->GetType() == eBitmapTypeLayer))
			{
				m_textureSet->textures[i].textures[1] = parsedBitmaps[i]->bitmaps[1];
				type = MaterialType_LuclinLayer;

				if (m_tag.length() >= 3 && m_tag[0] == 'I' && m_tag[1] == 'T' && isdigit(m_tag[2]))
				{
					type = MaterialType_LuclinLayerT1;
				}
			}
			else if (parsedBitmaps[i]->bitmaps.size() == 2
				&& parsedBitmap->GetType() == eBitmapTypeSingleDetail)
			{
				auto bitmap = parsedBitmaps[i]->bitmaps[1];

				m_textureSetAlt = std::make_unique<STextureSet>();
				m_textureSetAlt->updateInterval = m_textureSet->updateInterval;
				m_textureSetAlt->nextUpdate = m_textureSet->nextUpdate;
				m_textureSetAlt->currentFrame = m_textureSet->currentFrame;
				m_textureSetAlt->skipFrames = m_textureSet->skipFrames;
				m_textureSetAlt->textures.resize(m_textureSet->textures.size());

				m_textureSetAlt->textures[0].flags = 0;
				m_textureSetAlt->textures[0].filename = bitmap->GetFileName();
				m_textureSetAlt->textures[0].textures[0] = bitmap;

				type = MaterialType_SingleDetail;
				m_detailScale = bitmap->GetDetailScale();
			}
			else if (parsedBitmap->GetType() == eBitmapTypePaletteDetailMain)
			{
				m_detailPalette = std::make_unique<DetailPaletteInfo>();
				m_detailPalette->material = this;

				auto bitmap = pParsedBMPalette->bitmaps[1];
				m_detailPalette->paletteFileName = bitmap->GetFileName();
				m_detailPalette->width = bitmap->GetWidth();
				m_detailPalette->height = bitmap->GetHeight();
				m_detailPalette->paletteData = bitmap->GetRawData();

				uint32_t numDetail = (uint32_t)pParsedBMPalette->bitmaps.size();
				m_detailPalette->numDetails = numDetail - 2;
				for (uint32_t curDetail = 0; curDetail < numDetail - 2; ++curDetail)
				{
					auto bitmap = pParsedBMPalette->bitmaps[curDetail + 2];

					m_detailPalette->detailInfo[curDetail].fileName = bitmap->GetFileName();
					m_detailPalette->detailInfo[curDetail].scaleFactor = (int)bitmap->GetDetailScale();
					m_detailPalette->detailInfo[curDetail].grassDensity = bitmap->GetGrassDensity();

					auto detailMaterial = std::make_shared<Material>();
					detailMaterial->InitFromBitmap(bitmap);

					m_detailPalette->detailInfo[curDetail].material = detailMaterial;
				}

				type = MaterialType_PaletteDetail;
			}
		}

		if (pSimpleSpriteInst->flags & WLD_OBJ_SPROPT_HAVESKIPFRAMES)
		{
			m_textureSet->skipFrames = (pSimpleSpriteInst->flags & WLD_OBJ_SPROPT_SKIPFRAMES) != 0;
		}
	}

	SetWLDRenderMaterial(pWLDMaterialDef->render_method, type);
	return true;
}

bool Material::InitFromBitmap(const std::shared_ptr<Bitmap>& bitmap)
{
	m_tag = bitmap->GetFileName();
	m_renderMethod = 0x80000002;
	m_scaledAmbient = 0.0f;
	m_constantAmbient = 0.0f;

	m_textureSet = std::make_unique<STextureSet>();
	m_textureSet->textures.resize(1);
	m_textureSet->textures[0].flags = 0;
	m_textureSet->textures[0].filename = bitmap->GetFileName();
	m_textureSet->textures[0].textures[0] = bitmap;

	m_detailScale = bitmap->GetDetailScale();
	return true;
}

bool Material::InitFromMaterialInfo(const SMaterialInfo& info)
{
	m_renderMethod = 0x80000002;
	m_type = info.type;
	m_tag = info.tag;

	SetTextureSlot(m_tag);

	m_textureSet = std::make_unique<STextureSet>();
	m_textureSet->textures.resize(1);

	uint32_t currentBitmap = 0;
	ResourceManager* resourceMgr = ResourceManager::Get();

	for (auto& textureParam : info.params)
	{
		auto& newParam = m_effectParams.emplace_back();
		newParam.name = textureParam.name;
		newParam.type = textureParam.type;

		switch (newParam.type)
		{
		case FXParameterType_Int:
			newParam.n_value = textureParam.intValue;
			break;
		case FXParameterType_Float:
			newParam.f_value = textureParam.floatValue;
			break;
		case FXParameterType_Color:
			newParam.n_value = textureParam.intValue;
			break;
		case FXParameterType_Texture:
		{
			std::string_view filename = textureParam.fileName;
			BitmapPtr pBitmap = nullptr;

			if (filename != "None")
			{
				if (m_type == MaterialType_AlphaWater && find_substr(filename, "Environment") != -1)
				{
					// TODO: Something related to sky/cubemap
				}
				else if (m_type == MaterialType_AlphaLavaH && find_substr(filename, "Environment") != -1)
				{
					pBitmap = resourceMgr->CreateBitmap(filename, nullptr, true);
				}
				else
				{
					pBitmap = resourceMgr->CreateBitmap(filename, nullptr, false);
				}
			}

			if (currentBitmap == 0)
			{
				m_textureSet->textures[0].flags = 0;
				m_textureSet->textures[0].filename = filename;
				m_textureSet->textures[0].textures[0] = pBitmap;
			}
			else
			{
				m_textureSet->textures[0].textures[currentBitmap] = pBitmap;
			}
			newParam.index = currentBitmap++;
			break;
		}
		default: break;
		}
	}

	return UpdateMaterialFlags(false);
}

bool Material::UpdateMaterialFlags(bool eqmData)
{
	m_alpha = 255;

	switch (static_cast<EMaterialType>(m_type))
	{
	case MaterialType_Normal:
	{
		uint32_t renderMethod = m_renderMethod;

		if (renderMethod & RM_CUSTOM_MASK)
		{
			renderMethod = GetCustomRenderMethod(renderMethod & ~RM_CUSTOM_MASK);
		}

		if (renderMethod == RM_FILL_TRANSPARENT)
		{
			m_transparent = true;
			return true;
		}

		bool additive = renderMethod & RM_ADDITIVE_LIGHT_MASK;
		bool transparent = renderMethod & RM_TRANSPARENCY_MASK;
		bool translucent = renderMethod & RM_TRANSLUCENCY_MASK;

		if (transparent)
		{
			m_renderMaterial = RenderMaterial_Chroma;
		}
		else if (translucent)
		{
			m_alpha = ((renderMethod & RM_TRANSLUCENCY_LEVEL_MASK) >> 16) * 16;
			m_renderMaterial = additive ? RenderMaterial_AlphaBatchAdditive : RenderMaterial_AlphaBatch;
		}
		else
		{
			m_renderMaterial = RenderMaterial_Opaque;
		}
		break;
	}
	case MaterialType_PaletteDetail:
	case MaterialType_SingleDetail:
		m_renderMaterial = RenderMaterial_Opaque;
		break;

	case MaterialType_OpaqueC1:
		m_renderMaterial = RenderMaterial_OpaqueC1;
		break;
	case MaterialType_OpaqueCG1:
		m_renderMaterial = RenderMaterial_OpaqueCG1;
		break;
	case MaterialType_OpaqueCE1:
		m_renderMaterial = RenderMaterial_OpaqueCE1;
		break;
	case MaterialType_OpaqueCB1:
		m_renderMaterial = RenderMaterial_OpaqueCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBS1:
		m_renderMaterial = RenderMaterial_OpaqueCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBSG1:
		m_renderMaterial = RenderMaterial_OpaqueCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBSGE1:
		m_renderMaterial = RenderMaterial_OpaqueCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueC1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueC1_2UV;
		break;
	case MaterialType_OpaqueCB1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCB1_2UV;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueCBSG1_2UV:
		m_renderMaterial = RenderMaterial_OpaqueCBSG1_2UV;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueTerrain:
		m_renderMaterial = RenderMaterial_OpaqueTerrain;
		break;
	case MaterialType_OpaqueLava:
		m_renderMaterial = RenderMaterial_OpaqueLava;
		m_hasBumpMap = true;
		break;
	case MaterialType_OpaqueLava2:
		m_renderMaterial = RenderMaterial_OpaqueLava2;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaC1:
		m_renderMaterial = RenderMaterial_ChromaC1;
		break;
	case MaterialType_ChromaCG1:
		m_renderMaterial = RenderMaterial_ChromaCG1;
		break;
	case MaterialType_ChromaCE1:
		m_renderMaterial = RenderMaterial_ChromaCE1;
		break;
	case MaterialType_ChromaCB1:
		m_renderMaterial = RenderMaterial_ChromaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBS1:
		m_renderMaterial = RenderMaterial_ChromaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBSG1:
		m_renderMaterial = RenderMaterial_ChromaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_ChromaCBSGE1:
		m_renderMaterial = RenderMaterial_ChromaCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaC1:
		m_renderMaterial = RenderMaterial_AlphaC1;
		break;
	case MaterialType_AlphaCG1:
		m_renderMaterial = RenderMaterial_AlphaCG1;
		break;
	case MaterialType_AlphaCE1:
		m_renderMaterial = RenderMaterial_AlphaCE1;
		break;
	case MaterialType_AlphaCB1:
		m_renderMaterial = RenderMaterial_AlphaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBS1:
		m_renderMaterial = RenderMaterial_AlphaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBSG1:
		m_renderMaterial = RenderMaterial_AlphaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaCBSGE1:
		m_renderMaterial = RenderMaterial_AlphaCBSGE1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaWater:
		m_renderMaterial = RenderMaterial_AlphaWater;
		m_hasBumpMap = true;
		break;
	case MaterialType_AlphaWaterFall:
		m_renderMaterial = RenderMaterial_AlphaWaterFall;
		break;
	case MaterialType_AlphaLavaH:
		m_renderMaterial = RenderMaterial_AlphaLavaH;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaC1:
		m_renderMaterial = RenderMaterial_AddAlphaC1;
		break;
	case MaterialType_AddAlphaCG1:
		m_renderMaterial = RenderMaterial_AddAlphaCG1;
		break;
	case MaterialType_AddAlphaCE1:
		m_renderMaterial = RenderMaterial_AddAlphaCE1;
		break;
	case MaterialType_AddAlphaCB1:
		m_renderMaterial = RenderMaterial_AddAlphaCB1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBS1:
		m_renderMaterial = RenderMaterial_AddAlphaCBS1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBSG1:
		m_renderMaterial = RenderMaterial_AddAlphaCBSG1;
		m_hasBumpMap = true;
		break;
	case MaterialType_AddAlphaCBSGE1:
		m_renderMaterial = RenderMaterial_AddAlphaCBSGE1;
		m_hasBumpMap = true;
		break;

	case MaterialType_OpaqueCBS1_VSB:
	case MaterialType_OpaqueCBS_2UV:
	case MaterialType_OpaqueCBST2_2UV:
	case MaterialType_OpaqueBasic:
	case MaterialType_OpaqueBlend:
	case MaterialType_OpaqueBlendNoBump:
	case MaterialType_OpaqueFull:
	case MaterialType_OpaqueFull_2UV:
	case MaterialType_OpaqueBump:
	case MaterialType_OpaqueBump_2UV:
	case MaterialType_OpaqueSB:
	case MaterialType_OpaqueSB_2UV:
	case MaterialType_OpaqueGB:
	case MaterialType_OpaqueGB_2UV:
	case MaterialType_OpaqueRB:
	case MaterialType_OpaqueRB_2UV:
	case MaterialType_ChromaBasic:
	case MaterialType_ChromaBump:
	case MaterialType_AlphaBasic:
	case MaterialType_AlphaBump:
		if (eqmData)
		{
			// These types are only being handled by the EQM data
			switch (m_type)
			{
			case MaterialType_OpaqueCBS1_VSB:
				m_renderMaterial = RenderMaterial_OpaqueCBS1_VSB;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueCBS_2UV:
				m_renderMaterial = RenderMaterial_OpaqueCBS_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueCBST2_2UV:
				m_renderMaterial = RenderMaterial_OpaqueCBST2_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				m_hasVertexTint2 = true;
				break;
			case MaterialType_OpaqueBasic:
				m_renderMaterial = RenderMaterial_OpaqueBasic;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueBlend:
				m_renderMaterial = RenderMaterial_OpaqueBlend;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueBlendNoBump:
				m_renderMaterial = RenderMaterial_OpaqueBlendNoBump;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueFull:
				m_renderMaterial = RenderMaterial_OpaqueFull;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueFull_2UV:
				m_renderMaterial = RenderMaterial_OpaqueFull_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueBump:
				m_renderMaterial = RenderMaterial_OpaqueBump;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueBump_2UV:
				m_renderMaterial = RenderMaterial_OpaqueBump_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueSB:
				m_renderMaterial = RenderMaterial_OpaqueSB;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueSB_2UV:
				m_renderMaterial = RenderMaterial_OpaqueSB_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueGB:
				m_renderMaterial = RenderMaterial_OpaqueGB;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueGB_2UV:
				m_renderMaterial = RenderMaterial_OpaqueGB_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueRB:
				m_renderMaterial = RenderMaterial_OpaqueRB;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_OpaqueRB_2UV:
				m_renderMaterial = RenderMaterial_OpaqueRB_2UV;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_ChromaBasic:
				m_renderMaterial = RenderMaterial_ChromaBasic;
				m_hasVertexTint = true;
				break;
			case MaterialType_ChromaBump:
				m_renderMaterial = RenderMaterial_ChromaBump;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			case MaterialType_AlphaBasic:
				m_renderMaterial = RenderMaterial_AlphaBasic;
				m_hasVertexTint = true;
				break;
			case MaterialType_AlphaBump:
				m_renderMaterial = RenderMaterial_AlphaBump;
				m_hasBumpMap = true;
				m_hasVertexTint = true;
				break;
			}
			break;
		}
		[[fallthrough]];
	default:
		EQG_LOG_INFO("Unknown material type: {}", (int)m_type);
		break;
	}

	return true;
}

void Material::SetTextureSlot(std::string_view tag)
{
	m_itemSlot = ItemTextureSlot_None;

	if (tag.length() > 5)
	{
		std::string_view piece = tag.substr(3, 2);

		switch (piece[0])
		{
		case 'c':
		case 'C':
			if (piece[1] == 'h' || piece[1] == 'H')
			{
				if (tag.length() >= 8 && tag.substr(8, 1) == "1")
					m_itemSlot = ItemTextureSlot_Neck;
				else
					m_itemSlot = ItemTextureSlot_Chest;
			}
			break;
		case 'u':
		case 'U':
			if (piece[1] == 'a' || piece[1] == 'A')
				m_itemSlot = ItemTextureSlot_Arms;
			break;
		case 'f':
		case 'F':
			switch (piece[1])
			{
			case 'a':
			case 'A':
				m_itemSlot = ItemTextureSlot_Wrists;
				break;
			case 'r':
			case 'R':
				m_itemSlot = ItemTextureSlot_Legs;
				break;
			case 't':
			case 'T':
				m_itemSlot = ItemTextureSlot_Feet;
				break;
			}
			break;
		case 'h':
		case 'H':
			if (piece[1] == 'n' || piece[1] == 'N')
				m_itemSlot = ItemTextureSlot_Head;
			break;
		case 'l':
		case 'L':
			if (piece[1] == 'g' || piece[1] == 'G')
				m_itemSlot = ItemTextureSlot_Legs;
			break;
		default:
			if (ci_equals(tag, "a_dkm"))
			{
				m_itemSlot = ItemTextureSlot_Legs;
			}
			break;
		}
	}
}

void STextureSet::Update(world_clock::time_point time)
{
	if (textures.size() <= 1 || nextUpdate >= time)
		return;

	if (skipFrames)
	{
		// Interpolate frames based on how much time has passed
		std::chrono::milliseconds elapsedTime = time - nextUpdate;
		uint32_t missedFrames = static_cast<uint32_t>(elapsedTime.count() / updateInterval.count());

		currentFrame = (currentFrame + missedFrames + 1) % textures.size();
		std::chrono::milliseconds missedFrameTime = missedFrames * updateInterval;

		nextUpdate = time + updateInterval - (elapsedTime - missedFrameTime);
	}
	else
	{
		currentFrame = (currentFrame + 1) % textures.size();
		nextUpdate = time + std::chrono::milliseconds(updateInterval);
	}
}

void Material::Update(world_clock::time_point time, std::chrono::milliseconds elapsed)
{
	uint32_t renderMethod = GetRenderMethod();

	if ((renderMethod & RM_TEXTURE_MASK) != RM_TEXTURE_SOLID)
	{
		if (m_textureSet)
		{
			m_textureSet->Update(time);
		}
	}

	if (m_uvShiftPerMS.x != 0)
	{
		m_uvShift.x += static_cast<float>(elapsed.count()) * m_uvShiftPerMS.x;

		while (m_uvShift.x > 1.0f)
		{
			m_uvShift.x -= 1.0f;
		}

		while (m_uvShift.x < 0.0f)
		{
			m_uvShift.x += 1.0f;
		}
	}

	if (m_uvShiftPerMS.y != 0)
	{
		m_uvShift.y += static_cast<float>(elapsed.count()) * m_uvShiftPerMS.y;

		while (m_uvShift.y > 1.0f)
		{
			m_uvShift.y -= 1.0f;
		}

		while (m_uvShift.y < 0.0f)
		{
			m_uvShift.y += 1.0f;
		}
	}
}

std::shared_ptr<Material> Material::Clone() const
{
	auto copy = std::make_shared<Material>();

	copy->m_tag = m_tag;
	copy->m_effectName = m_effectName;
	copy->m_uvShift = m_uvShift;
	copy->m_renderMethod = m_renderMethod;
	copy->m_scaledAmbient = m_scaledAmbient;
	copy->m_constantAmbient = m_constantAmbient;
	copy->m_type = m_type;
	copy->m_detailScale = m_detailScale;
	copy->m_twoSided = m_twoSided;
	copy->m_transparent = m_transparent;
	copy->m_hasBumpMap = m_hasBumpMap;

	if (m_textureSet)
	{
		copy->m_textureSet = std::make_unique<STextureSet>(*m_textureSet);
	}

	if (m_textureSetAlt)
	{
		copy->m_textureSetAlt = std::make_unique<STextureSet>(*m_textureSetAlt);
	}

	if (m_detailPalette)
	{
		copy->m_detailPalette = std::make_unique<DetailPaletteInfo>(*m_detailPalette);
	}

	copy->m_effectParams = m_effectParams;

	return copy;
}

//-------------------------------------------------------------------------------------------------

MaterialPalette::MaterialPalette()
	: Resource(ResourceType::MaterialPalette)
{
}

MaterialPalette::MaterialPalette(std::string_view tag_, uint32_t num_materials_)
	: Resource(ResourceType::MaterialPalette)
	, m_tag(tag_)
	, m_materials(num_materials_)
{
}

MaterialPalette::~MaterialPalette()
{
}

void MaterialPalette::Update(world_clock::time_point time)
{
	if (m_requiresUpdate && m_lastUpdate != time)
	{
		std::chrono::milliseconds ms = time - m_lastUpdate;

		for (PaletteData& data : m_materials)
		{
			if (data.material)
				data.material->Update(time, ms);
		}

		m_lastUpdate = time;
	}
}

bool MaterialPalette::InitFromWLDData(std::string_view tag, ParsedMaterialPalette* materialPalette)
{
	m_tag = tag;

	m_materials.resize(materialPalette->matPalette->num_entries);
	for (size_t i = 0; i < m_materials.size(); ++i)
	{
		m_materials[i].material = materialPalette->materials[i];

		if (!m_requiresUpdate && m_materials[i].material->RequiresUpdate())
		{
			m_requiresUpdate = true;
		}
	}

	return true;
}

std::shared_ptr<MaterialPalette> MaterialPalette::Clone(bool deep) const
{
	std::shared_ptr<MaterialPalette> copy = std::make_shared<MaterialPalette>(m_tag, (uint32_t)m_materials.size());

	copy->m_tag = m_tag;
	copy->m_lastUpdate = m_lastUpdate;
	copy->m_requiresUpdate = m_requiresUpdate;
	copy->m_materials = m_materials;

	if (deep)
	{
		for (PaletteData& paletteData : copy->m_materials)
		{
			paletteData.material = paletteData.material->Clone();
		}
	}
	return copy;
}


} // namespace eqg
