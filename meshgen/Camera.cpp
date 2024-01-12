#include "pch.h"
#include "Camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>

Camera::Camera()
{
	for (int i = 0; i < CameraDir_Count; ++i)
		m_dirs[i] = 0.0f;
}

Camera::~Camera()
{
}

void Camera::UpdateMouseLook(int deltaX, int deltaY)
{
	if (deltaX != 0)
	{
		m_horizontalAngle += m_mouseSpeed * static_cast<float>(deltaX);
	}

	if (deltaY != 0)
	{
		m_verticalAngle -= m_mouseSpeed * static_cast<float>(deltaY);
	}
}

void Camera::Update(float deltaTime)
{
	bool changed = false;

	if (m_horizontalAngle != m_lastHorizontalAngle || m_verticalAngle != m_lastVerticalAngle)
	{
		m_lastHorizontalAngle = m_horizontalAngle;
		m_lastVerticalAngle = m_verticalAngle;

		float angleX = glm::radians(m_horizontalAngle);
		float angleY = glm::radians(180 - m_verticalAngle);

		m_direction = {
			glm::cos(angleY) * glm::sin(angleX),
			glm::sin(angleY),
			-glm::cos(angleY) * glm::cos(angleX),
		};
		m_right = {
			glm::sin(angleX - glm::half_pi<float>()),
			0.0f,
			-glm::cos(angleX - glm::half_pi<float>()),
		};

		changed = true;
	}

	if ((m_keys & CameraKey_Movement) != 0 || m_moving)
	{
		m_moving = false;
		float speed = m_moveSpeed;
		if (m_keys & CameraKey_SpeedUp)
			speed *= m_speedMultiplier;

		for (int dir = 0; dir < CameraDir_Count; ++dir)
		{
			int flag = 1 << dir;
			m_dirs[dir] = glm::clamp(m_dirs[dir] + deltaTime * m_speedGrowthFactor * (m_keys & flag ? 1 : -1), 0.0f, 1.0f);
		}

		glm::vec3 deltaPosition = {
			m_dirs[CameraDir_Backward] - m_dirs[CameraDir_Forward],
			m_dirs[CameraDir_Left] - m_dirs[CameraDir_Right],
			m_dirs[CameraDir_Up] - m_dirs[CameraDir_Down]
		};

		if (deltaPosition.x != 0)
		{
			m_eye += m_direction * deltaTime * speed * deltaPosition.x;
			m_moving = true;
		}

		if (deltaPosition.y != 0)
		{
			m_eye += m_right * deltaTime * speed * deltaPosition.y;
			m_moving = true;
		}

		if (deltaPosition.z != 0)
		{
			// Straight up and down (not by the up vector)
			m_eye.y += deltaTime * speed * deltaPosition.z;
			m_moving = true;
		}

		changed = true;
	}

	if (changed || m_dirty)
	{
		m_at = m_eye + m_direction;
		m_up = glm::cross(m_right, m_direction);

		m_viewMtx = glm::lookAtLH(m_eye, m_at, m_up);
		m_projMtx = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);

		m_viewProjMtx = m_projMtx * m_viewMtx;
		m_dirty = false;
	}
}

void Camera::SetPosition(const glm::vec3& position)
{
	m_eye = position;
	m_dirty = true;
}

void Camera::SetHorizontalAngle(float angle)
{
	m_horizontalAngle = angle;
}

void Camera::SetVerticalAngle(float angle)
{
	m_verticalAngle = angle;
}

void Camera::SetKeyState(uint32_t key, bool down)
{
	if (down)
		m_keys |= key;
	else
		m_keys &= ~key;
}

void Camera::ClearKeyState()
{
	m_keys = 0;
}
