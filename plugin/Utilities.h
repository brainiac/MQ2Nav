//
// Utilities.h
//

#pragma once

#include "plugin/MQ2Navigation.h"

#include <functional>

namespace nav {

//----------------------------------------------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
	// CTD prevention function
	return (GetGameState() == GAMESTATE_INGAME && GetCharInfo()->pSpawn && GetPcProfile() && GetCharInfo() && GetCharInfo()->Stunned != 3);
}

} // namespace nav

void ClickDoor(PDOOR pDoor);
