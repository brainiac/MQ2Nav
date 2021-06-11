//
// PluginHooks.h
//

#pragma once

#include "common/Signal.h"
#include "plugin/MQ2Navigation.h"

#include <mq/Plugin.h>
#include <directxsdk/d3dx9.h>

//----------------------------------------------------------------------------

#if !defined(MQNEXT)

// indicates whether imgui is ready to be used
extern bool g_imguiReady;

// indicate whether imgui should render a new frame
extern bool g_imguiRender;

// current mouse location
extern POINT MouseLocation;

// whether mouse is blocked from overlay capture or not
extern bool MouseBlocked;

extern IDirect3DDevice9* gpD3D9Device;

extern bool gbDeviceAcquired;

// Mouse state, pointed to by EQADDR_DIMOUSECOPY
struct MouseStateData {
	DWORD x;
	DWORD y;
	DWORD Scroll;
	DWORD relX;
	DWORD relY;
	DWORD InWindow;
};
extern MouseStateData* MouseState;

#endif // !defined(MQNEXT)

//----------------------------------------------------------------------------

enum class HookStatus {
	Success,
	MissingOffset,
	MissingDevice,
	AlreadyInstalled,
};

HookStatus InitializeHooks();

void ShutdownHooks();

//----------------------------------------------------------------------------
