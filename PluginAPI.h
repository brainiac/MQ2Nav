//
// PluginAPI.h
//

#pragma once

// Exports functions that can be used by other plugins to interface with MQ2Nav

#ifdef NAV_API_EXPORTS
#define NAV_API extern "C" __declspec(dllexport)
#define NAV_OBJECT __declspec(dllexport)
#else
#define NAV_API extern "C" __declspec(dllimport)
#define NAV_OBJECT __declspec(dllimport)
#endif

//----------------------------------------------------------------------------

// In the future, an API will be provided here that can be used by other plugins
// to implicitly link against MQ2Nav and import its functionality.

// That time is not now. Until then, plugins should GetModuleHandle/GetProcAddress
// the functions listed below.

// bool IsNavInitialized();
// bool IsNavMeshLoaded();
// bool IsNavPathActive();
// bool IsNavPathPaused();
// bool IsNavPossible(const char* szLine)
// float GetNavPathLength(const char* szLine)
// bool ExecuteNavCommand(const char* szLine)
//
// Using the old functions will print a warning the first time they are used.
