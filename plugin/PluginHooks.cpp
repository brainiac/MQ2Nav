//
// PluginHooks.cpp
//

#include "pch.h"
#include "PluginHooks.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/ImGuiRenderer.h"
#include "plugin/RenderHandler.h"
#include "plugin/imgui/imgui_impl_win32.h"
#include "common/FindPattern.h"

#ifdef DIRECTINPUT_VERSION
#undef DIRECTINPUT_VERSION
#endif
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <Shlobj.h>
#include <Psapi.h>
#include <imgui.h>

#include <atomic>
#include <thread>
#include <spdlog/spdlog.h>
#include <winternl.h>
#include <tchar.h>

#include <cfenv>

//============================================================================

// have detours been installed already?
bool g_hooksInstalled = false;

static void MQCreateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->CreateDeviceObjects();
	}
}

static void MQInvalidateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->InvalidateDeviceObjects();
	}
}

static void MQImGuiRender()
{
	if (g_imguiRenderer)
	{
		g_imguiRenderer->ImGuiRender();
	}
}

static void MQGraphicsSceneRender()
{
	if (g_renderHandler)
	{
		g_renderHandler->PerformRender();
	}
}

static int sMQCallbacksId = -1;

static void AddNavRenderCallbacks()
{
	if (sMQCallbacksId != -1)
		return;

	MQRenderCallbacks callbacks;
	callbacks.CreateDeviceObjects = MQCreateDeviceObjects;
	callbacks.InvalidateDeviceObjects = MQInvalidateDeviceObjects;
	callbacks.GraphicsSceneRender = MQGraphicsSceneRender;

	sMQCallbacksId = AddRenderCallbacks(callbacks);
}

HookStatus InitializeHooks()
{
	if (gpD3D9Device)
	{
		g_hooksInstalled = true;
		AddNavRenderCallbacks();
	}

	if (g_hooksInstalled)
	{
		if (!gpD3D9Device)
		{
			return HookStatus::MissingDevice;
		}

		return HookStatus::Success;
	}

#if EQSWITCH_USESWTICH_LOGGING
	EzDetour(EQSwitch__UseSwitch,
		&EQSwitch_Detour::UseSwitch_Detour,
		&EQSwitch_Detour::UseSwitch_Trampoline);
#endif

	return !gpD3D9Device ? HookStatus::MissingDevice : HookStatus::Success;
}

void ShutdownHooks()
{
	if (!g_hooksInstalled)
		return;

	RemoveRenderCallbacks(sMQCallbacksId);
}

PLUGIN_API void OnUpdateImGui()
{
	MQImGuiRender();
}


//----------------------------------------------------------------------------
