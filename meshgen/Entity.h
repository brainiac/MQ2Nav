
#pragma once

#include "meshgen/Components.h"

#include <entt/entity/fwd.hpp>

struct TransformComponent;
class Scene;

class Entity
{
public:
	Entity() = default;
	Entity(entt::entity handle, Scene* scene)
		: m_entityHandle(handle), m_scene(scene) {}

	~Entity() = default;

	const entt::entity& GetHandle() const { return m_entityHandle; }

	entt::registry& GetRegistry() const;

	template <typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template <typename T>
	T& GetComponent();

	template <typename T>
	const T& GetComponent() const;

	template <typename... T>
	bool HasComponent() const;

	template <typename... T>
	bool HasAny() const;

	template <typename T>
	void RemoveComponent();

	template <typename T>
	bool TryRemoveComponent();

	operator entt::entity() const { return m_entityHandle; }
	explicit operator bool() const;

	bool operator==(const Entity& other) const
	{
		return m_entityHandle == other.m_entityHandle && m_scene == other.m_scene;
	}

	bool operator!=(const Entity& other) const
	{
		return !(*this == other);
	}

	TransformComponent& GetTransform();
	const TransformComponent& GetTransform() const;

	Entity GetParent() const;
	void SetParent(Entity parent);

	entt::entity GetParentHandle() const;
	void SetParentHandle(entt::entity parent);

	std::vector<entt::entity>& GetChildren();
	const std::vector<entt::entity>& GetChildren() const;

	void AddChild(Entity child);
	bool RemoveChild(Entity child);

	bool IsAncestorOf(Entity entity) const;
	bool IsDescendentOf(Entity entity) const;

private:
	entt::entity m_entityHandle = entt::null;
	Scene* m_scene = nullptr;
};
