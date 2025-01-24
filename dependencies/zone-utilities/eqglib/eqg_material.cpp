#include "pch.h"
#include "eqg_material.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_resource_manager.h"
#include "eqg_structs.h"
#include "log_internal.h"
#include "wld_fragment.h"

namespace eqg {

//-------------------------------------------------------------------------------------------------

Bitmap::Bitmap()
	: Resource(ResourceType::Bitmap)
{
}

Bitmap::~Bitmap()
{
}

bool Bitmap::InitFromWLDData(SBitmapWLDData* wldData, Archive* archive, ResourceManager* resourceMgr)
{
	m_fileName = wldData->fileName;
	m_detailScale = wldData->detailScale;
	m_grassDensity = wldData->grassDensity;
	m_objectIndex = wldData->objectIndex;

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
					param.type = eFXParameterUnused;
				}
				else
				{
					param.type = eFXParameterFloat;

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
				param.type = eFXParameterInt;
				param.n_value = in_param->n_value;
				break;

			case eEQMFXParameterColor:
				param.type = eFXParameterColor;
				param.n_value = in_param->n_value;
				break;

			case eEQMFXParameterTexture:
				param.type = eFXParameterTexture;
				
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

std::shared_ptr<Material> Material::Clone() const
{
	auto copy = std::make_shared<Material>();

	copy->m_tag = m_tag;
	copy->m_effectName = m_effectName;
	copy->m_twoSided = m_twoSided;
	copy->m_uvShift = m_uvShift;
	copy->m_renderMethod = m_renderMethod;
	copy->m_scaledAmbient = m_scaledAmbient;
	copy->m_constantAmbient = m_constantAmbient;
	copy->m_type = m_type;
	copy->m_detailScale = m_detailScale;

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

//-------------------------------------------------------------------------------------------------

BlitSpriteDefinition::BlitSpriteDefinition()
	: Resource(GetStaticResourceType())
{
}

BlitSpriteDefinition::~BlitSpriteDefinition()
{
}

bool BlitSpriteDefinition::Init(std::string_view tag, const STextureDataDefinition& definition)
{
	m_tag = tag;
	m_columns = definition.columns;
	m_rows = definition.rows;
	m_width = definition.width;
	m_height = definition.height;
	m_numFrames = definition.numFrames;
	m_currentFrame = definition.currentFrame;
	m_updateInterval = definition.updateInterval;
	m_renderMethod = definition.renderMethod;
	m_valid = definition.valid;
	m_skipFrames = definition.skipFrames;

	// TODO: Init texture from frames of bitmap
	m_sourceTextures = definition.sourceTextures;

	return true;
}

void BlitSpriteDefinition::CopyDefinition(STextureDataDefinition& outDefinition)
{
	outDefinition.columns = m_columns;
	outDefinition.rows = m_rows;
	outDefinition.width = m_width;
	outDefinition.height = m_height;
	outDefinition.numFrames = m_numFrames;
	outDefinition.currentFrame = m_currentFrame;
	outDefinition.updateInterval = m_updateInterval;
	outDefinition.renderMethod = m_renderMethod;
	outDefinition.valid = m_valid;
	outDefinition.skipFrames = m_skipFrames;
	outDefinition.sourceTextures = m_sourceTextures;
}

} // namespace eqg
