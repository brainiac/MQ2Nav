//
// Scene.h
//

#pragma once

#include "meshgen/Entity.h"

#include <entt/entity/registry.hpp>

class Entity;
class Camera;
class Renderer;

class Scene
{
	friend class Entity;

public:
	explicit Scene(const std::string& name);
	virtual ~Scene();

	const std::string GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

	void OnUpdate(float timeStep);
	void OnRender(const std::shared_ptr<Renderer>& renderer, float timeStep, const Camera& camera);

	Entity CreateEntity(const std::string& name = std::string());
	Entity CreateEntityWithParent(Entity parent, const std::string& name = std::string());

	void DestroyEntity(const Entity& entity);

	template<typename... Components>
	auto GetAllEntitiesWith()
	{
		return m_registry.view<Components...>();
	}

	void ConvertToLocalSpace(Entity entity);
	void ConvertToWorldSpace(Entity entity);
	glm::mat4 GetWorldSpaceTransformMatrix(Entity entity) const;

	void ParentEntity(Entity entity, Entity parent);
	void UnparentEntity(Entity entity, bool convertToWorldspace = true);

protected:
	entt::entity m_sceneEntity = entt::null;
	entt::registry m_registry;

	std::string m_name;
};

#include "meshgen/Entity.inl"
