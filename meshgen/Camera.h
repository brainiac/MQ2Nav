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

	void SetKeyState(uint32_t key, bool down);
	void ClearKeyState();

	void GetViewMatrix(float* viewMatrix);

	glm::vec3 GetLookingAt() const { return m_at; }

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

	glm::mat4x4 m_viewMatrix;

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
