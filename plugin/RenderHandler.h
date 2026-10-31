//
// RenderHandler.h
//

#pragma once

#include "plugin/Renderable.h"
#include "plugin/RenderDX9.h"
#include "plugin/RenderDX11.h"

#include <mq/Plugin.h>
#include <imgui.h>
#include <memory>
#include <list>

class RenderHooks;

// The renderer for the graphics API the client was built with. Both backends
// expose the same functions, so shared code can call either one.
#if HAS_DIRECTX_11
using RenderBackend = RenderDX11;
#else
using RenderBackend = RenderDX9;
#endif

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

	RenderBackend* GetBackend() const { return m_deviceAcquired ? m_backend.get() : nullptr; }

private:
	bool m_deviceAcquired = false; // implies that g_pDevice is valid to use

	std::list<Renderable*> m_renderables;

	std::unique_ptr<RenderBackend> m_backend;
};

// Returns the active renderer, or nullptr if the device is not acquired.
RenderBackend* GetRenderBackend();

