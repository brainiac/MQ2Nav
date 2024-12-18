
#include "pch.h"
#include "meshgen/Scene.h"

#include "meshgen/Components.h"

Scene::Scene(const std::string& name)
	: m_name(name)
{
	m_sceneEntity = m_registry.create();
	m_registry.emplace<IdentComponent>(m_sceneEntity, name);
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

Entity Scene::CreateEntity(const std::string& name)
{
	return CreateEntityWithParent({}, name);
}

Entity Scene::CreateEntityWithParent(Entity parent, const std::string& name)
{
	Entity entity = Entity{ m_registry.create(), this };
	
	if (name.empty())
	{
		entity.AddComponent<IdentComponent>(name);
	}

	entity.AddComponent<TransformComponent>();
	entity.AddComponent<HierarchicalComponent>(parent);

	if (parent)
	{
		entity.SetParent(parent);
	}

	return entity;
}

void Scene::DestroyEntity(const Entity& entity)
{
	if (!entity)
		return;

	// Callback to destroyed entity?


	// Destroy children
	auto children = entity.GetChildren();
	for (entt::entity child : children)
	{
		DestroyEntity(Entity{ child, this });
	}

	// Remove self from parent
	if (auto parent = entity.GetParent())
	{
		parent.RemoveChild(entity);
	}

	m_registry.destroy(entity);
}

void Scene::ConvertToLocalSpace(Entity entity)
{
	Entity parent = entity.GetParent();
	if (!parent)
		return;

	auto& transformComponent = entity.GetTransform();

	glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
	glm::mat4 localTransform = glm::inverse(parentTransform) * transformComponent.GetTransform();

	transformComponent.SetTransform(localTransform);
}

void Scene::ConvertToWorldSpace(Entity entity)
{
	Entity parent = entity.GetParent();
	if (!parent)
		return;

	glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
	entity.GetTransform().SetTransform(transform);
}

glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity) const
{
	glm::mat4 transform(1.0f);

	Entity parent = entity.GetParent();
	if (parent)
	{
		transform = GetWorldSpaceTransformMatrix(parent);
	}

	return transform * entity.GetTransform().GetTransform();
}

void Scene::ParentEntity(Entity entity, Entity parent)
{
	if (parent.IsDescendentOf(entity))
	{
		UnparentEntity(parent);

		if (Entity newParent = entity.GetParent())
		{
			UnparentEntity(entity);
			ParentEntity(parent, newParent);
		}
	}
	else
	{
		if (Entity prev = entity.GetParent())
		{
			UnparentEntity(entity);
		}
	}

	entity.SetParentHandle(parent);
	parent.GetChildren().push_back(entity.GetHandle());

	ConvertToLocalSpace(entity);
}

void Scene::UnparentEntity(Entity entity, bool convertToWorldspace)
{
	Entity parent = entity.GetParent();
	if (!parent)
		return;

	std::erase(parent.GetChildren(), entity.GetHandle());

	if (convertToWorldspace)
		ConvertToWorldSpace(entity);

	entity.SetParentHandle(entt::null);
}
