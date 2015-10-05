//
// RenderList.h
//
// Implements a render list for rendering geometry

#pragma once

#include "Renderable.h"

#include <d3dx9.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <memory>
#include <vector>

//----------------------------------------------------------------------------

class RenderList : public Renderable
{
public:
	RenderList(IDirect3DDevice9* d3dDevice);
	~RenderList();

	enum PrimitiveType
	{
		Prim_Points = 0,
		Prim_Lines,
		Prim_Triangles,
		Prim_Quads,

		Prim_Count
	};

	void Reset();

	//----------------------------------------------------------------------------
	// build the geometry

	void Begin(PrimitiveType type, float size = 1.0f);

	void AddVertex(float x, float y, float z, unsigned int color, float u, float v);

	void End();

	//----------------------------------------------------------------------------

	// Render the geometry
	virtual void Render();

	// Release any resources we might be holding onto
	virtual void InvalidateDeviceObjects();

	// Create the resources we need to render
	virtual bool CreateDeviceObjects();

private:
	void ResetCounter();

	struct Vertex
	{
		D3DXVECTOR3 pos;
		D3DCOLOR    col;
		D3DXVECTOR2 uv;
	};
	static const DWORD VertexType = (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	struct PrimitiveList
	{
		std::vector<uint16_t> indices;
		int vertices = 0;
		float size = 1.0; // not used for now
		PrimitiveType type;

		// used for rendering from the index buffer
		uint16_t startingIndex = 0;
	};

private:
	IDirect3DDevice9* m_pDevice = nullptr;
	IDirect3DVertexBuffer9* m_pVB = nullptr;
	IDirect3DIndexBuffer9* m_pIB = nullptr;

	std::vector<Vertex> m_vertices;
	std::vector<std::unique_ptr<PrimitiveList>> m_prims;

	PrimitiveList* m_currentPrim;
	int m_currVertexCnt; // used for assembling lines, tris, quads
};
