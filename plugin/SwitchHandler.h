//
// SwitchHandler.h
//

#pragma once

#include "common/NavModule.h"

#include <mq/Plugin.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <chrono>

//----------------------------------------------------------------------------

class SwitchHandler : public NavModule
{
public:
	SwitchHandler();
	~SwitchHandler();

	using clock = std::chrono::system_clock;

	void SetActive(bool active);

	void OnPulse() override;

	void DebugUI();

private:
	void ActivateDoor(PDOOR door);

	bool m_active = false;
	bool m_activeOverride = false;

	clock::time_point m_fastUpdate = clock::now();
	clock::time_point m_slowUpdate = clock::now();
	const clock::duration m_slowUpdateInterval, m_fastUpdateInterval;

	float c_updateDistThreshold = 150.f;
	float c_triggerThreshold = 20.f;

	const clock::duration c_activationCooldown;

	PDOOR m_closestDoor = nullptr;
	std::unordered_map<uint8_t, float> m_distanceCache;
	glm::vec3 m_lastPos;

	struct DoorActivation
	{
		BYTE door_id;
		clock::time_point activation_time;
	};
	std::vector<DoorActivation> m_activations;
};
