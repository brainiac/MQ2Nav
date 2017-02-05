//
// MQ2Nav_Util.h
//

#pragma once

#include "MQ2Navigation.h"

#include <functional>

namespace mq2nav {

//----------------------------------------------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
	// CTD prevention function
	return (GetGameState() == GAMESTATE_INGAME && GetCharInfo()->pSpawn && GetCharInfo2() && GetCharInfo() && GetCharInfo()->Stunned != 3);
}

} // namespace mq2nav

void ClickDoor(PDOOR pDoor);
