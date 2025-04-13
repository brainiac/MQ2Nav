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
	return 0; // return s_customRenderMethods[customIndex];
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

bool Bitmap::Init(std::string_view fileName, Archive* archive, bool cubeMap)
{
	m_fileName = fileName;
	m_detailScale = 1.0f;
	m_grassDensity = 0;

	ResourceManager* resourceMgr = ResourceManager::Get();

	if (cubeMap)
	{
		// TODO: cube map
	}
	else
	{
		if (!resourceMgr->LoadTexture(this, archive))
		{
			EQG_LOG_ERROR("Failed to create texture for {}", m_fileName);
			return false;
		}
	}

	m_hasTexture = true;
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

void Material::InitFromEQMData(SEQMMaterial* eqm_material, SEQMFXParameter* eqm_fx_params, Archive* archive, const char* string_pool)
{
	// Initialize a material from EQG model data

	m_tag = string_pool + eqm_material->name_index;
	m_effectName = string_pool + eqm_material->effect_name_index;
	uint32_t num_effect_params = eqm_material->num_params;

	int type = -1;

	if (m_effectName.find("C1DTP") != std::string::npos)
	{
		type = 2;
	}

	int max_frames = 1;
	int frame_interval = 0;
	//std::vector<std::string> frames;

	if (type != 2) // not detail palette
	{
		for (uint32_t param_index = 0; param_index < num_effect_params; ++param_index)
		{
			SEQMFXParameter* param = eqm_fx_params + param_index;
			const char* filename = string_pool + param->n_value;

			if (param->type == eEQMFXParameterTexture && archive != nullptr)
			{
				char info_name[256];
				strcpy_s(info_name, filename);
				info_name[strlen(info_name) - 3] = 0;
				strcat_s(info_name, "txt");

				std::vector<char> anim_info_buffer;
				if (archive->Get(info_name, anim_info_buffer))
				{
					struct ReadBuf : public std::streambuf
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

					// Parse lines;
					if (lines.size() > 2)
					{
						int frame_count = str_to_int(lines[0], 0);
						frame_interval = str_to_int(lines[1], 0);
						//if (frame_count > 0 && frame_count < lines.size() - 2)
						//{
						//	for (int i = 0; i < frame_count; ++i)
						//	{
						//		frames.push_back(lines[2 + i]);
						//	}
						//}

						max_frames = std::max(frame_count, max_frames);
					}
				}
			}
		}
	}

	m_textureSet = std::make_unique<STextureSet>();
	m_textureSet->textures.resize(max_frames);
	m_textureSet->updateInterval = frame_interval;
	m_effectParams.resize(num_effect_params);

	if (type == 2) // detail palette
	{
		
	}
	else
	{
		uint32_t curr_bitmap = 0;

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
				else
				{
					param.type = FXParameterType_Float;

					if (param.name == "e_fEnvMapStrength0")
					{
						param.f_value = in_param->f_value * 2.0f;
					}
					else
					{
						param.f_value = in_param->f_value;
					}
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
				param.type = FXParameterType_Texture;
				
				// this is where we load the textures for the material
				const char* filename = string_pool + in_param->n_value;

				if (curr_bitmap == 0)
				{
					for (uint32_t index = 0; index < m_textureSet->textures.size(); ++index)
					{
						m_textureSet->textures[index].flags = 0;
						m_textureSet->textures[index].filename = filename;
					}
				}
			}
		}
	}
}

bool Material::InitFromWLDData(
	std::string_view tag,
	WLD_OBJ_MATERIALDEFINITION* pWLDMaterialDef,
	WLD_OBJ_SIMPLESPRITEINSTANCE* pSimpleSpriteInst,
	ParsedSimpleSpriteDef* pParsedSimpleSpriteDef,
	ParsedBMInfo* pParsedBMPalette)
{
	m_tag = tag;
	// something something item slot

	BufferReader materiaDefReader((const uint8_t*)(pWLDMaterialDef + 1), 0x100000); // we have no idea how long these buffers are

	if (pWLDMaterialDef->flags & WLD_OBJ_MATOPT_TWOSIDED)
	{
		m_twoSided = true;
	}
	if (pWLDMaterialDef->flags & WLD_OBJ_MATOPT_HASUVSHIFTPERMS)
	{
		materiaDefReader.read(m_uvShift);
	}

	m_renderMethod = pWLDMaterialDef->render_method;
	m_scaledAmbient = pWLDMaterialDef->scaled_ambient;
	m_constantAmbient = pWLDMaterialDef->brightness;

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
			spriteDefReader.read(m_textureSet->updateInterval);
		}
		if (pSimpleSpriteDef->flags & WLD_OBJ_SPROPT_SKIPFRAMES)
		{
			m_textureSet->skipFrames = true;
		}

		m_textureSet->textures.resize(pSimpleSpriteDef->num_frames);
		const std::vector<std::shared_ptr<ParsedBMInfo>>& parsedBitmaps = pParsedSimpleSpriteDef->parsedBitmaps;

		for (uint32_t i = 0; i < pSimpleSpriteDef->num_frames; ++i)
		{
			auto bitmap = parsedBitmaps[i]->bitmaps[0];
			m_textureSet->textures[i].flags = 0;
			m_textureSet->textures[i].filename = bitmap->GetFileName();

			// TODO: Figure out how to properly set/load texture?
			m_textureSet->textures[i].textures[0] = bitmap;

			// If there are two, check the 2nd one.
			if (parsedBitmaps[i]->bitmaps.size() == 2
				&& (bitmap->GetType() == eBitmapTypeLayer || parsedBitmaps[i]->bitmaps[1]->GetType() == eBitmapTypeLayer))
			{
				m_textureSet->textures[i].textures[1] = parsedBitmaps[i]->bitmaps[1];
				m_type = 3;

				if (m_tag.length() >= 3 && m_tag[0] == 'I' && m_tag[1] == 'T' && isdigit(m_tag[2]))
				{
					m_type = 4;
				}
			}
			else if (parsedBitmaps[i]->bitmaps.size() == 2
				&& bitmap->GetType() == eBitmapTypeSingleDetail)
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

				m_type = 1;
				m_detailScale = bitmap->GetDetailScale();
			}
			else if (bitmap->GetType() == eBitmapTypePaletteDetailMain)
			{
				m_detailPalette = std::make_unique<DetailPaletteInfo>();
				m_detailPalette->material = this;

				auto bitmap = pParsedBMPalette->bitmaps[1];
				m_detailPalette->paletteFileName = bitmap->GetFileName();
				m_detailPalette->width = bitmap->GetWidth();
				m_detailPalette->height = bitmap->GetHeight();
				m_detailPalette->paletteData = bitmap->GetRawData();

				uint32_t numDetail = (uint32_t)pParsedBMPalette->bitmaps.size();
				m_detailPalette->detailInfo.resize(numDetail - 2);
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

				m_type = 2;
			}
		}

		if (pSimpleSpriteInst->flags & WLD_OBJ_SPROPT_HAVESKIPFRAMES)
		{
			if (pSimpleSpriteInst->flags & WLD_OBJ_SPROPT_SKIPFRAMES)
			{
				m_textureSet->skipFrames = true;
			}
		}
	}

	// TODO: Process material type, render method, transparency, etc

	if (m_type == eBitmapTypeNormal)
	{
		uint32_t renderMethod = m_renderMethod;

		if (renderMethod & RM_CUSTOM_MASK)
		{
			renderMethod = GetCustomRenderMethod(renderMethod & ~RM_CUSTOM_MASK);
		}
		else if ((renderMethod & RM_TEXTURE_MASK) == 0 && (renderMethod & RM_TRANSLUCENCY_MASK) == 0)
		{
			renderMethod = 0;
		}

		if ((renderMethod & RM_FILL_MASK) == RM_FILL_TRANSPARENT)
		{
			m_transparent = true;
			return true;
		}
	}

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

	SetTextureSlot();

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
		EQG_LOG_INFO("Unknown material type: {}", m_type);
		break;
	}

	return true;
}

void Material::SetTextureSlot()
{
	m_itemSlot = ItemTextureSlot_None;

	if (m_tag.length() > 5)
	{
		std::string_view piece = std::string_view(m_tag).substr(3, 2);

		switch (piece[0])
		{
		case 'c':
		case 'C':
			if (piece[1] == 'h' || piece[1] == 'H')
			{
				if (std::string_view(m_tag).substr(8, 1) == "1")
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
			if (ci_equals(m_tag, "a_dkm"))
			{
				m_itemSlot = ItemTextureSlot_Legs;
			}
			break;
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
