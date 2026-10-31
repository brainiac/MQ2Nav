//
// RenderHandler.h
//

#pragma once

#include "plugin/Renderable.h"
#include "plugin/RenderDX11.h"

#include <mq/Plugin.h>
#include <imgui.h>
#include <memory>
#include <list>

class RenderHooks;

class RenderHandler
{
	friend class RenderHooks;

public:
	RenderHandler();
	~RenderHandler();

	void Shutdown();

	void AddRenderable(Renderable* renderable);
	void RemoveRenderable(Renderable* renderable);

	// Called by RenderHooks
	void CreateDeviceObjects();
	void InvalidateDeviceObjects();

	void PerformRender();

#if HAS_DIRECTX_11
	RenderDX11* GetDX11() const { return m_deviceAcquired ? m_dx11.get() : nullptr; }
#endif

private:
	bool m_deviceAcquired = false; // implies that g_pDevice is valid to use

	std::list<Renderable*> m_renderables;

#if HAS_DIRECTX_11
	std::unique_ptr<RenderDX11> m_dx11;
#endif
};

// utility function to reset the state of the current direct3d9 device
void ResetDeviceState();

