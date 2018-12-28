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

	// Does some drawing commands
	void DrawUI();

	virtual void InvalidateDeviceObjects() override;
	virtual bool CreateDeviceObjects() override;
	virtual void Render(RenderPhase phase) override;

	void BeginNewFrame();

	void SetVisible(bool visible);

	// add a signal to do ui stuff
	Signal<> OnUpdateUI;

private:
	// indicates whether imgui is ready to be used
	bool m_imguiReady = false;

	// indicate whether imgui should render a new frame
	bool m_imguiRender = false;

	bool m_visible = true;

	// we're holding onto the device, we need to maintain a refcount
	IDirect3DDevice9* m_pDevice = nullptr;

	// fps counter
	std::vector<float> m_renderFrameRateHistory;
	std::chrono::system_clock::time_point m_prevHistoryPoint;

	std::string m_iniFileName;
};
