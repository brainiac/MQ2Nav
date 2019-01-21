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
	: m_pDevice(device)
{
	m_pDevice->AddRef();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// TODO: Set optional io.ConfigFlags values, e.g. 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard' to enable keyboard controls.
	// TODO: Fill optional fields of the io structure later.
	// TODO: Load TTF/OTF fonts if you don't want to use the default font.
	fmt::format_to(ImGuiSettingsFile, "{}\\MQ2NavUI.ini", gszINIPath);
	io.IniFilename = &ImGuiSettingsFile[0];

	// Initialize helper Platform and Renderer bindings
	ImGui_ImplWin32_Init(eqhwnd);
	ImGui_ImplDX9_Init(device);

	ImGui::SetupImGuiStyle(true, 0.7f);
	ImGuiEx::ConfigureFonts();

	g_renderHandler->AddRenderable(this);
}

ImGuiRenderer::~ImGuiRenderer()
{
	g_renderHandler->RemoveRenderable(this);

	// Cleanup the ImGui overlay
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	m_pDevice->Release();
	m_pDevice = nullptr;
}

void ImGuiRenderer::InvalidateDeviceObjects()
{
	m_imguiReady = false;
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

bool ImGuiRenderer::CreateDeviceObjects()
{
	return m_imguiReady = ImGui_ImplDX9_CreateDeviceObjects();
}

void ImGuiRenderer::ImGuiRender()
{
	if (!m_visible)
		return;

	if (!m_imguiReady)
		return;
	//if (gGameState != GAMESTATE_INGAME)
	//	return;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// call the OnUpdateUI signal.
	OnUpdateUI();

	// Render the ui
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiRenderer::SetVisible(bool visible)
{
	m_visible = visible;
}

//----------------------------------------------------------------------------
