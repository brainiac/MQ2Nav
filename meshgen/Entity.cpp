#include "pch.h"
#include "meshgen/Entity.h"
#include "meshgen/Scene.h"
#include "meshgen/Components.h"

TransformComponent& Entity::GetTransform()
{
	return GetComponent<TransformComponent>();
}

const TransformComponent& Entity::GetTransform() const
{
	return GetComponent<TransformComponent>();
}

Entity Entity::GetParent() const
{
	return Entity(GetParentHandle(), m_scene);
}

void Entity::SetParent(Entity parent)
{
	Entity currentParent = GetParent();
	if (currentParent == parent)
		return;

	// Remove from current parent's list of children.
	if (currentParent)
	{
		std::erase(currentParent.GetComponent<HierarchicalComponent>().children, *this);
	}

	auto& comp = GetComponent<HierarchicalComponent>();
	comp.parent = parent;

	// Add self to parent's list of children.
	if (parent)
	{
		auto& children = comp.children;

		// Make sure the child isn't already in the list.
		if (std::ranges::find(children, *this) == children.end())
		{
			children.push_back(*this);
		}
	}
}

entt::entity Entity::GetParentHandle() const
{
	return GetComponent<HierarchicalComponent>().parent;
}

void Entity::SetParentHandle(entt::entity parent)
{
	GetComponent<HierarchicalComponent>().parent = parent;
}

std::vector<entt::entity>& Entity::GetChildren()
{
	return GetComponent<HierarchicalComponent>().children;
}

const std::vector<entt::entity>& Entity::GetChildren() const
{
	return GetComponent<HierarchicalComponent>().children;
}

void Entity::AddChild(Entity child)
{
	auto& children = GetChildren();

	// Make sure the child isn't already in the list.
	if (std::ranges::find(children, child) == children.end())
	{
		children.push_back(child);
	}
}

bool Entity::RemoveChild(Entity child)
{
	auto& children = GetChildren();

	if (auto iter = std::ranges::find(children, child.m_entityHandle); iter != end(children))
	{
		children.erase(iter);
		return true;
	}

	return false;
}

bool Entity::IsAncestorOf(Entity entity) const
{
	const auto& children = GetChildren();

	if (children.empty())
		return false;

	if (std::ranges::find(children, entity.m_entityHandle) != end(children))
		return true;

	for (const auto& child : children)
	{
		if (Entity{ child, m_scene }.IsAncestorOf(entity))
			return true;
	}

	return false;
}

bool Entity::IsDescendentOf(Entity entity) const
{
	return entity.IsAncestorOf(*this);
}

Entity::operator bool() const
{
	return m_entityHandle != entt::null && m_scene != nullptr && m_scene->m_registry.valid(m_entityHandle);
}
