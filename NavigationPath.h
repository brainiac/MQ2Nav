//
// NavigationPath.h
//

#pragma once

#include "Renderable.h"
#include "RenderList.h"

#include "DetourNavMeshQuery.h"
#include "DetourPathCorridor.h"

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <d3d9.h>
#include <d3dx9.h>

#include <memory>

//----------------------------------------------------------------------------

class NavigationLine;

class NavigationPath
{
	friend class NavigationLine;

public:
	NavigationPath(dtNavMesh* navMesh, bool renderPaths = false);
	~NavigationPath();

	//----------------------------------------------------------------------------
	// constants

	static const int MAX_POLYS = 4028 * 4;

	static const int MAX_NODES = 2048 * 4;

	static const int MAX_PATH_SIZE = 2048 * 4;

	//----------------------------------------------------------------------------

	const glm::vec3& GetDestination() const { return m_destination; }

	int GetPathSize() const { return m_currentPathSize; }
	int GetPathIndex() const { return m_currentPathCursor; }

	bool FindPath(const glm::vec3& pos);

	void UpdatePath();

	// Check if we are at the end if our path
	inline bool IsAtEnd() const { return m_currentPathCursor >= m_currentPathSize; }

	inline glm::vec3 GetNextPosition() const
	{
		return GetPosition(m_currentPathCursor);
	}
	inline const float* GetRawPosition(int index) const
	{
		assert(index < m_currentPathSize);
		return &m_currentPath[index * 3];
	}
	inline glm::vec3 GetPosition(int index) const
	{
		const float* rawcoord = GetRawPosition(index);
		return glm::vec3(rawcoord[0], rawcoord[1], rawcoord[2]);
	}

	inline void Increment() { ++m_currentPathCursor; }

	const float* GetCurrentPath() const { return &m_currentPath[0]; }

	dtNavMesh* GetNavMesh() const { return m_navMesh; }
	dtNavMeshQuery* GetNavMeshQuery() const { return m_query.get(); }

private:
	void FindPathInternal(const glm::vec3& pos);

	glm::vec3 m_destination;

	float m_currentPath[MAX_POLYS * 3];
	unsigned char m_cornerFlags[MAX_POLYS];

	int m_currentPathCursor = 0;
	int m_currentPathSize = 0;

	// the plugin owns the mesh
	dtNavMesh* m_navMesh;

	// we own the query
	std::unique_ptr<dtNavMeshQuery> m_query;
	std::unique_ptr<dtPathCorridor> m_corridor;

	bool m_renderPaths;
	std::shared_ptr<NavigationLine> m_line;

	dtQueryFilter m_filter;
	float m_extents[3] = { 50, 400, 50 }; // note: X, Z, Y
};

class NavigationLine : public Renderable
{
public:
	NavigationLine(NavigationPath* path);
	virtual ~NavigationLine();

	void SetThickness(float thickness);

	void Update() { m_needsUpdate = true; }

	virtual void Render(RenderPhase phase) override;
	virtual bool CreateDeviceObjects() override;
	virtual void InvalidateDeviceObjects() override;

	void GenerateBuffers();

private:
	NavigationPath* m_path;

	// The vertex structure we'll be using for line drawing. Each line is defined as two vertices,
	// and the vertex shader will create a quad from these two vertices. However, since the vertex
	// shader can only process one vertex at a time, we need to store information in each vertex
	// about the other vertex (the other end of the line).
	struct TVertex
	{
		D3DXVECTOR3 pos;
		D3DXVECTOR3 otherPos;
		D3DXVECTOR4 texOffset;
		D3DXVECTOR3 thickness;

		static const DWORD FVF = D3DFVF_XYZ | D3DFVF_NORMAL |
			D3DFVF_TEX2 | // D3DFVF_TEX2 specifies we have two sets of texture coordinates.
			D3DFVF_TEXCOORDSIZE4(0) | // This specifies that the first (0th) tex coord set has size 4 floats.
			D3DFVF_TEXCOORDSIZE3(1); // Specifies that second tex coord set has size 2 floats.
	};

	IDirect3DVertexBuffer9* m_vertexBuffer = nullptr;
	D3DMATERIAL9 m_material;

	struct RenderCommand
	{
		UINT StartVertex;
		UINT PrimitiveCount;
	};
	std::vector<RenderCommand> m_commands;
	int m_lastSize = 0;

	bool m_loaded = false;
	bool m_needsUpdate = false;
	float m_thickness = 1.0f;
};
