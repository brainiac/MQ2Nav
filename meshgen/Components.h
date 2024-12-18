
#pragma once

#include <entt/entity/registry.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct IdentComponent
{
	std::string name;
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
		: translation(pos)
	{
	}

	glm::mat4 GetTransform() const
	{
		return glm::translate(glm::mat4(1.0f), translation)
			* glm::toMat4(rotation_)
			* glm::scale(glm::mat4(1.0f), scale);
	}

	void SetTransform(const glm::mat4& transform)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(transform, scale, rotation_, translation, skew, perspective);
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

	// translation
	glm::vec3 translation = { 0.0f, 0.0f, 0.0f };

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
