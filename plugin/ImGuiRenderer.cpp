//
// ImGuiRenderer.cpp
//

#include "pch.h"
#include "ImGuiRenderer.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/RenderHandler.h"
#include "common/Utilities.h"

#pragma comment(lib, "imgui.lib")

//----------------------------------------------------------------------------

ImGuiRenderer::ImGuiRenderer(HWND eqhwnd, IDirect3DDevice9* device)
{
}

ImGuiRenderer::~ImGuiRenderer()
{
}

void ImGuiRenderer::InvalidateDeviceObjects()
{
}

bool ImGuiRenderer::CreateDeviceObjects()
{
	return true;
}

void ImGuiRenderer::ImGuiRender()
{
	if (!m_visible)
		return;

	if (gGameState != GAMESTATE_INGAME)
		return;

	// call the OnUpdateUI signal.
	OnUpdateUI();
}

void ImGuiRenderer::SetVisible(bool visible)
{
	m_visible = visible;
}

//----------------------------------------------------------------------------
