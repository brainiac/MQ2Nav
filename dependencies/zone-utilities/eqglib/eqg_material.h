
#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace eqg {

class Archive;
struct SEQMMaterial;
struct SEQMFXParameter;

struct STexture
{
	std::string filename;
	uint32_t flags = 0;

	// Actual graphics texture goes here
};

struct STextureSet
{
	uint32_t update_interval = 0;
	uint32_t next_update = 0;
	uint32_t current_frame = 0;
	std::vector<STexture> textures;
	bool skip_frames = false;
};

enum eFXParameterType
{
	eFXParameterTexture,
	eFXParameterFloat,
	eFXParameterInt,
	eFXParameterColor,
	eFXParameterUnused,
};

struct SFXParameter
{
	std::string name;
	eFXParameterType type;

	union
	{
		uint32_t index;
		float f_value;
		int n_value;
	};
};

class Material
{
public:
	std::string m_tag;
	std::string m_effectName;

	STextureSet* m_textureSet = nullptr;
	std::vector<SFXParameter> m_effectParams;

	Material();
	~Material();

	void InitFromEQMData(SEQMMaterial* eqm_material, SEQMFXParameter* eqm_fx_params, Archive* archive, const char* string_pool);

};


class MaterialPalette
{
public:
	std::string tag;

	struct PaletteData
	{
		std::shared_ptr<Material> material;
		uint32_t tint = (uint32_t)-1;
		uint32_t secondary_tint = (uint32_t)-1;
	};
	std::vector<PaletteData> materials;

	MaterialPalette(std::string_view tag_, uint32_t num_materials_)
		: tag(tag_)
		, materials(num_materials_)
	{
	}

	void SetMaterial(uint32_t index, const std::shared_ptr<Material>& material)
	{
		if (index < materials.size())
		{
			materials[index].material = material;
		}
	}
};


} // namespace eqg
