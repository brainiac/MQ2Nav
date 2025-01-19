#include "pch.h"
#include "eqg_material.h"

#include <mq/base/String.h>

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_structs.h"

namespace eqg {

Material::Material()
{
	
}

Material::~Material()
{
	if (m_textureSet != nullptr)
	{
		delete m_textureSet;
		m_textureSet = nullptr;
	}
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

	m_textureSet = new STextureSet();
	m_textureSet->textures.resize(max_frames);
	m_textureSet->update_interval = frame_interval;
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
						m_textureSet->textures[index].filename = string_pool + eqm_material->effect_name_index;
					}
				}
			}
		}
	}
}

} // namespace eqg
