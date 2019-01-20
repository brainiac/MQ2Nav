//
// SwitchHandler.cpp
//
// Handles proximity door/switch management during navigation.
//

#include "pch.h"
#include "SwitchHandler.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"

#include <imgui.h>

using namespace std::chrono_literals;

//============================================================================

inline bool SwitchIsTeleporter(uint8_t typeId)
{
	return typeId == 57
		|| typeId == 58;
}

inline bool SwitchIsStationary(uint8_t typeId)
{
	return typeId >= 50 && typeId <= 59;
}

inline bool SwitchIsElevator(uint8_t typeId)
{
	return typeId == 59;
}

SwitchHandler::SwitchHandler()
	: m_slowUpdateInterval(1s)
	, m_fastUpdateInterval(50ms)
	, c_activationCooldown(2s)
{
}

SwitchHandler::~SwitchHandler()
{
}

void SwitchHandler::SetActive(bool active)
{
	m_active = active;
}

inline bool ShouldAutoUseSwitch(PDOOR door)
{
	if (door && door->bVisible && door->bUsable
		&& !SwitchIsTeleporter(door->Type)
		&& door->Key == -1 /* todo: support keys */)
	{
		return true;
	}

	return false;
}

// accepts any callable of type void (PDOOR)
template <typename T>
inline void IterateDoors(const T& callable)
{
	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;

	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		PDOOR door = pDoorTable->pDoor[count];

		if (door)
		{
			callable(door);
		}
	}
}

void SwitchHandler::OnPulse()
{
	if (!m_activeOverride)
	{
		if (!m_active)
			return;
		if (!nav::GetSettings().open_doors)
			return;
	}

	auto now = clock::now();

	// we do fast updates on nearby doors and slow updates on further away doors.
	bool doFast = m_fastUpdate + m_fastUpdateInterval <= now;
	bool doSlow = m_slowUpdate + m_slowUpdateInterval <= now;

	PCHARINFO charInfo = GetCharInfo();
	if (!charInfo)
		return;

	// update distances if we moved
	glm::vec3 myPos(charInfo->pSpawn->X, charInfo->pSpawn->Y, charInfo->pSpawn->Z);
	bool updateDists = (m_lastPos != myPos);
	m_lastPos = myPos;

	// find X closest doors with distance closer than Y
	std::vector<PDOOR> closestDoors;

	IterateDoors([&](PDOOR door)
	{
		if (!ShouldAutoUseSwitch(door))
			return;

		glm::vec3 doorPos{ door->DefaultX, door->DefaultY, door->DefaultZ };
		float distance;

		auto it = m_distanceCache.find(door->ID);
		if (it == m_distanceCache.end())
		{
			distance = glm::distance(myPos, doorPos);
			m_distanceCache[door->ID] = distance;
		}
		else
		{
			// check if we should update distance. Do it if so.
			float& dist = it->second;
			if (updateDists && (dist >= c_updateDistThreshold && doSlow)
				|| (dist < c_updateDistThreshold && dist >= c_triggerThreshold && doFast)
				|| dist < c_triggerThreshold)
			{
				dist = glm::distance(myPos, doorPos);
			}

			distance = dist;
		}

		if (distance < c_triggerThreshold)
		{
			closestDoors.push_back(door);
		}
	});

	if (doFast) m_fastUpdate = now;
	if (doSlow) m_slowUpdate = now;

	for (auto door : closestDoors)
	{
		ActivateDoor(door);
	}
}

enum struct DoorState {
	Closed = 0,
	Open = 1,
	Opening = 2,
	Closing = 3
};

void SwitchHandler::ActivateDoor(PDOOR door)
{
	auto now = clock::now();

	// Check if we have this door currently tracked.
	auto iter = std::find_if(m_activations.begin(), m_activations.end(),
		[&](const DoorActivation& da) { return door->ID == da.door_id; });
	if (iter != m_activations.end())
	{
		// activation record exists. Check if the door was used recently.
		if (iter->activation_time + c_activationCooldown > now)
			return;

		iter->activation_time = now;
	}
	else
	{
		m_activations.push_back(DoorActivation{ door->ID, now });
	}

	if (door->State == (BYTE)DoorState::Closed
		|| door->State == (BYTE)DoorState::Open)
	{
		// TODO: Check if door is in front. If door is blocking us then we also need to close it
		ClickDoor(door);
	}
}

void SwitchHandler::DebugUI()
{
	ImGui::Checkbox("Force enabled", &m_activeOverride);

	bool enabled = m_activeOverride || (m_active && nav::GetSettings().open_doors);
	ImGui::TextColored(enabled ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", enabled ? "Active" : "Inactive");

	ImGui::DragFloat("Trigger distance", &c_triggerThreshold);

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	std::vector<PDOOR> doors;

	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		PDOOR door = pDoorTable->pDoor[count];

		if (door && m_distanceCache.count(door->ID) != 0)
		{
			doors.push_back(pDoorTable->pDoor[count]);
		}
	}

	std::sort(std::begin(doors), std::end(doors),
		[&](PDOOR a1, PDOOR a2)
	{
		return m_distanceCache[a1->ID] < m_distanceCache[a2->ID];
	});

	for (PDOOR door : doors)
	{
		// color: RED = excluded, GREEN = close enough to activate, YELLOW = won't activate (opening/closing)
		ImColor col(255, 255, 255);

		float distance = m_distanceCache[door->ID];
		if (distance < c_triggerThreshold)
		{
			if (door->State == (BYTE)DoorState::Opening
				|| door->State == (BYTE)DoorState::Closing)
			{
				col = ImColor(255, 255, 0);
			}
			else
			{
				col = ImColor(0, 255, 0);
			}
		}

		ImGui::TextColored(col, "%d: %s (%.2f)", door->ID, door->Name, distance);
	}

	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		PDOOR door = pDoorTable->pDoor[count];

		if (door && m_distanceCache.count(door->ID) == 0)
		{
			ImGui::TextColored(ImColor(255, 0, 0), "%d: %s", door->ID, door->Name);
		}
	}
}

//============================================================================
