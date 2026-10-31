//
// RenderHandler.cpp
//

#include "pch.h"
#include "RenderHandler.h"

#include "plugin/MQ2Navigation.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DebugDraw.h>

#include <glm/glm.hpp>

//============================================================================

RenderHandler::RenderHandler()
{
}

RenderHandler::~RenderHandler()
{
}

void RenderHandler::Shutdown()
{
	InvalidateDeviceObjects();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void RenderHandler::CreateDeviceObjects()
{
	if (m_deviceAcquired)
		return;
	if (!gpD3D9Device)
		return;

#if HAS_DIRECTX_11
	if (!gpD3D11Device)
		return;
	auto device = gpD3D11Device;
#else
	auto device = gpD3D9Device;
#endif

	m_backend = std::make_unique<RenderBackend>();
	if (!m_backend->Initialize(device))
	{
		SPDLOG_ERROR("Failed to initialize rendering");
		m_backend.reset();
		return;
	}

	for (auto& p : m_renderables)
	{
		p->CreateDeviceObjects();
	}
	m_deviceAcquired = true;
}

void RenderHandler::InvalidateDeviceObjects()
{
	if (!m_deviceAcquired)
		return;
	m_deviceAcquired = false;

	for (auto& p : m_renderables)
	{
		p->InvalidateDeviceObjects();
	}

	m_backend.reset();
}

void RenderHandler::AddRenderable(Renderable* renderable)
{
	assert(std::find(m_renderables.begin(), m_renderables.end(), renderable) == m_renderables.cend());

	m_renderables.push_back(renderable);

	if (m_deviceAcquired)
	{
		renderable->CreateDeviceObjects();
	}
}

void RenderHandler::RemoveRenderable(Renderable* renderable)
{
	auto iter = std::find(m_renderables.begin(), m_renderables.end(), renderable);
	assert(iter != m_renderables.end());

	if (m_deviceAcquired)
	{
		renderable->InvalidateDeviceObjects();
	}

	m_renderables.erase(iter);
}

void RenderHandler::PerformRender()
{
	if (!m_deviceAcquired)
		return;

	m_backend->BeginFrame();

	for (auto& r : m_renderables)
	{
		r->Render();
	}

	m_backend->EndFrame();
}

RenderBackend* GetRenderBackend()
{
	return g_renderHandler ? g_renderHandler->GetBackend() : nullptr;
}
