//
// PluginMain.cpp
//

#include "plugin/MQ2Navigation.h"
#include "../PluginAPI.h"

PreSetup("MQ2Nav");
PLUGIN_VERSION(1.30);

#define PLUGIN_NAME "MQ2Nav"
#define MQ2NAV_PLUGIN_VERSION "1.3.0"

#include <chrono>
#include <memory>

//----------------------------------------------------------------------------

std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

//----------------------------------------------------------------------------

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2Nav");

	WriteChatf(PLUGIN_MSG "v%s by brainiac (\aohttps://github.com/brainiac/MQ2Nav\ax)", MQ2NAV_PLUGIN_VERSION);

	g_mq2Nav = std::make_unique<MQ2NavigationPlugin>();
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
NAV_API bool IsNavInitialized()
{
	return g_mq2Nav && g_mq2Nav->IsInitialized();
}

// Used to check if mesh is loaded
NAV_API bool IsNavMeshLoaded()
{
	return g_mq2Nav && g_mq2Nav->IsMeshLoaded();
}

// Used to check if a path is active
NAV_API bool IsNavPathActive()
{
	return g_mq2Nav && g_mq2Nav->IsActive();
}

 // Used to check if path is paused
NAV_API bool IsNavPathPaused()
{
	return g_mq2Nav && g_mq2Nav->IsPaused();
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

//============================================================================
// These are old, unofficial exports. These are deprecated.
// They will be removed in a future version.

inline void DeprecatedCheck(const char* exportName)
{
	static bool once = false;
	if (!once)
	{
		WriteChatf(PLUGIN_MSG "Deprecated export '%s' was used. Please make sure "
			"to use the documented exports, as deprecated names will be removed in "
			"a future update.", exportName);
		once = true;
	}
}

PLUGIN_API bool NavInitialized()
{
	DeprecatedCheck("NavInitialized");
	return IsNavInitialized();
}


// Used to check if mesh is loaded
PLUGIN_API bool NavMeshLoaded()
{
	DeprecatedCheck("NavMeshLoaded");
	return IsNavMeshLoaded();
}

// Used to check if a path is active
PLUGIN_API bool NavPathActive()
{
	DeprecatedCheck("NavPathActive");
	return IsNavPathActive();
}

// Used to check if path is paused
PLUGIN_API bool NavPathPaused()
{
	DeprecatedCheck("NavPathPaused");
	return IsNavPathPaused();
}

// Check if path is possible to the specified target
PLUGIN_API bool NavPossible(PCHAR szLine)
{
	DeprecatedCheck("NavPossible");
	return IsNavPossible(szLine);
}

// Check path length
PLUGIN_API float NavPathLength(PCHAR szLine)
{
	DeprecatedCheck("NavPathLength");
	return GetNavPathLength(szLine);
}

// used to pass mq2nav commands
PLUGIN_API void NavCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	DeprecatedCheck("NavCommand");
	ExecuteNavCommand(szLine);
}

//============================================================================
