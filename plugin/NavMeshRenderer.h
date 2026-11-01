//
// NavMeshRenderer.h
//

#pragma once

#include "common/NavModule.h"
#include "plugin/Renderable.h"
#include "plugin/RenderList.h"

#include <mq/base/Signal.h>

#include <cassert>
#include <thread>
#include <mutex>

class dtNavMesh;
class NavMesh;

//----------------------------------------------------------------------------

class NavMeshRenderer : public Renderable, public NavModule
{
	friend class NavMeshDebugDraw;

public:
	NavMeshRenderer();
	~NavMeshRenderer();

	virtual void Initialize() override;
	virtual void Shutdown() override;

	virtual void InvalidateDeviceObjects() override;
	virtual bool CreateDeviceObjects() override;
	virtual void Render() override;

	void UpdateNavMesh();

	void OnUpdateUI();

private:
	void CleanupObjects();

	// stop any pending geometry load
	void StopLoad();
	void StartLoad();

	// for NavMeshDebugDraw
	unsigned int GetColorForPolyArea(uint8_t areaType);

private:
	NavMesh* m_navMesh = nullptr;

	bool m_enabled = false;
	bool m_loaded = false;
	bool m_initialized = false;

	std::unique_ptr<RenderGroup> m_primGroup;
	mq::Signal<>::ScopedConnection m_meshConn;

	// loading progress
	bool m_loading = false;
	bool m_stopLoading = false;
	float m_progress = 0.0f;
	std::thread m_loadThread;
};
