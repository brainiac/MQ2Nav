//
// Camera.h
//

#pragma once

#include <glm/glm.hpp>

enum CameraKeyState
{
	CameraKey_Forward  = 0x0001,
	CameraKey_Backward = 0x0002,
	CameraKey_Left     = 0x0004,
	CameraKey_Right    = 0x0008,
	CameraKey_Up       = 0x0010,
	CameraKey_Down     = 0x0020,

	CameraKey_Movement = 0x003f,
	CameraKey_SpeedUp  = 0x0100,
};

enum CameraDirection
{
	CameraDir_Forward,
	CameraDir_Backward,
	CameraDir_Left,
	CameraDir_Right,
	CameraDir_Up,
	CameraDir_Down,

	CameraDir_Count
};

class Camera
{
public:
	Camera();
	Camera(float fov, uint32_t viewportWidth, uint32_t viewportHeight, float nearPlane, float farPlane);
	~Camera();

	void SetPosition(const glm::vec3& position);
	glm::vec3 GetPosition() const { return m_eye; }

	void SetYaw(float angle) { m_yawAngle = angle; }
	float GetYaw() const { return m_yawAngle; }

	void SetPitch(float angle) { m_pitchAngle = angle; }
	float GetPitch() const { return m_pitchAngle; }

	void SetFieldOfView(float fov)
	{
		if (m_fov == fov)
			return;
		SetPerspectiveProjectionMatrix(fov, static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight), m_nearPlane, m_farPlane);
		m_fov = fov;
	}
	float GetFieldOfView() const { return m_fov; }

	void SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_viewportWidth == width && m_viewportHeight == height)
			return;

		SetPerspectiveProjectionMatrix(m_fov, static_cast<float>(width), static_cast<float>(height), m_nearPlane, m_farPlane);
		m_viewportWidth = width;
		m_viewportHeight = height;
	}
	glm::ivec2 GetViewportSize() const { return { m_viewportWidth, m_viewportHeight }; }

	void SetNearClipPlane(float nearPlane)
	{
		if (m_nearPlane == nearPlane)
			return;

		SetPerspectiveProjectionMatrix(m_fov, static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight), nearPlane, m_farPlane);
		m_nearPlane = nearPlane;
	}
	float GetNearClipPlane() const { return m_nearPlane; }

	void SetFarClipPlane(float farPlane)
	{
		if (m_farPlane == farPlane)
			return;
		SetPerspectiveProjectionMatrix(m_fov, static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight), m_nearPlane, farPlane);
		m_farPlane = farPlane;
	}
	float GetFarClipPlane() const { return m_farPlane; }

	void SetKeyState(uint32_t key, bool down);
	void ClearKeyState();

	const glm::mat4& GetViewMtx() const { return m_viewMtx; }
	const glm::mat4& GetProjMtx() const { return m_projMtx; }
	const glm::mat4& GetViewProjMtx() const { return m_viewProjMtx; }

	const glm::vec3& GetLookingAt() const { return m_at; }
	const glm::vec3& GetDirection() const { return m_direction; }

	void UpdateMouseLook(int deltaX, int deltaY);

	void Update(float deltaTime = 0.0f);

	const glm::vec3& GetUp() const { return m_up; }
	const glm::vec3& GetRight() const { return m_right; }

private:
	void SetPerspectiveProjectionMatrix(float fov, float width, float height, float nearPlane, float farPlane);

private:
	glm::vec3 m_direction;
	glm::vec3 m_right;
	
	glm::vec3 m_eye;
	glm::vec3 m_at;
	glm::vec3 m_up;
	float m_dirs[CameraDir_Count];
	bool m_dirty = false;
	bool m_moving = false;

	glm::mat4 m_viewMtx;
	glm::mat4 m_viewProjMtx;

	glm::mat4 m_projMtx;
	uint32_t m_viewportWidth;
	uint32_t m_viewportHeight;
	float m_fov = 50;
	float m_nearPlane = 1.0f;
	float m_farPlane = 1000.0f;

	// rotational angle, in degrees
	float m_yawAngle = 0.0f;
	float m_pitchAngle = 0.0f;

	// last rotational angle
	float m_lastYawAngle = 0.0f;
	float m_lastPitchAngle = 0.0f;

	float m_mouseSpeed = 0.25f;
	float m_moveSpeed = 250.0f;
	float m_speedMultiplier = 10.0f;
	float m_speedGrowthFactor = 4.0f;

	uint32_t m_keys = 0;
};
