
#pragma once

#include "eqg_resource_manager.h"
#include "eqg_types_fwd.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace eqg {

struct ParsedBMInfo;
struct ParsedMaterialPalette;
struct WLD_OBJ_SIMPLESPRITEINSTANCE;
struct WLD_OBJ_MATERIALDEFINITION;
struct WLD_OBJ_MATERIALPALETTE;

struct ParsedSimpleSpriteDef;
struct STextureDataDefinition;

struct SEQMMaterial;
struct SEQMFXParameter;

using std::chrono::steady_clock;

enum EBitmapType
{
	eBitmapTypeNormal,
	eBitmapTypeLayer,
	eBitmapTypeSingleDetail,
	eBitmapTypePaletteDetailMain,
	eBitmapTypePaletteDetailPalette,
	eBitmapTypePaletteDetailDetail,
};

enum EItemTextureSlot
{
	ItemTextureSlot_None = -1,
	ItemTextureSlot_Head = 0,
	ItemTextureSlot_Chest,
	ItemTextureSlot_Arms,
	ItemTextureSlot_Wrists,
	ItemTextureSlot_Hands,
	ItemTextureSlot_Legs,
	ItemTextureSlot_Feet,
	ItemTextureSlot_Primary,
	ItemTextureSlot_Secondary,
	ItemTextureSlot_Face,
	ItemTextureSlot_Neck,

	ItemTextureSlot_LastBody = ItemTextureSlot_Feet,
};


// Render Method data

// Render Method flags - Fill type
constexpr uint32_t RM_FILL_MASK                  = 0x00000003;
constexpr uint32_t RM_FILL_TRANSPARENT           = 0;
constexpr uint32_t RM_FILL_POINT                 = 1;
constexpr uint32_t RM_FILL_WIREFRAME             = 2;
constexpr uint32_t RM_FILL_SOLID                 = 3;

// Render Method flags - Lighting type
constexpr uint32_t RM_LIGHTING_MASK              = 0x0000001c;
constexpr uint32_t RM_LIGHTING_ZERO              = 0x00000000;
constexpr uint32_t RM_LIGHTING_FULL              = 0x00000004;
constexpr uint32_t RM_LIGHTING_CONSTANT          = 0x00000008;
constexpr uint32_t RM_LIGHTING_AMBIENT           = 0x00000010;
constexpr uint32_t RM_LIGHTING_SCALED_AMBIENT    = 0x00000014;

// Render Method flags - Transparency type
constexpr uint32_t RM_TRANSPARENCY_MASK          = 0x00000080;

// Render Method flags - Texture type
constexpr uint32_t RM_TEXTURE_MASK               = 0x0000ff00;
constexpr uint32_t RM_TEXTURE_SOLID              = 0x00000000; // i.e. no texture
constexpr uint32_t RM_TEXTURE_1                  = 0x00000100;
constexpr uint32_t RM_TEXTURE_2                  = 0x00000200;
constexpr uint32_t RM_TEXTURE_3                  = 0x00000300;
constexpr uint32_t RM_TEXTURE_4                  = 0x00000400;
constexpr uint32_t RM_TEXTURE_5                  = 0x00000500;
constexpr uint32_t RM_TEXTURE_BLIT               = 0x0000ff00;

constexpr uint32_t RM_TRANSLUCENCY_LEVEL_MASK    = 0x000f0000;
constexpr uint32_t RM_TRANSLUCENCY_LEVEL_NONE    = 0x00000000;

constexpr uint32_t RM_ADDITIVE_LIGHT_MASK        = 0x00100000;

// Render Method flags - Translucency type
constexpr uint32_t RM_TRANSLUCENCY_MASK          = 0x01000000;

constexpr uint32_t RM_TRANSLUCENCY_LEVEL_0       = 0x01000000;
constexpr uint32_t RM_TRANSLUCENCY_LEVEL_25	     = 0x01040000;
constexpr uint32_t RM_TRANSLUCENCY_LEVEL_50      = 0x01080000;
constexpr uint32_t RM_TRANSLUCENCY_LEVEL_75      = 0x010c0000;

constexpr uint32_t RM_TRANSLUCENCY_ADDITIVE_25   = 0x01130000;
constexpr uint32_t RM_TRANSLUCENCY_ADDITIVE_50   = 0x01170000;
constexpr uint32_t RM_TRANSLUCENCY_ADDITIVE_75   = 0x011b0000;
constexpr uint32_t RM_TRANSLUCENCY_ADDITIVE_100  = 0x011f0000;

constexpr uint32_t RM_DIRECTIONAL_LIGHTS_MASK    = 0x00200000;
constexpr uint32_t RM_PRECALC_VERTEX_LIGHTING    = 0x00800000;

// Render Method - custom type
constexpr uint32_t RM_CUSTOM_MASK                = 0x80000000;

// Preset render methods
constexpr uint32_t RM_TRANSPARENT         = 0;
constexpr uint32_t RM_WIRE_FRAME = RM_FILL_WIREFRAME;

uint32_t GetCustomRenderMethod(uint32_t customIndex);

struct SBitmapWLDData
{
	std::string fileName;
	float detailScale;
	uint32_t grassDensity;
	bool createTexture;
	uint32_t objectIndex;
};

class Bitmap : public eqg::Resource
{
public:
	Bitmap();
	~Bitmap() override;

	static ResourceType GetStaticResourceType() { return ResourceType::Bitmap; }

	const std::string& GetFileName() const { return m_fileName; }
	std::string_view GetTag() const override { return m_fileName; }
	EBitmapType GetType() const { return m_type; }
	uint32_t GetObjectIndex() const { return m_objectIndex; }
	float GetDetailScale() const { return m_detailScale; }
	uint32_t GetGrassDensity() const { return m_grassDensity; }
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }

	virtual bool LoadTexture() { return true; }

	void SetType(EBitmapType type) { m_type = type; }
	void SetSize(uint32_t width, uint32_t height) { m_width = width; m_height = height; }
	void SetSourceSize(uint32_t width, uint32_t height) { m_sourceWidth = width; m_sourceHeight = height; }
	void SetForceMipMap(bool force) { m_forceMipMap = force; }

	bool InitFromWLDData(SBitmapWLDData* wldData, Archive* archive);
	bool Init(std::string_view fileName, Archive* archive, bool cubeMap);

	virtual char* GetRawData() const { return nullptr; }
	virtual void SetRawData(std::unique_ptr<char[]> rawData, size_t rawDatasize) { /*m_rawData = rawData; m_byteSize = (uint32_t)rawDatasize;*/ }

private:
	std::string              m_fileName;
	EBitmapType              m_type = eBitmapTypeNormal;
	uint32_t                 m_sourceWidth = 0;
	uint32_t                 m_sourceHeight = 0;
	float                    m_detailScale = 1.0f;
	uint32_t                 m_grassDensity = 0;
	uint32_t                 m_width = 0;
	uint32_t                 m_height = 0;
	uint32_t                 m_objectIndex = (uint32_t)-1;
	bool                     m_hasTexture = false;
	bool                     m_forceMipMap = false;

	//std::unique_ptr<char[]>  m_rawData;
	//uint32_t                 m_byteSize = 0;
};

struct STexture
{
	std::string              filename;
	uint32_t                 flags = 0;

	// This is what actually gets drawn as a texture
	std::shared_ptr<Bitmap> textures[8];
};

struct STextureSet
{
	uint32_t                 updateInterval = 0;
	steady_clock::time_point nextUpdate = steady_clock::now();
	uint32_t                 currentFrame = 0;
	std::vector<STexture>    textures;
	bool                     skipFrames = false;
};

enum EMaterialType
{
	MaterialType_Normal,
	MaterialType_SingleDetail,
	MaterialType_PaletteDetail,
	MaterialType_LuclinLayer,
	MaterialType_LuclinLayerT1,

	MaterialType_OpaqueC1,
	MaterialType_OpaqueCG1,
	MaterialType_OpaqueCE1,
	MaterialType_OpaqueCB1,
	MaterialType_OpaqueCBS1,
	MaterialType_OpaqueCBS1_VSB,
	MaterialType_OpaqueCBS_2UV,
	MaterialType_OpaqueCBSG1,
	MaterialType_OpaqueCBSGE1,
	MaterialType_OpaqueC1_2UV,
	MaterialType_OpaqueCB1_2UV,
	MaterialType_OpaqueCBSG1_2UV,
	MaterialType_OpaqueCBST2_2UV,
	MaterialType_OpaqueTerrain,
	MaterialType_OpaqueLava,
	MaterialType_OpaqueLava2,
	MaterialType_OpaqueBasic,
	MaterialType_OpaqueBlend,
	MaterialType_OpaqueBlendNoBump,
	MaterialType_OpaqueFull,
	MaterialType_OpaqueFull_2UV,
	MaterialType_OpaqueBump,
	MaterialType_OpaqueBump_2UV,
	MaterialType_OpaqueSB,
	MaterialType_OpaqueSB_2UV,
	MaterialType_OpaqueGB,
	MaterialType_OpaqueGB_2UV,
	MaterialType_OpaqueRB,
	MaterialType_OpaqueRB_2UV,
	MaterialType_ChromaC1,
	MaterialType_ChromaCG1,
	MaterialType_ChromaCE1,
	MaterialType_ChromaCB1,
	MaterialType_ChromaCBS1,
	MaterialType_ChromaCBS1_VSB,
	MaterialType_ChromaCBSG1,
	MaterialType_ChromaCBSGE1,
	MaterialType_ChromaBasic,
	MaterialType_ChromaBump,
	MaterialType_AlphaC1,
	MaterialType_AlphaCG1,
	MaterialType_AlphaCE1,
	MaterialType_AlphaCB1,
	MaterialType_AlphaCBS1,
	MaterialType_AlphaCBSG1,
	MaterialType_AlphaCBSGE1,
	MaterialType_AlphaBasic,
	MaterialType_AlphaBump,
	MaterialType_AlphaWater,
	MaterialType_AlphaWaterFall,
	MaterialType_AlphaLavaH,
	MaterialType_AddAlphaC1,
	MaterialType_AddAlphaCG1,
	MaterialType_AddAlphaCE1,
	MaterialType_AddAlphaCB1,
	MaterialType_AddAlphaCBS1,
	MaterialType_AddAlphaCBSG1,
	MaterialType_AddAlphaCBSGE1,
};

enum ERenderMaterial
{
	RenderMaterial_Opaque,
	RenderMaterial_Chroma,
	RenderMaterial_AlphaSingleDetail,
	RenderMaterial_AlphaPaletteDetail,
	RenderMaterial_AlphaBatchAdditive,
	RenderMaterial_AlphaBatch,
	RenderMaterial_OpaqueC1,
	RenderMaterial_OpaqueCG1,
	RenderMaterial_OpaqueCE1,
	RenderMaterial_OpaqueCB1,
	RenderMaterial_OpaqueCBS1,
	RenderMaterial_OpaqueCBS1_VSB,
	RenderMaterial_OpaqueCBS_2UV,
	RenderMaterial_OpaqueCBSG1,
	RenderMaterial_OpaqueCBSGE1,
	RenderMaterial_OpaqueC1_2UV,
	RenderMaterial_OpaqueCB1_2UV,
	RenderMaterial_OpaqueCBSG1_2UV,
	RenderMaterial_OpaqueCBST2_2UV,
	RenderMaterial_OpaqueTerrain,
	RenderMaterial_OpaqueLava,
	RenderMaterial_OpaqueLava2,
	RenderMaterial_OpaqueBasic,
	RenderMaterial_OpaqueBlend,
	RenderMaterial_OpaqueBlendNoBump,
	RenderMaterial_OpaqueFull,
	RenderMaterial_OpaqueFull_2UV,
	RenderMaterial_OpaqueBump,
	RenderMaterial_OpaqueBump_2UV,
	RenderMaterial_OpaqueSB,
	RenderMaterial_OpaqueSB_2UV,
	RenderMaterial_OpaqueGB,
	RenderMaterial_OpaqueGB_2UV,
	RenderMaterial_OpaqueRB,
	RenderMaterial_OpaqueRB_2UV,
	RenderMaterial_ChromaC1,
	RenderMaterial_ChromaCG1,
	RenderMaterial_ChromaCE1,
	RenderMaterial_ChromaCB1,
	RenderMaterial_ChromaCBS1,
	RenderMaterial_ChromaCBSG1,
	RenderMaterial_ChromaCBSGE1,
	RenderMaterial_ChromaBasic,
	RenderMaterial_ChromaBump,
	RenderMaterial_AlphaC1,
	RenderMaterial_AlphaCG1,
	RenderMaterial_AlphaCE1,
	RenderMaterial_AlphaCB1,
	RenderMaterial_AlphaCBS1,
	RenderMaterial_AlphaCBSG1,
	RenderMaterial_AlphaCBSGE1,
	RenderMaterial_AlphaBasic,
	RenderMaterial_AlphaBump,
	RenderMaterial_AlphaWater,
	RenderMaterial_AlphaWaterFall,
	RenderMaterial_AlphaLavaH,
	RenderMaterial_AddAlphaC1,
	RenderMaterial_AddAlphaCG1,
	RenderMaterial_AddAlphaCE1,
	RenderMaterial_AddAlphaCB1,
	RenderMaterial_AddAlphaCBS1,
	RenderMaterial_AddAlphaCBSG1,
	RenderMaterial_AddAlphaCBSGE1,
};

enum EFXParameterType
{
	FXParameterType_Texture,
	FXParameterType_Float,
	FXParameterType_Int,
	FXParameterType_Color,
	FXParameterType_Unused,
};

struct SFXParameter
{
	std::string name;
	EFXParameterType type;

	union
	{
		uint32_t index;
		float f_value;
		int n_value;
	};
};

struct DetailInfo
{
	std::string fileName;
	int scaleFactor;
	int grassDensity;
	std::shared_ptr<Material> material; // detail palette material
};

struct DetailPaletteInfo
{
	std::string paletteFileName;
	int width;
	int height;
	char* paletteData;
	std::vector<DetailInfo> detailInfo;

	Material* material; // owner
};

struct SMaterialInfoFXParameter
{
	SMaterialInfoFXParameter(std::string_view name, std::string_view textureFileName)
		: name(name)
		, type(FXParameterType_Texture)
		, fileName(textureFileName)
	{
	}

	SMaterialInfoFXParameter(std::string_view name, float floatValue)
		: name(name)
		, type(FXParameterType_Float)
		, floatValue(floatValue)
	{
	}

	SMaterialInfoFXParameter(std::string_view name, int intValue)
		: name(name)
		, type(FXParameterType_Int)
		, intValue(intValue)
	{
	}

	SMaterialInfoFXParameter(std::string_view name, float red, float green, float blue, float alpha)
		: name(name)
		, type(FXParameterType_Color)
	{
		intValue = static_cast<int>(red * 255.0f) << 24
			| static_cast<int>(green * 255.0f) << 16
			| static_cast<int>(blue * 255.0f) << 8
			| static_cast<int>(alpha * 255.0f);
	}

	SMaterialInfoFXParameter(std::string_view name, const glm::vec4& color)
		: name(name)
		, type(FXParameterType_Color)
	{
		intValue = static_cast<int>(color[0] * 255.0f) << 24
			| static_cast<int>(color[1] * 255.0f) << 16
			| static_cast<int>(color[2] * 255.0f) << 8
			| static_cast<int>(color[3] * 255.0f);
	}

	std::string               name;
	EFXParameterType          type;

	std::string               fileName;
	int                       intValue = 0;
	float                     floatValue = 0;
};

struct SMaterialInfo
{
	std::string   tag;
	EMaterialType type;
	std::vector<SMaterialInfoFXParameter> params;
};

class Material : public eqg::Resource
{
public:
	Material();
	~Material() override;

	static ResourceType GetStaticResourceType() { return ResourceType::Material; }

	std::string_view GetTag() const override { return m_tag; }

	bool RequiresUpdate() const
	{
		return m_textureSet != nullptr && (m_textureSet->textures.size() > 1 || m_uvShift != glm::vec2(0.0f));
	}

	std::shared_ptr<Material> Clone() const;

	void InitFromEQMData(SEQMMaterial* eqm_material, SEQMFXParameter* eqm_fx_params, Archive* archive, const char* string_pool);
	bool InitFromWLDData(std::string_view tag, WLD_OBJ_MATERIALDEFINITION* pWLDMaterialDef, WLD_OBJ_SIMPLESPRITEINSTANCE* pSimpleSpriteInst,
		ParsedSimpleSpriteDef* pParsedSimpleSpriteDef, ParsedBMInfo* pParsedBMPalette);
	bool InitFromBitmap(const std::shared_ptr<Bitmap>& bitmap);
	bool InitFromMaterialInfo(const SMaterialInfo& info);

	bool IsTransparent() const { return m_transparent; }
	bool HasBumpMap() const { return m_hasBumpMap; }

private:
	bool UpdateMaterialFlags(bool eqm);
	void SetTextureSlot();

public:
	std::string                 m_tag;
	std::string                 m_effectName;
	glm::vec2                   m_uvShift = glm::vec2(0.0f);
	uint32_t                    m_renderMethod = 0;
	uint32_t                    m_renderMaterial = 0;
	float                       m_scaledAmbient = 0.0f;
	float                       m_constantAmbient = 0.0f;
	int                         m_type = 0;
	float                       m_detailScale = 1.0f;
	uint8_t                     m_alpha = 255;
	bool                        m_transparent = false;
	bool                        m_hasBumpMap = false;
	bool                        m_twoSided = false;
	bool                        m_hasVertexTint = false;
	bool                        m_hasVertexTint2 = false;
	EItemTextureSlot            m_itemSlot = ItemTextureSlot_None;

	std::unique_ptr<STextureSet> m_textureSet;
	std::unique_ptr<STextureSet> m_textureSetAlt;
	std::unique_ptr<DetailPaletteInfo> m_detailPalette;
	std::vector<SFXParameter>   m_effectParams;
};

class MaterialPalette : public Resource
{
public:
	MaterialPalette();
	MaterialPalette(std::string_view tag_, uint32_t num_materials_);
	~MaterialPalette() override;

	struct PaletteData
	{
		std::shared_ptr<Material> material;
		uint32_t                  tint = (uint32_t)-1;
		uint32_t                  secondary_tint = (uint32_t)-1;
	};

	static ResourceType GetStaticResourceType() { return ResourceType::MaterialPalette; }

	std::string_view GetTag() const override { return m_tag; }

	void SetMaterial(uint32_t index, const std::shared_ptr<Material>& material)
	{
		if (index < m_materials.size())
		{
			m_materials[index].material = material;
		}
	}
	Material* GetMaterial(uint32_t index) const { return m_materials[index].material.get(); }
	uint32_t GetNumMaterials() const { return (uint32_t)m_materials.size(); }

	bool InitFromWLDData(std::string_view tag, ParsedMaterialPalette* materialPalette);

	std::shared_ptr<MaterialPalette> Clone(bool deep = false) const;

private:
	std::string              m_tag;
	steady_clock::time_point m_lastUpdate = steady_clock::now();
	bool                     m_requiresUpdate = false;
	std::vector<PaletteData> m_materials;
};


} // namespace eqg
