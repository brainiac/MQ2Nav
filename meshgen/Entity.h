
#pragma once

#include "meshgen/Components.h"

#include "entt/entity/fwd.hpp"

struct TransformComponent;
class Scene;

// TransformComponent management
TransformComponent& GetTransform(const entt::handle& entity);

// HierarchicalComponent management

entt::handle GetParent(const entt::handle& entity);
std::vector<entt::entity>& GetChildren(const entt::handle& entity);

// Check if `entity` is an ancestor or descendent of this `otherEntity`.
bool IsAncestorOf(const entt::handle& entity, const entt::handle& otherEntity);
bool IsDescendentOf(const entt::handle& entity, const entt::handle& otherEntity);

void ParentEntity(const entt::handle& entity, const entt::handle& parent);
void UnparentEntity(const entt::handle& entity, bool convertToWorldspace = true);

void SetParent(const entt::handle& entity, const entt::handle& parent);
bool RemoveChild(const entt::handle& entity, const entt::handle& child);


// Helpers

glm::mat4 GetWorldSpaceTransformMatrix(const entt::handle& entity);
