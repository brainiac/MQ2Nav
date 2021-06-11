//
// ImGuiRenderer.h
//

#pragma once

#include "common/Signal.h"
#include "plugin/Renderable.h"

#include <mq/Plugin.h>

#include <chrono>
#include <memory>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <directxsdk/d3dx9.h>

//----------------------------------------------------------------------------

class ImGuiRenderer : public Renderable
{
public:
	ImGuiRenderer(HWND eqhwnd, IDirect3DDevice9* device);
	~ImGuiRenderer();

	void ImGuiRender();

	void SetVisible(bool visible);
	bool IsReady() const { return true; }

	// add a signal to do ui stuff
	Signal<> OnUpdateUI;

private:
	virtual void InvalidateDeviceObjects() override;
	virtual bool CreateDeviceObjects() override;

private:
	bool m_visible = true;
};
