
#pragma once

#include "meshgen/Entity.h"
#include "meshgen/Scene.h"

inline entt::registry& Entity::GetRegistry() const
{
	return m_scene->m_registry;
}

template <typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
	return m_scene->m_registry.emplace<T>(m_entityHandle, std::forward<Args>(args)...);
}

template <typename T>
T& Entity::GetComponent()
{
	return m_scene->m_registry.get<T>(m_entityHandle);
}

template <typename T>
const T& Entity::GetComponent() const
{
	return m_scene->m_registry.get<T>(m_entityHandle);
}

template <typename... T>
bool Entity::HasComponent() const
{
	return m_scene->m_registry.all_of<T...>(m_entityHandle);
}

template <typename... T>
bool Entity::HasAny() const
{
	return m_scene->m_registry.any_of<T...>(m_entityHandle);
}

template <typename T>
void Entity::RemoveComponent()
{
	m_scene->m_registry.remove<T>(m_entityHandle);
}

template <typename T>
bool Entity::TryRemoveComponent()
{
	if (HasComponent<T>())
	{
		RemoveComponent<T>();
		return true;
	}
	
	return false;
}
