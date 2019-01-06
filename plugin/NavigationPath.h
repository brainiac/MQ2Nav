//
// NavigationPath.h
//

#pragma once

#include "common/NavigationRoute.h"
#include "common/Signal.h"
#include "common/Utilities.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/Renderable.h"
#include "plugin/RenderList.h"

#include <DetourNavMeshQuery.h>
#include <DetourPathCorridor.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <d3d9.h>
#include <d3dx9.h>

#include <imgui.h>
#include <memory>

#define DEBUG_NAVIGATION_LINES 1

//----------------------------------------------------------------------------

class NavMesh;
class NavigationLine;
struct DestinationInfo;

enum struct NavigationState
{
	Inactive = 0,
	Active,
	OffMeshLink,
};

class NavigationPath
{
	friend class NavigationLine;

public:
	NavigationPath(const std::shared_ptr<DestinationInfo>& dest);
	~NavigationPath();

	void SetDestination(const std::shared_ptr<DestinationInfo>& info);
	std::shared_ptr<DestinationInfo> GetDestinationInfo() const { return m_destinationInfo; }

	// try to find a path to the current destination. Returns true if a path has been found.
	bool FindPath();

	// trigger a recalculation of the path towards the destination.
	void UpdatePath();

	// trigger render of the debug ui
	void RenderUI();

	void SetShowNavigationPaths(bool renderPaths);

	//----------------------------------------------------------------------------

	// get the destination point this path is navigating to.
	glm::vec3 GetDestination() const;

	std::shared_ptr<NavigationRoute> GetRoute() const { return m_route; }

	// get the full length of the path as traversed
	float GetPathTraversalDistance() const;

	NavigationState GetState() const { return m_state; }

	bool IsAtEnd() const { return false; }

	size_t GetPathSize() const { return m_route ? m_route->size() : 0; }
	
	Signal<> PathUpdated;

private:
	void SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh,
		bool updatePath = true);

	void ResetState();
	void UpdatePositions();
	bool FindRoute();

	// the plugin owns the mesh
	std::shared_ptr<dtNavMesh> m_navMesh;
	Signal<>::ScopedConnection m_navMeshConn;

	std::shared_ptr<DestinationInfo> m_destinationInfo;
	std::unique_ptr<RenderGroup> m_debugDrawGrp;
	std::shared_ptr<NavigationLine> m_line;
	bool m_renderPaths = true;

	std::shared_ptr<NavigationRoute> m_route;
	std::shared_ptr<Navigator> m_navigator;

	glm::vec3 m_pos, m_target;

	bool m_allowPartial = false;
	bool m_offMesh = false;
	glm::vec3 m_offMeshStart, m_offMeshEnd;
	bool m_needReplan = true;
	bool m_failed = false;
	bool m_havePath = false;
	dtPolyRef m_offMeshPoly = 0;
	NavigationState m_state = NavigationState::Inactive;
};

//----------------------------------------------------------------------------

class NavigationLine : public Renderable
{
public:
	NavigationLine(NavigationPath* path);
	virtual ~NavigationLine();

	void SetThickness(float thickness);
	float GetThickness() const { return m_thickness; }
	void SetVisible(bool visible);

	void Update() { m_needsUpdate = true; }

	virtual void Render(RenderPhase phase) override;
	virtual bool CreateDeviceObjects() override;
	virtual void InvalidateDeviceObjects() override;

	void GenerateBuffers();

#if DEBUG_NAVIGATION_LINES
	void RenderUI();
#endif

private:
	NavigationPath* m_path;

	ID3DXEffect* m_effect = nullptr;

	// The vertex structure we'll be using for line drawing. Each line is defined as two vertices,
	// and the vertex shader will create a quad from these two vertices. However, since the vertex
	// shader can only process one vertex at a time, we need to store information in each vertex
	// about the other vertex (the other end of the line).
	struct TVertex
	{
		D3DXVECTOR3 pos;
		D3DXVECTOR3 otherPos;
		D3DXVECTOR3 adjPos; // position of previous or next line segment, depending on the vertex
		FLOAT thickness;
		FLOAT adjHint;

		static const DWORD FVF = D3DFVF_XYZ | D3DFVF_NORMAL |
			D3DFVF_TEX2 | // D3DFVF_TEX2 specifies we have two sets of texture coordinates.
			D3DFVF_TEXCOORDSIZE3(0) | // This specifies that the first (0th) tex coord set has size 3 floats.
			D3DFVF_TEXCOORDSIZE1(1) | // Specifies that second tex coord set has size 2 floats.
			D3DFVF_TEXCOORDSIZE1(2); // hint towards where the adjacent coord is
	};

	IDirect3DVertexBuffer9* m_vertexBuffer = nullptr;
	IDirect3DVertexDeclaration9* m_vDeclaration = nullptr;

	struct RenderCommand
	{
		UINT StartVertex;
		UINT PrimitiveCount;
	};
	std::vector<RenderCommand> m_commands;
	int m_lastSize = 0;

	bool m_loaded = false;
	bool m_needsUpdate = false;
	float m_thickness = 0.25f;
	bool m_visible = true;

	struct RenderStyle {
		ImColor render_color = { 255, 0, 0 };
		float width = 1.0;
		bool enabled = true;

		RenderStyle(ImColor rc, float w)
			: render_color(rc)
			, width(w) {}
		RenderStyle() {}
	};

	std::vector<RenderStyle> m_renderPasses;

	Signal<>::ScopedConnection m_pathUpdated;
};
