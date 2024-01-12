#pragma once

#include <glm/glm.hpp>
#include <SDL2/SDL_events.h>

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
	~Camera();

	void SetPosition(const glm::vec3& position);
	glm::vec3 GetPosition() const { return m_eye; }

	void SetHorizontalAngle(float angle);
	float GetHorizontalAngle() const { return m_horizontalAngle; }

	void SetVerticalAngle(float angle);
	float GetVerticalAngle() const { return m_verticalAngle; }

	void SetFieldOfView(float fov) { m_fov = fov; m_dirty = true; }
	float GetFieldOfView() const { return m_fov; }

	void SetAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; m_dirty = true; }
	float GetAspectRatio() const { return m_aspectRatio; }

	void SetNearClipPlane(float nearPlane) { m_nearPlane = nearPlane; m_dirty = true; }
	float GetNearClipPlane() const { return m_nearPlane; }

	void SetFarClipPlane(float farPlane) { m_farPlane = farPlane; m_dirty = true; }
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
	glm::vec3 m_direction;
	glm::vec3 m_right;

	glm::vec3 m_eye;
	glm::vec3 m_at;
	glm::vec3 m_up;
	float m_dirs[CameraDir_Count];
	bool m_dirty = false;
	bool m_moving = false;

	glm::mat4 m_viewMtx;
	glm::mat4 m_projMtx;
	glm::mat4 m_viewProjMtx;
	float m_aspectRatio = 1.0f;
	float m_fov = 50;
	float m_nearPlane = 1.0f;
	float m_farPlane = 1000.0f;

	// rotational angle, in degrees
	float m_horizontalAngle = 0.0f;
	float m_verticalAngle = 0.0f;

	// last rotational angle
	float m_lastHorizontalAngle = 0.0f;
	float m_lastVerticalAngle = 0.0f;

	float m_mouseSpeed = 0.25f;
	float m_moveSpeed = 250.0f;
	float m_speedMultiplier = 10.0f;
	float m_speedGrowthFactor = 4.0f;

	uint32_t m_keys = 0;
};
