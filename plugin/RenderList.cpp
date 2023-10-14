//
// RenderList.cpp
//

#include "pch.h"
#include "RenderList.h"

#include <imgui.h>

#include <algorithm>
#include <Windows.h>

static inline int PrimVertexCount(RenderList::PrimitiveType type)
{
	switch (type)
	{
	case RenderList::Prim_Points:
		return 1;
	case RenderList::Prim_Lines:
		return 2;
	case RenderList::Prim_Triangles:
		return 3;
	case RenderList::Prim_Quads:
		return 4;
	default: break;
	}

	return 0;
}

RenderList::RenderList(eqlib::Direct3DDevice9* pDevice, PrimitiveType type)
	: m_pDevice(pDevice)
	, m_type(type)
	, m_tempMax(PrimVertexCount(type))
	, m_currentPrim(nullptr)
{
	assert(m_pDevice != nullptr);
}

RenderList::~RenderList()
{
	InvalidateDeviceObjects();
}

static inline unsigned int ConvertColor(unsigned int color)
{
	return D3DCOLOR_RGBA(
		((color >> 0) & 0xff),
		((color >> 8) & 0xff),
		((color >> 16) & 0xff),
		((color >> 24) & 0xff));
}

void RenderList::Reset()
{
	InvalidateDeviceObjects();

	m_vertices.clear();
	m_prims.clear();
}

void RenderList::Begin(float size /* = 1.0f */)
{
	// check if we have a primitive list that matches this size
	auto iter = std::find_if(m_prims.begin(), m_prims.end(),
		[size](std::unique_ptr<PrimitiveList>& l) { return l->size == size; });

	if (iter == m_prims.end())
	{
		std::unique_ptr<PrimitiveList> p = std::make_unique<PrimitiveList>();
		p->size = size;

		m_currentPrim = p.get();
		m_prims.emplace_back(std::move(p));
	}
	else
	{
		m_currentPrim = iter->get();
	}
}

void RenderList::AddVertex(float x, float y, float z, unsigned int color, float u, float v)
{
	if (m_eqCoords)
		m_vertices.emplace_back(Vertex{ { x, y, z}, ConvertColor(color), {u, v} });
	else
		m_vertices.emplace_back(Vertex{ { z, x, y}, ConvertColor(color), {u, v} });
	++m_currentPrim->vertices;
	++m_tempIndex;

	if (m_tempIndex == m_tempMax)
	{
		m_tempIndex = 0;

		// used up all the vertices. Add indexes
		switch (m_type)
		{
		case Prim_Points:
			AddPoint();
			break;

		case Prim_Lines:
			AddLine();
			break;

		case Prim_Triangles:
			AddTriangle();
			break;

		case Prim_Quads:
			AddQuad();
			break;
		}
	}
}

void RenderList::AddPoint()
{
	uint32_t index = (uint32_t)m_vertices.size() - 1;
	m_currentPrim->indices.push_back(index);
	m_currentPrim->count++;
}

void RenderList::AddLine()
{
	uint32_t index = (uint32_t)m_vertices.size() - 2;
	m_currentPrim->indices.push_back(index);
	m_currentPrim->indices.push_back(index + 1);
	m_currentPrim->count++;
}

inline float distance(const glm::vec3& a, const glm::vec3& b)
{
	return glm::distance(a, b);
}

void RenderList::AddTriangle()
{
	uint32_t index = (uint32_t)m_vertices.size() - 3;
	m_currentPrim->indices.push_back(index);
	m_currentPrim->indices.push_back(index + 1);
	m_currentPrim->indices.push_back(index + 2);
	m_currentPrim->count++;
}

void RenderList::AddQuad()
{
	uint32_t index = (uint32_t)m_vertices.size() - 4;
	m_currentPrim->indices.push_back(index);
	m_currentPrim->indices.push_back(index + 1);
	m_currentPrim->indices.push_back(index + 2);
	m_currentPrim->indices.push_back(index + 1);
	m_currentPrim->indices.push_back(index + 3);
	m_currentPrim->indices.push_back(index + 2);
	m_currentPrim->count++;
}

void RenderList::End()
{
}

void RenderList::Render()
{
#if HAS_DIRECTX_9
	GenerateBuffers();

	if (!m_pVB || !m_pIB)
		return;

	m_pDevice->SetStreamSource(0, m_pVB, 0, sizeof(Vertex));
	m_pDevice->SetIndices(m_pIB);
	m_pDevice->SetFVF(VertexType);

	if (m_mtx)
	{
		m_pDevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)m_mtx);
		m_mtx = nullptr;
	}

	int start = std::max(0u, m_firstRender);
	int end = std::min<int>((int)m_prims.size(), m_lastRender);

	for (auto& p : m_prims)
	{
		PrimitiveList* l = p.get();

		if (l->indices.size() == 0)
			continue;

		switch (m_type)
		{
		case Prim_Points:
			m_pDevice->DrawIndexedPrimitive(D3DPT_POINTLIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,             // NumVertices
				l->startingIndex,        // StartIndex
				l->count                 // PrimitiveCount
				);
			break;

		case Prim_Lines:
			m_pDevice->DrawIndexedPrimitive(D3DPT_LINELIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,             // NumVertices
				l->startingIndex,        // StartIndex
				l->count                 // PrimitiveCount
				);
			break;

		case Prim_Triangles:
		case Prim_Quads:
			m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,             // NumVertices
				l->startingIndex,        // StartIndex
				l->count                 // PrimitiveCount
				);
			break;
		}
	}
#endif
}

void RenderList::GenerateBuffers()
{
#if HAS_DIRECTX_9
	bool rebuildBuffers = false;
	int vertexSize = 0, indexSize = 0;

	if (!m_pVB && !m_vertices.empty())
	{
		vertexSize = (int)m_vertices.size();
		if (m_pDevice->CreateVertexBuffer(vertexSize * sizeof(Vertex),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, VertexType, D3DPOOL_DEFAULT, &m_pVB, nullptr) < 0)
		{
			return;
		}

		rebuildBuffers = true;
	}

	if (!m_pIB && m_prims.size() > 0)
	{
		// rebuild the indices and render sets
		for (auto& p : m_prims)
			indexSize += (int)p->indices.size();

		if (m_pDevice->CreateIndexBuffer(indexSize * sizeof(uint32_t),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &m_pIB, nullptr) < 0)
		{
			return;
		}

		rebuildBuffers = true;
	}

	if (rebuildBuffers)
	{
		Vertex* vertexDest;
		uint32_t* indexDest;

		if (m_pVB->Lock(0, vertexSize * sizeof(Vertex), (void**)&vertexDest, D3DLOCK_DISCARD) < 0)
			return;
		if (m_pIB->Lock(0, indexSize * sizeof(uint32_t), (void**)&indexDest, D3DLOCK_DISCARD) < 0)
			return;

		// fill vertex buffer
		memcpy(vertexDest, &m_vertices[0], vertexSize * sizeof(Vertex));

		// build the index buffer
		int currentIndex = 0;
		for (auto& p : m_prims)
		{
			PrimitiveList* l = p.get();

			if (l->indices.size() == 0)
				continue;

			size_t source_len = l->indices.size() * sizeof(uint32_t);
			uint32_t* source = &l->indices[0];
			uint32_t* dest = indexDest + currentIndex;
			memcpy(dest, source, source_len);

			l->startingIndex = currentIndex;
			currentIndex += (int)l->indices.size();
		}

		m_lastRender = (int)m_prims.size();
		m_pVB->Unlock();
		m_pIB->Unlock();
	}
#endif
}

void RenderList::InvalidateDeviceObjects()
{
#if HAS_DIRECTX_9
	if (m_pVB)
	{
		m_pVB->Release();
		m_pVB = nullptr;
	}

	if (m_pIB)
	{
		m_pIB->Release();
		m_pIB = nullptr;
	}
#endif
}

bool RenderList::CreateDeviceObjects()
{
	if (!m_pDevice)
		return false;

	// We'll create our objects when we need them.

	return true;
}

void RenderList::Debug_SetRenderRange(int begin, int end)
{
	m_firstRender = begin;
	m_lastRender = end;

	if (m_firstRender >= m_lastRender)
		m_firstRender = m_lastRender;

}

void RenderList::Debug_GetRenderRange(int& begin, int& end)
{
	begin = m_firstRender;
	end = m_lastRender;
}
