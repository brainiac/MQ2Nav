#include "pch.h"
#include "meshgen/Entity.h"
#include "meshgen/Scene.h"
#include "meshgen/Components.h"

#include "entt/entity/handle.hpp"

static void ConvertToLocalSpace(const entt::handle& entity);
static void ConvertToWorldSpace(const entt::handle& entity);


TransformComponent& GetTransform(const entt::handle& entity)
{
	return entity.get<TransformComponent>();
}

entt::handle GetParent(const entt::handle& entity)
{
	return { *entity.registry(), entity.get<HierarchicalComponent>().parent };
}

void SetParent(const entt::handle& entity, const entt::handle& parent)
{
	entt::handle currentParent = GetParent(entity);
	if (currentParent == parent)
		return;

	// Remove from current parent's list of children.
	if (currentParent != entt::null)
	{
		std::erase(currentParent.get<HierarchicalComponent>().children, entity.entity());
	}

	auto& component = entity.get<HierarchicalComponent>();
	component.parent = parent;

	// Add self to parent's list of children.
	if (parent != entt::null)
	{
		auto& children = component.children;

		// Make sure child isn't already in the list
		if (std::ranges::find(children, entity.entity()) == children.end())
		{
			children.push_back(entity.entity());
		}
	}
}

std::vector<entt::entity>& GetChildren(const entt::handle& entity)
{
	return entity.get<HierarchicalComponent>().children;
}

bool RemoveChild(const entt::handle& entity, const entt::handle& child)
{
	auto& children = GetChildren(entity);

	if (auto iter = std::ranges::find(children, child.entity()); iter != end(children))
	{
		children.erase(iter);
		return true;
	}

	return false;
}

bool IsAncestorOf(const entt::handle& entity, const entt::handle& otherEntity)
{
	const auto& children = GetChildren(entity);

	if (children.empty())
		return false;

	if (std::ranges::find(children, otherEntity) != end(children))
		return true;

	for (const auto& child : children)
	{
		if (IsAncestorOf({ *entity.registry(), child }, otherEntity))
			return true;
	}

	return false;
}

bool IsDescendentOf(const entt::handle& entity, const entt::handle& otherEntity)
{
	return IsAncestorOf(otherEntity, entity);
}

void ParentEntity(const entt::handle& entity, const entt::handle& parent)
{
	if (IsDescendentOf(parent, entity))
	{
		UnparentEntity(parent);

		entt::handle newParent = GetParent(entity);
		if (newParent != entt::null)
		{
			UnparentEntity(entity);
			ParentEntity(parent, newParent);
		}
	}
	else
	{
		entt::handle prev = GetParent(entity);
		if (prev != entt::null)
		{
			UnparentEntity(entity);
		}
	}

	entity.get<HierarchicalComponent>().parent = parent;
	GetChildren(parent).push_back(entity);

	ConvertToLocalSpace(entity);
}

void UnparentEntity(const entt::handle& entity, bool convertToWorldspace /* = true */)
{
	entt::handle parent = GetParent(entity);
	if (parent == entt::null)
		return;

	std::erase(GetChildren(parent), entity);

	if (convertToWorldspace)
	{
		ConvertToWorldSpace(entity);
	}

	entity.get<HierarchicalComponent>().parent = entt::null;
}

static void ConvertToLocalSpace(const entt::handle& entity)
{
	entt::handle parent = GetParent(entity);
	if (parent == entt::null)
		return;

	auto& transformComponent = GetTransform(entity);

	glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
	glm::mat4 localTransform = glm::inverse(parentTransform) * transformComponent.GetMatrix();

	transformComponent.SetMatrix(localTransform);
}

static void ConvertToWorldSpace(const entt::handle& entity)
{
	entt::handle parent = GetParent(entity);
	if (parent == entt::null)
		return;

	glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
	GetTransform(entity).SetMatrix(transform);
}

glm::mat4 GetWorldSpaceTransformMatrix(const entt::handle& entity)
{
	glm::mat4 transform(1.0f);

	entt::handle parent = GetParent(entity);
	if (parent)
	{
		transform = GetWorldSpaceTransformMatrix(parent);
	}

	return transform * GetTransform(entity).GetMatrix();
}
