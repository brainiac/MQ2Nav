// MQ2NavSettings.h : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#ifndef __MQ2NAVUTIL_H
#define __MQ2NAVUTIL_H


#include "../MQ2Plugin.h"
#include "MQ2Navigation.h"

namespace mq2nav {
namespace util {
	// ----------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
    // CTD prevention function
    PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer || !pChSpawn->SpawnID || !initialized_ || (bCheckDead && pChSpawn->RespawnTimer > 0))
    {
        return false;
    }
    return true;
}


}
} // namespace mq2nav {


#endif