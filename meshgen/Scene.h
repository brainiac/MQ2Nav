//
// Scene.h
//

#pragma once

#include "meshgen/Entity.h"

#include "entt/entity/registry.hpp"

class Camera;
class Renderer;

class Scene
{
public:
	explicit Scene(const std::string& name);
	virtual ~Scene();

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

	void Clear();

	void OnUpdate(float timeStep);
	void OnRender(const std::shared_ptr<Renderer>& renderer, float timeStep, const Camera& camera);

	entt::handle CreateEntity(const std::string_view& name = std::string_view());
	entt::handle CreateEntityWithParent(const entt::entity& parent, const std::string_view& name = std::string_view());

	void DestroyEntity(const entt::entity& entity);

	template<typename... Components>
	auto GetAllEntitiesWith()
	{
		return m_registry.view<Components...>();
	}

	entt::registry& GetRegistry() { return m_registry; }

protected:
	std::string m_name;

	entt::entity m_sceneEntity = entt::null;
	entt::registry m_registry;
};
