#pragma once

#include <memory>

namespace eqg {

class ActorDefinition;
using ActorDefinitionPtr = std::shared_ptr<ActorDefinition>;

class Archive;
using ArchivePtr = std::shared_ptr<Archive>;

class Bitmap;
using BitmapPtr = std::shared_ptr<Bitmap>;

class LightDefinition;
using LightDefinitionPtr = std::shared_ptr<LightDefinition>;

class LODList;
using LODListPtr = std::shared_ptr<LODList>;
struct LODListElement;
using LODListElementPtr = std::shared_ptr<LODListElement>;

class Material;
using MaterialPtr = std::shared_ptr<Material>;

class MaterialPalette;
using MaterialPalettePtr = std::shared_ptr<MaterialPalette>;

class PointLight;
using PointLightPtr = std::shared_ptr<PointLight>;

class ResourceManager;

class SimpleActor;
using SimpleActorPtr = std::shared_ptr<SimpleActor>;

class WaterSheet;
using WaterSheetPtr = std::shared_ptr<WaterSheet>;

class WaterSheetData;
using WaterSheetDataPtr = std::shared_ptr<WaterSheetData>;

} // namespace eqg
