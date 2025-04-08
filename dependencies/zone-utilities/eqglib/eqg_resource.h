#pragma once

#include <string>

namespace eqg {

class ResourceManager;

enum class ResourceType
{
	Bitmap,
	Material,
	MaterialPalette,

	BlitSpriteDefinition,
	Animation,
	LightDefinition,

	SimpleModelDefinition,
	HierarchicalModelDefinition,
	ParticleCloudDefinition,
	ActorDefinition,

	LODList,

	Max,
};

// Base class for resources loaded by the eqg resource system
class Resource
{
public:
	Resource(ResourceType type) : m_resourceType(type) {}
	virtual ~Resource() = default;

	ResourceType GetResourceType() const { return m_resourceType; }
	virtual std::string_view GetTag() const { return ""; }

private:
	ResourceType m_resourceType;
};

} // namespace eqg
