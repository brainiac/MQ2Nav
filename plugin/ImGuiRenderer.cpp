//
// ImGuiRenderer.cpp
//

#include "pch.h"
#include "ImGuiRenderer.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginHooks.h"
#include "plugin/RenderHandler.h"
#include "plugin/imgui/imgui_impl_dx9.h"
#include "plugin/imgui/imgui_impl_win32.h"
#include "common/Utilities.h"

#include <fmt/format.h>
#include <imgui.h>
#include <imgui/custom/imgui_user.h>
#include <imgui/custom/imgui_utils.h>

//----------------------------------------------------------------------------

// needs to be global, its referenced by imgui
char ImGuiSettingsFile[MAX_PATH] = { 0 };

ImGuiRenderer::ImGuiRenderer(HWND eqhwnd, IDirect3DDevice9* device)
#if !defined(MQNEXT)
	: m_pDevice(device)
#endif
{
#if !defined(MQNEXT)
	m_pDevice->AddRef();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

	fmt::format_to(ImGuiSettingsFile, "{}\\MQ2NavUI.ini", gPathConfig);
	io.IniFilename = &ImGuiSettingsFile[0];

	// Initialize helper Platform and Renderer bindings
	ImGui_ImplWin32_Init(eqhwnd);
	ImGui_ImplDX9_Init(device);

	g_renderHandler->AddRenderable(this);
#endif
}

ImGuiRenderer::~ImGuiRenderer()
{
#if !defined(MQNEXT)
	g_renderHandler->RemoveRenderable(this);

	// Cleanup the ImGui overlay
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	m_pDevice->Release();
	m_pDevice = nullptr;
#endif
}

void ImGuiRenderer::InvalidateDeviceObjects()
{
#if !defined(MQNEXT)
	m_imguiReady = false;
	ImGui_ImplDX9_InvalidateDeviceObjects();
#endif
}

bool ImGuiRenderer::CreateDeviceObjects()
{
	if (!m_imguiConfigured)
	{
		ImGui::SetupImGuiStyle(true, 0.7f);
		ImGuiEx::ConfigureFonts();

		m_imguiConfigured = true;
	}

#if !defined(MQNEXT)
	return m_imguiReady = ImGui_ImplDX9_CreateDeviceObjects();
#else
	return m_imguiReady = true;
#endif
}

void ImGuiRenderer::ImGuiRender()
{
	if (!m_visible)
		return;

	if (!m_imguiReady)
		return;
	if (gGameState != GAMESTATE_INGAME)
		return;

#if !defined(MQNEXT)
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

#endif
	// call the OnUpdateUI signal.
	OnUpdateUI();

#if !defined(MQNEXT)
	// Render the ui
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiRenderer::SetVisible(bool visible)
{
	m_visible = visible;
}

//----------------------------------------------------------------------------
