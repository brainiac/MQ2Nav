//
// PluginMain.cpp
//

#include "MQ2Navigation.h"

PreSetup("MQ2Nav");
PLUGIN_VERSION(2.00);
#define PLUGIN_NAME "MQ2Nav"

#include <chrono>
#include <memory>

//----------------------------------------------------------------------------

std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

namespace internal
{
	// Chrono's static initialization somehow conflicts with innerspace. No idea why,
	// but patch the broken function to use our version instead of the standard library's.
	// The implementation is the same, but it doesn't rely on static initialization.

	static std::chrono::high_resolution_clock::time_point high_resolution_clock_now() noexcept
	{
		int64_t freq = _Query_perf_frequency();
		int64_t ctr = _Query_perf_counter();

		int64_t whole = (ctr / freq) * 1000000000;
		int64_t part = (ctr % freq) * 1000000000 / freq;

		return std::chrono::high_resolution_clock::time_point(
			std::chrono::high_resolution_clock::duration(whole + part)
		);
	}

} // namespace internal

//----------------------------------------------------------------------------

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2Nav");

	WriteChatf(PLUGIN_MSG "v%1.1f \ay(BETA)\ax by brainiac (\aohttps://github.com/brainiac/MQ2Nav\ax)", MQ2Version);

	// Patch our chrono::system_clock::now() function
	DetourFunction(reinterpret_cast<PBYTE>(&std::chrono::high_resolution_clock::now),
		reinterpret_cast<PBYTE>(&internal::high_resolution_clock_now));

	g_mq2Nav.reset(new MQ2NavigationPlugin);
	g_mq2Nav->Plugin_Initialize();

	if (!g_mq2Nav->IsInitialized())
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
