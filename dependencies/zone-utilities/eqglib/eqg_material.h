
#pragma once

#include "eqg_resource_manager.h"

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

class Archive;
class Material;
class ResourceManager;
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

struct SBitmapWLDData
{
	std::string fileName;
	float detailScale;
	uint32_t grassDensity;
	bool createTexture;
	uint32_t objectIndex;
};

class EQGBitmap : public eqg::Resource
{
public:
	EQGBitmap();
	~EQGBitmap() override;

	static ResourceType GetStaticResourceType() { return ResourceType::Bitmap; }

	const std::string& GetFileName() const { return m_fileName; }
	std::string_view GetTag() const override { return m_fileName; }
	EBitmapType GetType() const { return m_type; }
	uint32_t GetObjectIndex() const { return m_objectIndex; }
	float GetDetailScale() const { return m_detailScale; }
	uint32_t GetGrassDensity() const { return m_grassDensity; }
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	char* GetRawData() const { return m_rawData; }

	void SetType(EBitmapType type) { m_type = type; }
	void SetSize(uint32_t width, uint32_t height) { m_width = width; m_height = height; }
	void SetSourceSize(uint32_t width, uint32_t height) { m_sourceWidth = width; m_sourceHeight = height; }
	void SetRawData(char* rawData, size_t rawDatasize) { m_rawData = rawData; m_byteSize = (uint32_t)rawDatasize; }

	bool InitFromWLDData(SBitmapWLDData* wldData, Archive* archive, ResourceManager* resourceMgr);

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
	char*                    m_rawData = nullptr;
	uint32_t                 m_byteSize = 0;
};

struct STexture
{
	std::string              filename;
	uint32_t                 flags = 0;

	// This is what actually gets drawn as a texture
	std::shared_ptr<EQGBitmap> textures[8];
};

struct STextureSet
{
	uint32_t                 updateInterval = 0;
	steady_clock::time_point nextUpdate = steady_clock::now();
	uint32_t                 currentFrame = 0;
	std::vector<STexture>    textures;
	bool                     skipFrames = false;
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

	void InitFromEQMData(SEQMMaterial* eqm_material, SEQMFXParameter* eqm_fx_params, Archive* archive, const char* string_pool);
	bool InitFromWLDData(std::string_view tag, WLD_OBJ_MATERIALDEFINITION* pWLDMaterialDef, WLD_OBJ_SIMPLESPRITEINSTANCE* pSimpleSpriteInst,
		ParsedSimpleSpriteDef* pParsedSimpleSpriteDef, ParsedBMInfo* pParsedBMPalette);
	bool InitFromBitmap(const std::shared_ptr<EQGBitmap>& bitmap);

	std::string                 m_tag;
	std::string                 m_effectName;
	float                       m_twoSided = false;
	glm::vec2                   m_uvShift = glm::vec2(0.0f);
	uint32_t                    m_renderMethod = 0;
	float                       m_scaledAmbient = 0.0f;
	float                       m_constantAmbient = 0.0f;
	int                         m_type = 0;
	float                       m_detailScale = 1.0f;

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

private:
	std::string              m_tag;
	steady_clock::time_point m_lastUpdate = steady_clock::now();
	bool                     m_requiresUpdate = false;
	std::vector<PaletteData> m_materials;
};


} // namespace eqg
