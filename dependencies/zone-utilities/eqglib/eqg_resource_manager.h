#pragma once

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace eqg {

class Archive;
class EQGBitmap;

namespace detail
{
	template <class T>
	concept StringLike = std::is_convertible_v<T, std::string_view>;
}

enum class ResourceType
{
	Bitmap,
	Material,
	MaterialPalette,

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

		template <detail::StringLike T>
		std::size_t operator()(const std::pair<T, ResourceType>& pair) const
		{
			std::size_t h1 = std::hash<std::string_view>()(std::string_view(pair.first));
			std::size_t h2 = std::hash<std::underlying_type_t<ResourceType>>()(static_cast<std::underlying_type_t<ResourceType>>(pair.second));

			return h1 ^ h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
		}

		std::size_t operator()(const ResourceKey& key) const
		{
			std::size_t h1 = std::hash<std::string>()(key.tag);
			std::size_t h2 = std::hash<std::underlying_type_t<ResourceType>>()(static_cast<std::underlying_type_t<ResourceType>>(key.type));

			return h1 ^ h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
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

// The resource manager manages all the resources created when loading data from eqg and s3d files.
// In its default state, it will not create graphics device resources. However, it can be subclassed
// to provide that functionality.
//
// Resource managers can be chained (i.e. global data exists in a different manager from zone data). This
// lets us load global data once and share it between multiple zones.

class ResourceManager
{
public:
	ResourceManager(ResourceManager* parent = nullptr);
	virtual ~ResourceManager();

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
	virtual std::shared_ptr<EQGBitmap> CreateBitmap() const;

	virtual bool CreateTexture(EQGBitmap* bitmap, Archive* archive) { return true; }
	virtual bool LoadBitmapData(EQGBitmap* bitmap, Archive* archive);

private:
	ResourceManager* m_parent;
	ResourceContainer m_resources;

	// Track resources by type and by tag (sorted). This is useful for debugging and
	// inspecting, but we'll use the ResourceContainer for actual lookups.
	std::map<ResourceType, std::map<std::string_view, std::shared_ptr<Resource>>> m_sortedResources;
};

} // namespace eqg

