//
// ImGuiRenderer.h
//

#pragma once

#include "common/Signal.h"
#include "plugin/Renderable.h"

#include <chrono>
#include <memory>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3dx9.h>

//----------------------------------------------------------------------------

class ImGuiRenderer : public Renderable
{
public:
	ImGuiRenderer(HWND eqhwnd, IDirect3DDevice9* device);
	~ImGuiRenderer();

	void ImGuiRender();

	void SetVisible(bool visible);
	bool IsReady() const { return m_imguiReady; }

	// add a signal to do ui stuff
	Signal<> OnUpdateUI;

private:
	virtual void InvalidateDeviceObjects() override;
	virtual bool CreateDeviceObjects() override;

private:
	// indicates whether imgui is ready to be used
	bool m_imguiReady = false;
	bool m_visible = true;

	// we're holding onto the device, we need to maintain a refcount
	IDirect3DDevice9* m_pDevice = nullptr;
};
