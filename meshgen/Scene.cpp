
#include "pch.h"
#include "meshgen/Scene.h"

#include "meshgen/Components.h"

#include "entt/entity/handle.hpp"

Scene::Scene(const std::string& name)
	: m_name(name)
{
	m_sceneEntity = m_registry.create();
	m_registry.emplace<NameComponent>(m_sceneEntity, name);
}

Scene::~Scene()
{
	m_registry.clear();
}

void Scene::OnUpdate(float timeStep)
{
}

void Scene::OnRender(const std::shared_ptr<Renderer>& renderer, float timeStep, const Camera& camera)
{
}

entt::handle Scene::CreateEntity(const std::string_view& name)
{
	return CreateEntityWithParent(entt::null, name);
}

entt::handle Scene::CreateEntityWithParent(const entt::entity& parent, const std::string_view& name)
{
	entt::entity entity = m_registry.create();
	
	if (!name.empty())
	{
		m_registry.emplace<NameComponent>(entity, std::string{ name });
	}

	[[maybe_unused]] auto& xform = m_registry.emplace<TransformComponent>(entity);
	[[maybe_unused]] auto& hierarchy = m_registry.emplace<HierarchicalComponent>(entity, parent);

	if (m_registry.valid(parent))
	{
		SetParent({ m_registry, entity }, { m_registry, parent });
	}

	return { m_registry, entity };
}

void Scene::DestroyEntity(const entt::entity& entity)
{
	if (!m_registry.valid(entity))
		return;

	// Callback to destroyed entity?
	entt::handle handle{ m_registry, entity };

	// Destroy children
	auto& children = GetChildren(handle);
	for (entt::entity child : children)
	{
		DestroyEntity(child);
	}

	// Remove self from parent
	if (auto parent = GetParent(handle))
	{
		RemoveChild(parent, handle);
	}

	handle.destroy();
}

void Scene::Clear()
{
	m_registry.clear();
}
