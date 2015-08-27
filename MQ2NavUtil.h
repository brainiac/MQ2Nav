// MQ2NavSettings.h
//

#pragma once

#include "../MQ2Plugin.h"
#include "MQ2Navigation.h"

namespace mq2nav {
namespace util {

//----------------------------------------------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
    // CTD prevention function
    PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
    if (GetGameState() != GAMESTATE_INGAME
        || !pLocalPlayer
        || !pChSpawn->SpawnID
        || (bCheckDead && pChSpawn->RespawnTimer > 0))
    {
        return false;
    }
    return true;
}
//----------------------------------------------------------------------------

} // namespace util
} // namespace mq2nav
