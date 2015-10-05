//
// RenderHandler.h
//

#pragma once

#include "Renderable.h"
#include "Signal.h"

#include <d3dx9.h>
#include <imgui.h>

#include <chrono>
#include <memory>
#include <vector>

class CEQDraw;
class MQ2NavigationPath;

class RenderHooks;
//class RenderList;

class dtNavMesh;

extern IDirect3DDevice9* g_pDevice;


class RenderHandler
{
	const float HEIGHT_OFFSET = 2.0;

	friend class RenderHooks;

public:
	RenderHandler();
	~RenderHandler();

	void AddRenderable(const std::shared_ptr<Renderable>& renderable);
	void RemoveRenderable(const std::shared_ptr<Renderable>& renderable);

	// testing
	void SetNavigationPath(MQ2NavigationPath* navPath);
	void UpdateNavigationPath();
	void ClearNavigationPath();

private:

	// Called by RenderHooks
	void CreateDeviceObjects();
	void InvalidateDeviceObjects();

	void PerformRender(Renderable::RenderPhase phase);

	void Render3D();

private:
	bool m_deviceAcquired = false; // implies that g_pDevice is valid to use

	std::vector<std::shared_ptr<Renderable>> m_renderables;

	// demo purposes
	IDirect3DVertexBuffer9* m_pVertexBuffer = nullptr;
	IDirect3DTexture9* m_pTexture = nullptr;

	D3DXMATRIX m_worldMatrix;
	D3DXMATRIX m_viewMatrix;
	D3DXMATRIX m_projMatrix;

	//std::unique_ptr<RenderList> m_renderList;

	Signal<dtNavMesh*>::ScopedConnection m_navMeshConnection;

	//----------------------------------------------------------------------------
	// nav path rendering

	D3DMATERIAL9 m_navPathMaterial;

	struct LineVertex
	{
		float x, y, z;

		enum FVF
		{
			FVF_Flags = D3DFVF_XYZ
		};
	};

	int m_navpathLen = 0;
	MQ2NavigationPath* m_navPath = nullptr;

	IDirect3DVertexBuffer9* m_pLines = nullptr;
};

extern std::shared_ptr<RenderHandler> g_renderHandler;
