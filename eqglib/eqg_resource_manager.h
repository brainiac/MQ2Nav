#pragma once

#include "eqg_geometry.h"
#include "eqg_light.h"
#include "eqg_resource.h"
#include "eqg_types_fwd.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace eqg {

class Animation;
class Archive;
class Bitmap;
class BlitSpriteDefinition;
class HierarchicalModelDefinition;
class HierarchicalModel;
class LightDefinition;
class ParticleActor;
class SimpleModelDefinition;
class SimpleModel;
class Terrain;

namespace detail
{
	template <class T>
	concept StringLike = std::is_convertible_v<T, std::string_view>;

	// Hash combine helper for building composite hash values
	template <class T>
	void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
}

template <class T>
concept ResourceConcept = std::is_base_of_v<Resource, T>&& requires(T t)
{
	{ t.GetStaticResourceType() } -> std::same_as<ResourceType>;
};

struct ResourceKey
{
	std::string tag;
	ResourceType type;

	ResourceKey() = default;
	ResourceKey(const ResourceKey&) = default;
	ResourceKey(ResourceKey&&) = default;
	ResourceKey& operator=(const ResourceKey&) = default;
	ResourceKey& operator=(ResourceKey&&) = default;

	template <detail::StringLike T>
	ResourceKey(const std::pair<T, ResourceType>& pair)
		: tag(pair.first), type(pair.second)
	{
	}

	struct Hasher
	{
		using is_transparent = void;

		using ResType = std::underlying_type_t<ResourceType>;

		template <detail::StringLike T>
		std::size_t operator()(const std::pair<T, ResourceType>& pair) const
		{
			size_t hash = 0;
			detail::hash_combine(hash, std::hash<std::string_view>()(std::string_view(pair.first)));
			detail::hash_combine(hash, std::hash<ResType>()(static_cast<ResType>(pair.second)));

			return hash;
		}

		std::size_t operator()(const ResourceKey& key) const
		{
			size_t hash = 0;
			detail::hash_combine(hash, std::hash<std::string_view>()(std::string_view(key.tag)));
			detail::hash_combine(hash, std::hash<ResType>()(static_cast<ResType>(key.type)));

			return hash;
		}
	};

	struct Equals
	{
		using is_transparent = void;

		bool operator()(const ResourceKey& lhs, const ResourceKey& rhs) const
		{
			return lhs.tag == rhs.tag && lhs.type == rhs.type;
		}

		template <detail::StringLike T>
		bool operator()(const ResourceKey& lhs, const std::pair<T, ResourceType>& rhs) const
		{
			return lhs.tag == rhs.first && lhs.type == rhs.second;
		}

		template <detail::StringLike T>
		bool operator()(const std::pair<T, ResourceType>& lhs, const ResourceKey& rhs) const
		{
			return lhs.first == rhs.tag && lhs.second == rhs.type;
		}
	};
};

using ResourceContainer = std::unordered_map<ResourceKey, std::shared_ptr<Resource>, ResourceKey::Hasher, ResourceKey::Equals>;

// Load flags are used by the loaders to modify their behavior from the client code.
enum LoadFlags
{
	LoadFlag_None             = 0x00,
	LoadFlag_GlobalLoad       = 0x01,
	LoadFlag_ItemAnims        = 0x02,
	LoadFlag_LuclinAnims      = 0x04,
	LoadFlag_SkipOldAnims     = 0x08,
	LoadFlag_OptimizeAnims    = 0x10,
	LoadFlag_SkipSocials      = 0x20,
};

// The resource manager manages all the resources created when loading data from eqg and s3d files.
// In its default state, it will not create graphics device resources. However, it can be subclassed
// to provide that functionality.
//
// Resource managers can be chained (i.e. global data exists in a different manager from zone data). This
// lets us load global data once and share it between multiple zones.

class ResourceManager
{
public:
	ResourceManager(const std::string& data_path, ResourceManager* parent = nullptr);
	virtual ~ResourceManager();

	// Enable global access to resource manager
	static void PushActive(ResourceManager* resourceMgr);
	static void PopActive();
	static ResourceManager* Get();

	// Retrieve resources from the manager
	std::shared_ptr<Resource> Get(std::string_view tag, ResourceType type) const;

	template <ResourceConcept T>
	std::shared_ptr<T> Get(std::string_view tag) const
	{
		return std::static_pointer_cast<T>(Get(tag, T::GetStaticResourceType()));
	}

	// Add resources to the manager
	bool Add(std::string_view tag, ResourceType type, const std::shared_ptr<Resource>& value);

	template <ResourceConcept T>
	bool Add(const std::shared_ptr<T>& value)
	{
		return Add(value->GetTag(), T::GetStaticResourceType(), value);
	}

	template <ResourceConcept T, detail::StringLike U>
	bool Add(const U& tag, const std::shared_ptr<T>& value)
	{
		return Add(tag, T::GetStaticResourceType(), value);
	}

	// Check if resources exist in the manager
	bool Contains(std::string_view tag, ResourceType type) const;

	// Factory creation of resource objects
	virtual std::shared_ptr<Bitmap> CreateBitmap() const;
	virtual std::shared_ptr<Bitmap> CreateBitmap(std::string_view fileName, Archive* archive, bool cubeMap = false, bool rawData = false);
	virtual std::shared_ptr<SimpleModelDefinition> CreateSimpleModelDefinition() const;
	virtual std::shared_ptr<SimpleModel> CreateSimpleModel() const;
	virtual std::shared_ptr<HierarchicalModelDefinition> CreateHierarchicalModelDefinition() const;
	virtual std::shared_ptr<HierarchicalModel> CreateHierarchicalModel() const;
	virtual std::shared_ptr<BlitSpriteDefinition> CreateBlitSpriteDefinition() const;
	virtual std::shared_ptr<Animation> CreateAnimation() const;
	virtual std::shared_ptr<LightDefinition> CreateLightDefinition() const;

	// Creating actor instances might be better off in a scene manager?
	virtual std::shared_ptr<SimpleActor> CreateSimpleActor(
		std::string_view actorTag,
		const std::shared_ptr<ActorDefinition>& simpleActorDef,
		const glm::vec3& position,
		const glm::vec3& orientation,
		float scale,
		ECollisionVolumeType collisionVolumeType,
		float boundingRadius = 1.0f,
		int actorIndex = -1,
		SDMRGBTrackWLDData* DMRGBTrackWLDData = nullptr,
		const std::span<uint32_t>& RGBs = {},
		std::string_view actorName = "");

	virtual std::shared_ptr<SimpleActor> CreateSimpleActor(
		std::string_view actorTag,
		const std::shared_ptr<ActorDefinition>& simpleActorDef,
		int actorIndex = -1,
		bool useDefaultBoundingRadius = false,
		std::string_view actorName = "");

	virtual std::shared_ptr<HierarchicalActor> CreateHierarchicalActor(
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		const glm::vec3& position,
		const glm::vec3& orientation,
		float scale,
		ECollisionVolumeType collisionVolumeType,
		float boundingRadius = 1.0f,
		int actorIndex = -1,
		SDMRGBTrackWLDData* DMRGBTrackWLDData = nullptr,
		const std::span<uint32_t>& RGBs = {},
		std::string_view actorName = "");

	virtual std::shared_ptr<HierarchicalActor> CreateHierarchicalActor(
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		int actorIndex = -1,
		bool allSkinsActive = false,
		bool useDefaultBoundingRadius = false,
		bool sharedBoneGroups = true,
		Bone* pBone = nullptr,
		std::string_view actorName = "");

	virtual std::shared_ptr<ParticleActor> CreateParticleActor(
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		int actorIndex = -1,
		bool allSkinsActive = false,
		Bone* pBone = nullptr);

	virtual std::shared_ptr<Terrain> CreateTerrain() const;

	virtual std::shared_ptr<PointLight> CreatePointLight(
		std::string_view name,
		const std::shared_ptr<LightDefinition>& lightDefinition,
		const glm::vec3& position,
		float radius);

	virtual bool LoadTexture(Bitmap* bitmap, Archive* archive);
	virtual bool LoadBitmapData(Bitmap* bitmap, Archive* archive);

	bool ReadFile(std::string_view filePath, std::vector<char>& data);
	std::unique_ptr<uint8_t[]> ReadFile(std::string_view filePath, uint32_t& size);

	//===============================================================================================

	void SetTerrain(const TerrainPtr& terrain) { m_terrain = terrain; }
	TerrainPtr GetTerrain() const { return m_terrain; }
	TerrainPtr GetOrCreateTerrain()
	{
		if (!m_terrain)
			m_terrain = CreateTerrain();
		return m_terrain;
	}

	void SetTerrainSystem(const TerrainSystemPtr& terrainSystem) { m_terrainSystem = terrainSystem; }
	TerrainSystemPtr GetTerrainSystem() const { return m_terrainSystem; }

	void AddActor(const ActorPtr& actor) { m_actors.push_back(actor); }
	const std::vector<ActorPtr>& GetActors() const { return m_actors; }

	void AddLight(const PointLightPtr& light) { m_lights.push_back(light); }
	const std::vector<PointLightPtr>& GetLights() const { return m_lights; }

	// Get all resources of a specific type
	const std::map<std::string_view, std::shared_ptr<Resource>>& GetResourcesByType(ResourceType type) const
	{
		static const std::map<std::string_view, std::shared_ptr<Resource>> s_empty;
		auto it = m_sortedResources.find(type);
		if (it != m_sortedResources.end())
			return it->second;
		return s_empty;
	}

private:
	std::string m_dataPath;
	ResourceManager* m_parent;
	ResourceContainer m_resources;

	// Track resources by type and by tag (sorted). This is useful for debugging and
	// inspecting, but we'll use the ResourceContainer for actual lookups.
	std::map<ResourceType, std::map<std::string_view, std::shared_ptr<Resource>>> m_sortedResources;

	// There is only one terrain per zone.
	TerrainPtr m_terrain;
	TerrainSystemPtr m_terrainSystem;

	std::vector<ActorPtr> m_actors;
	std::vector<PointLightPtr> m_lights;
};

struct ScopedUseResourceManager
{
	ScopedUseResourceManager(ResourceManager* resourceMgr)
	{
		ResourceManager::PushActive(resourceMgr);
	}

	~ScopedUseResourceManager()
	{
		ResourceManager::PopActive();
	}
};

} // namespace eqg
