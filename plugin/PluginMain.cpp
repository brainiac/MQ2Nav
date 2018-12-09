//
// PluginMain.cpp
//

#include "MQ2Navigation.h"

PreSetup("MQ2Nav");
PLUGIN_VERSION(1.22);

#define PLUGIN_NAME "MQ2Nav"
#define MQ2NAV_PLUGIN_VERSION "1.2.2"

#include <chrono>
#include <memory>

//----------------------------------------------------------------------------

std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

//----------------------------------------------------------------------------

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2Nav");

	WriteChatf(PLUGIN_MSG "v%s by brainiac (\aohttps://github.com/brainiac/MQ2Nav\ax)", MQ2NAV_PLUGIN_VERSION);

	g_mq2Nav.reset(new MQ2NavigationPlugin);
	g_mq2Nav->Plugin_Initialize();

	if (g_mq2Nav->InitializationFailed())
	{
		WriteChatf(PLUGIN_MSG "\arFailed to initialize plugin!");
	}
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("Shutting down MQ2Nav");

	g_mq2Nav->Plugin_Shutdown();
	g_mq2Nav.reset();
}

PLUGIN_API void OnPulse()
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnPulse();
}

PLUGIN_API void OnBeginZone()
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnBeginZone();
}

PLUGIN_API void OnEndZone()
{
	// OnZoned occurs later than OnEndZone, when more data is available
	//if (g_mq2Nav)
	//	g_mq2Nav->OnEndZone();
}

PLUGIN_API void OnZoned()
{
	// OnZoned occurs later than OnEndZone, when more data is available
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnEndZone();
}

PLUGIN_API void SetGameState(DWORD GameState)
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_SetGameState(GameState);
}

PLUGIN_API void OnAddGroundItem(PGROUNDITEM pNewGroundItem)
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnAddGroundItem(pNewGroundItem);
}

PLUGIN_API void OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnRemoveGroundItem(pGroundItem);
}

//============================================================================
// Wrappers for other plugins to access MQ2Nav

// Used to check if MQ2Nav is initialized.
// TODO check if this is needed, the other stuff should catch if this failed
PLUGIN_API bool NavInitialized()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized();
}

// Used to check if mesh is loaded
PLUGIN_API bool NavMeshLoaded()
{
	return g_mq2Nav && g_mq2Nav->IsMeshLoaded();
}

// Used to check if a path is active
PLUGIN_API bool NavPathActive()
{
	return g_mq2Nav && g_mq2Nav->IsActive();
}

 // Used to check if path is paused
PLUGIN_API bool NavPathPaused()
{
	return (g_mq2Nav && g_mq2Nav->IsPaused());
}

// Check if path is possible to the specified target
PLUGIN_API bool NavPossible(PCHAR szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		return g_mq2Nav->CanNavigateToPoint(szLine);
	}

	return false;
}

// Check path length
PLUGIN_API float NavPathLength(PCHAR szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		return g_mq2Nav->GetNavigationPathLength(szLine);
	}

	return -1;
}

// used to pass mq2nav commands
PLUGIN_API void NavCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		g_mq2Nav->Command_Navigate(pChar, szLine);
	}
}

//============================================================================
