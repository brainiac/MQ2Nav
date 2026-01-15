
#pragma once

#include "mq/base/Color.h"

#include "entt/entity/registry.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/matrix_decompose.hpp"

namespace eqg
{
	class Actor;
}

struct IdentityComponent
{
	std::string name;
};

enum class EntityType
{
	Actor,
	Area,
	PointLight,

	Unspecified,
};

struct EntityTypeComponent
{
	EntityType type = EntityType::Unspecified;
};

struct HiddenComponent
{
};

struct HierarchicalComponent
{
	entt::entity parent = entt::null;
	std::vector<entt::entity> children;
};

struct TransformComponent
{
	TransformComponent() = default;
	TransformComponent(const TransformComponent& other) = default;

	explicit TransformComponent(const glm::vec3& pos)
		: position(pos)
	{
	}

	glm::mat4 GetMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), position)
			* glm::toMat4(rotation_)
			* glm::scale(glm::mat4(1.0f), scale);
	}

	void SetMatrix(const glm::mat4& transform)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(transform, scale, rotation_, position, skew, perspective);
		rotationEuler_ = glm::eulerAngles(rotation_);
	}

	const glm::vec3& GetRotationEuler() const { return rotationEuler_; }
	void SetRotationEuler(const glm::vec3& euler)
	{
		rotationEuler_ = euler;
		rotation_ = glm::quat(euler);
	}

	const glm::quat& GetRotation() const { return rotation_; }
	void SetRotation(const glm::quat& rotation)
	{
		glm::vec3 old_rotation = rotationEuler_;
		rotation_ = rotation;
		rotationEuler_ = glm::eulerAngles(rotation);

		if ((fabs(rotationEuler_.x - old_rotation.x) == glm::pi<float>())
			&& (fabs(rotationEuler_.z - old_rotation.z) == glm::pi<float>()))
		{
			rotationEuler_ = {
				old_rotation.x,
				glm::pi<float>() - rotationEuler_.y,
				old_rotation.z
			};
		}
	}

	// position
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };

	// scale
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

private:
	// rotation
	glm::quat rotation_ = { 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 rotationEuler_ = { 0.0f, 0.0f, 0.0f };

public:
	__declspec(property(get = GetRotation, put = SetRotation)) glm::quat rotation;
	__declspec(property(get = GetRotationEuler, put = SetRotationEuler)) glm::vec3 rotationEuler;
};

struct TagComponent
{
	std::string tag;
};

//============================================================================
// Render components

struct RenderBoundingBoxComponent
{
	mq::MQColor color;
};

struct RenderConvexHulls
{
	// Hull geometry for rendering
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> triangleIndices;  // Triangulated faces
	std::vector<std::pair<uint16_t, uint16_t>> edges;  // Wireframe edges
	mq::MQColor color;
};
