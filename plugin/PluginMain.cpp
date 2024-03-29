//
// PluginMain.cpp
//

#include "pch.h"

#include "plugin/MQ2Navigation.h"
#include "../PluginAPI.h"

PreSetup("MQ2Nav");
PLUGIN_VERSION(1.33);

#define PLUGIN_NAME "MQ2Nav"
#define MQ2NAV_PLUGIN_VERSION "1.3.3"

#include <chrono>
#include <memory>

//----------------------------------------------------------------------------

MQ2NavigationPlugin* g_mq2Nav;

//----------------------------------------------------------------------------

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2Nav");

	WriteChatf(PLUGIN_MSG "v%s by brainiac (\aohttps://github.com/brainiac/MQ2Nav\ax)", MQ2NAV_PLUGIN_VERSION);

	g_mq2Nav = new MQ2NavigationPlugin();
	g_mq2Nav->Plugin_Initialize();
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("Shutting down MQ2Nav");

	g_mq2Nav->Plugin_Shutdown();

	delete g_mq2Nav;
	g_mq2Nav = nullptr;
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

PLUGIN_API void OnAddSpawn(PSPAWNINFO pNewSpawn)
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnAddSpawn(pNewSpawn);
}

PLUGIN_API void OnRemoveSpawn(PSPAWNINFO pSpawn)
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnRemoveSpawn(pSpawn);
}

PLUGIN_API void OnUpdateImGui()
{
	if (g_mq2Nav)
		g_mq2Nav->Plugin_OnUpdateImGui();
}

//============================================================================
// Wrappers for other plugins to access MQ2Nav

// Used to check if MQ2Nav is initialized.
NAV_API bool IsNavInitialized()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized();
}

// Used to check if mesh is loaded
NAV_API bool IsNavMeshLoaded()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized() && g_mq2Nav->IsMeshLoaded();
}

// Used to check if a path is active
NAV_API bool IsNavPathActive()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized() && g_mq2Nav->IsActive();
}

 // Used to check if path is paused
NAV_API bool IsNavPathPaused()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized() && g_mq2Nav->IsPaused();
}

// Check if path is possible to the specified target
NAV_API bool IsNavPossible(const char* szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		return g_mq2Nav->CanNavigateToPoint(szLine);
	}

	return false;
}

// Check path length
NAV_API float GetNavPathLength(const char* szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		return g_mq2Nav->GetNavigationPathLength(szLine);
	}

	return -1;
}

// used to pass mq2nav commands
NAV_API bool ExecuteNavCommand(const char* szLine)
{
	if (g_mq2Nav && g_mq2Nav->IsInitialized())
	{
		g_mq2Nav->Command_Navigate(szLine);
		return true;
	}

	return false;
}

PLUGIN_API nav::NavAPI* GetPluginInterface()
{
	return GetNavAPI();
}

//============================================================================
