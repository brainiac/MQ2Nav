#include "Camera.h"

#include <bx/math.h>
#include <glm/ext/matrix_transform.hpp>


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
			bx::cos(angleY) * bx::sin(angleX),
			bx::sin(angleY),
			-bx::cos(angleY) * bx::cos(angleX),
		};
		m_right = {
			bx::sin(angleX - bx::kPiHalf),
			0.0f,
			-bx::cos(angleX - bx::kPiHalf),
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

void Camera::GetViewMatrix(float* viewMatrix)
{
	bx::mtxLookAt(viewMatrix, bx::load<bx::Vec3>(&m_eye.x), bx::load<bx::Vec3>(&m_at.x), bx::load<bx::Vec3>(&m_up.x));
}
