//
// NavigationPath.h
//

#pragma once

#include "common/Signal.h"
#include "common/Utilities.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/Renderable.h"
#include "plugin/RenderList.h"

#include <DetourNavMeshQuery.h>
#include <DetourPathCorridor.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <d3d9.h>
#include <d3dx9.h>

#include <imgui.h>
#include <memory>

#define DEBUG_NAVIGATION_LINES 1

//----------------------------------------------------------------------------

class NavMesh;
class NavigationLine;
struct DestinationInfo;

struct StraightPath
{
	StraightPath() = default;
	StraightPath(int size) { Reset(size); }

	std::unique_ptr<glm::vec3[]> verts;
	std::unique_ptr<uint8_t[]> flags;
	std::unique_ptr<dtPolyRef[]> polys;
	int length = 0;
	int cursor = 0;

	void Reset(int size);
	int GetAllocatedSize() { return alloc; }

	using Node = std::tuple<glm::vec3, uint8_t, dtPolyRef>;

	inline Node GetNode(int pos) const
	{
		if (pos >= length)
			return { {}, 0, 0 };

		return { verts[pos], flags[pos], polys[pos] };
	}

private:
	int alloc = 0;
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

	// trigger a recalculation of the path towards the destination. Returns true if the path
	// has been changed.
	void UpdatePath(bool force = false, bool incremental = false);

	// trigger render of the debug ui
	void RenderUI();

	void SetShowNavigationPaths(bool renderPaths);

	//----------------------------------------------------------------------------

	// get the destination point this path is navigating to.
	glm::vec3 GetDestination() const;

	// get the full length of the path as traversed
	float GetPathTraversalDistance() const;

	// Get the number of nodes in the path and the index of the current node
	// along that path.
	int GetPathSize() const { return m_currentPath->length; }
	int GetPathIndex() const { return m_currentPath->cursor; }

	// Check if we are at the end if our path
	inline bool IsAtEnd() const
	{
		return m_currentPath->cursor >= m_currentPath->length
			|| m_currentPath->length <= 0;
	}

	inline glm::vec3 GetNextPosition() const
	{
		return GetPosition(m_currentPath->cursor);
	}

	// get the coordinates of a point in a raw float3 form
	inline const float* GetRawPosition(int index) const
	{
		assert(index < m_currentPath->length);
		return glm::value_ptr(m_currentPath->verts[index]);
	}

	// get the coordinates in silly eq coordinates
	inline glm::vec3 GetPosition(int index) const
	{
		assert(index < m_currentPath->length);
		return m_currentPath->verts[index];
	}

	inline void Increment() { ++m_currentPath->cursor; }

	inline const float* GetCurrentPath() const {
		return GetRawPosition(0);
	}

	bool CanSeeDestination() const;

	dtNavMesh* GetNavMesh() const { return m_navMesh.get(); }
	dtNavMeshQuery* GetNavMeshQuery() const { return m_query.get(); }

	Signal<> PathUpdated;

private:
	void SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh,
		bool updatePath = true);

	std::unique_ptr<StraightPath> RecomputePath(
		const glm::vec3& startPos,
		const glm::vec3& endPos,
		bool force,
		bool incremental);

	std::shared_ptr<DestinationInfo> m_destinationInfo;

	std::unique_ptr<RenderGroup> m_debugDrawGrp;

	glm::vec3 m_destination;
	glm::vec3 m_lastPos;

	// the plugin owns the mesh
	std::shared_ptr<dtNavMesh> m_navMesh;

	// we own the query
	deleting_unique_ptr<dtNavMeshQuery> m_query;

	std::unique_ptr<StraightPath> m_currentPath;

	bool m_renderPaths;
	std::shared_ptr<NavigationLine> m_line;

	dtQueryFilter m_filter;
	glm::vec3 m_extents = { 2, 10, 2 }; // note: X, Z, Y

	Signal<>::ScopedConnection m_navMeshConn;
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
	void SetCurrentPos(const glm::vec3& currentPos);

	void Update();

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
	glm::vec3 m_startPos;

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
