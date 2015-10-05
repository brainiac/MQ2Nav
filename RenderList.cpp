//
// RenderList.cpp
//

#include "RenderList.h"

#include <d3d9.h>

RenderList::RenderList(IDirect3DDevice9* pDevice)
	: m_pDevice(pDevice)
{
	assert(m_pDevice != nullptr);
}

RenderList::~RenderList()
{
	InvalidateDeviceObjects();
}

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
	}
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
}

void RenderList::Begin(PrimitiveType type, float size /* = 1.0f */)
{
	std::unique_ptr<PrimitiveList> prims = std::make_unique<PrimitiveList>();
	m_currentPrim = prims.get();

	// add prims to the list
	m_prims.emplace_back(std::move(prims));

	m_currentPrim->type = type;
	m_currentPrim->size = size;

	// count how many vertices are left before we update the indices
	m_currVertexCnt = PrimVertexCount(type);
}

void RenderList::AddVertex(float x, float y, float z, unsigned int color, float u, float v)
{
	m_vertices.emplace_back(Vertex{ { z, x, y }, ConvertColor(color),{ u, v } });

	--m_currVertexCnt;
	if (m_currVertexCnt == 0)
	{
		int16_t index = m_vertices.size() - 1;
		std::vector<uint16_t>& indices = m_currentPrim->indices;
		m_currentPrim->vertices++;

		// used up all the vertices. Add indexes
		switch (m_currentPrim->type)
		{
		case Prim_Points:
			indices.push_back(index);
			break;

		case Prim_Lines:
			indices.push_back(index - 1);
			indices.push_back(index - 0);
			break;

		case Prim_Triangles:
			indices.push_back(index - 2);
			indices.push_back(index - 1);
			indices.push_back(index - 0);
			break;

		case Prim_Quads:
			indices.push_back(index - 3);
			indices.push_back(index - 2);
			indices.push_back(index - 1);
			indices.push_back(index - 2);
			indices.push_back(index - 0);
			indices.push_back(index - 1);
			break;
		}

		m_currVertexCnt = PrimVertexCount(m_currentPrim->type);
	}
}

void RenderList::End()
{
}

void RenderList::Render()
{
	bool rebuildBuffers = false;
	int vertexSize = 0, indexSize = 0;

	if (!m_pVB && m_vertices.size() > 0)
	{
		vertexSize = m_vertices.size();
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
		for (int i = 0; i < m_prims.size(); ++i)
			indexSize += m_prims[i]->indices.size();

		if (m_pDevice->CreateIndexBuffer(indexSize * sizeof(uint16_t),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pIB, nullptr) < 0)
		{
			return;
		}

		rebuildBuffers = true;
	}

	if (rebuildBuffers)
	{
		Vertex* vertexDest;
		uint16_t* indexDest;

		if (m_pVB->Lock(0, vertexSize * sizeof(Vertex), (void**)&vertexDest, D3DLOCK_DISCARD) < 0)
			return;
		if (m_pIB->Lock(0, indexSize * sizeof(uint16_t), (void**)&indexDest, D3DLOCK_DISCARD) < 0)
			return;

		// fill vertex buffer
		memcpy(vertexDest, &m_vertices[0], vertexSize * sizeof(Vertex));

		// build the index buffer
		int currentIndex = 0;
		for (int i = 0; i < m_prims.size(); ++i)
		{
			PrimitiveList* l = m_prims[i].get();

			for (int j = 0; j < l->indices.size(); ++j)
			{
				memcpy(indexDest + currentIndex, &l->indices[0], l->indices.size() * sizeof(uint16_t));
				l->startingIndex = currentIndex;
			}
			currentIndex += l->indices.size();
		}

		m_pVB->Unlock();
		m_pIB->Unlock();
	}

	m_pDevice->SetStreamSource(0, m_pVB, 0, sizeof(Vertex));
	m_pDevice->SetIndices(m_pIB);
	m_pDevice->SetFVF(VertexType);

	m_pDevice->SetPixelShader(NULL);
	m_pDevice->SetVertexShader(NULL);
	m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	m_pDevice->SetRenderState(D3DRS_LIGHTING, false);
	m_pDevice->SetRenderState(D3DRS_ZENABLE, true);
	m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	m_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, true);
	m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	for (int i = 0; i < m_prims.size(); ++i)
	{
		PrimitiveList* l = m_prims[i].get();

		if (l->indices.size() == 0)
			continue;

		switch (l->type)
		{
		case Prim_Points:
			m_pDevice->DrawIndexedPrimitive(D3DPT_POINTLIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,             // NumVertices
				l->startingIndex,        // StartIndex
				l->indices.size()        // PrimitiveCount
				);
			break;

		case Prim_Lines:
			m_pDevice->DrawIndexedPrimitive(D3DPT_LINELIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,       // NumVertices
				l->startingIndex,        // StartIndex
				l->indices.size() / 2    // PrimitiveCount
				);
			break;

		case Prim_Triangles:
		case Prim_Quads:
			m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
				0,                       // BaseVertexIndex
				l->indices[0],           // MinIndex
				l->vertices,             // NumVertices
				l->startingIndex,        // StartIndex
				l->indices.size() / 3    // PrimitiveCount
				);
			break;
		}
	}

	m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
}

void RenderList::InvalidateDeviceObjects()
{
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
}

bool RenderList::CreateDeviceObjects()
{
	if (!m_pDevice)
		return false;

	// We'll create our objects when we need them.

	return true;
}
