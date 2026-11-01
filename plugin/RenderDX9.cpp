//
// RenderDX9.cpp
//

#include "pch.h"
#include "RenderDX9.h"

#if HAS_DIRECTX_9

#include "common/Utilities.h"
#include "plugin/EmbedResources.h"

#pragma comment(lib, "d3d9")
#ifdef _DEBUG
#pragma comment(lib, "d3dx9d")
#else
#pragma comment(lib, "d3dx9")
#endif

// Matches RenderList::Vertex
static constexpr DWORD GeometryFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

bool RenderDX9::Initialize(IDirect3DDevice9* device)
{
	m_device = device;

	// Volume line effect
	auto shaderFile = LoadResource(IDR_VOLUMELINES_FX);
	if (shaderFile.empty())
		return false;

	wil::com_ptr<ID3DXBuffer> errors;
	HRESULT hr = D3DXCreateEffect(m_device.get(), shaderFile.data(), (uint32_t)shaderFile.length(),
		nullptr, nullptr, 0, nullptr, &m_volumeEffect, &errors);
	if (FAILED(hr))
	{
		SPDLOG_ERROR("Error loading navigation line shader: {}",
			errors ? std::string_view{ (const char*)errors->GetBufferPointer(), errors->GetBufferSize() } : "");
		return false;
	}

	// Matches NavigationLine::TVertex
	const D3DVERTEXELEMENT9 vertexElements[] =
	{
		{ 0,  0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
		{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,  0 },
		{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
		{ 0, 36,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
		{ 0, 40,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2 },
		{ 0, 44,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  3 },
		D3DDECL_END()
	};

	return SUCCEEDED(m_device->CreateVertexDeclaration(vertexElements, &m_volumeDeclaration));
}

void RenderDX9::BeginFrame()
{
	D3DPERF_BeginEvent(D3DCOLOR_XRGB(255, 0, 0), L"RenderHandler::PerformRender");

	// General state that the bind functions below don't set themselves.
	m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
	m_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	m_device->SetRenderState(D3DRS_FOGENABLE, false);
	m_device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
	m_device->SetRenderState(D3DRS_CLIPPING, true);
	m_device->SetRenderState(D3DRS_DEPTHBIAS, 0);
	m_device->SetRenderState(D3DRS_DITHERENABLE, false);
	m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	m_device->SetRenderState(D3DRS_LASTPIXEL, true);
	m_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
	m_device->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	m_device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	m_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
	m_device->SetRenderState(D3DRS_WRAP0, 0);
	m_device->SetRenderState(D3DRS_WRAP1, 0);
	m_device->SetRenderState(D3DRS_WRAP2, 0);
	m_device->SetRenderState(D3DRS_WRAP3, 0);
	m_device->SetRenderState(D3DRS_WRAP4, 0);
	m_device->SetRenderState(D3DRS_WRAP5, 0);
	m_device->SetRenderState(D3DRS_WRAP6, 0);
	m_device->SetRenderState(D3DRS_WRAP7, 0);
	m_device->SetRenderState(D3DRS_WRAP8, 0);
	m_device->SetRenderState(D3DRS_WRAP9, 0);
	m_device->SetRenderState(D3DRS_WRAP10, 0);
	m_device->SetRenderState(D3DRS_WRAP11, 0);
	m_device->SetRenderState(D3DRS_WRAP12, 0);
	m_device->SetRenderState(D3DRS_WRAP13, 0);
	m_device->SetRenderState(D3DRS_WRAP15, 0);
}

void RenderDX9::EndFrame()
{
	D3DPERF_EndEvent();
}

void RenderDX9::BindGeometry(const glm::mat4& world, bool depthTest)
{
	m_device->SetPixelShader(nullptr);
	m_device->SetVertexShader(nullptr);
	m_device->SetFVF(GeometryFVF);

	// glm's column-major layout is the same memory layout as a row-major D3DMATRIX.
	m_device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));

	m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	m_device->SetRenderState(D3DRS_ALPHATESTENABLE, true);
	m_device->SetRenderState(D3DRS_ALPHAREF, 0);
	m_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);

	m_device->SetRenderState(D3DRS_ZENABLE, depthTest);
	m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	m_device->SetRenderState(D3DRS_ZWRITEENABLE, true);

	m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	m_device->SetRenderState(D3DRS_LIGHTING, false);
	m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	m_device->SetRenderState(D3DRS_STENCILENABLE, false);

	// Vertex color only, no textures
	for (int i = 0; i < 8; i++)
	{
		m_device->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		m_device->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
	}
}

void RenderDX9::BindVolumeLines(IDirect3DVertexBuffer9* vertexBuffer, UINT stride)
{
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	m_device->SetTransform(D3DTS_WORLD, &world);

	D3DXMATRIX view, proj;
	m_device->GetTransform(D3DTS_VIEW, &view);
	m_device->GetTransform(D3DTS_PROJECTION, &proj);

	D3DXMATRIX mWV = world * view;
	D3DXMATRIX mWVP = mWV * proj;
	m_volumeEffect->SetMatrix("mWV", &mWV);
	m_volumeEffect->SetMatrix("mWVP", &mWVP);

	m_device->GetRenderState(D3DRS_ZENABLE, &m_savedZEnable);
	m_device->GetRenderState(D3DRS_ZFUNC, &m_savedZFunc);

	m_device->SetVertexDeclaration(m_volumeDeclaration.get());
	m_device->SetStreamSource(0, vertexBuffer, 0, stride);

	m_device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 0, 0);

	UINT passes = 0;
	m_volumeEffect->Begin(&passes, 0);
	m_inVolumePass = false;
}

void RenderDX9::SetVolumeLinePass(int pass, const glm::vec4& lineColor, const glm::vec4& linkColor, float lineWidth)
{
	if (m_inVolumePass)
		m_volumeEffect->EndPass();

	m_volumeEffect->BeginPass(pass);
	m_inVolumePass = true;

	bool hidden = pass == 0 || pass == 2;
	m_device->SetRenderState(D3DRS_ZFUNC, hidden ? D3DCMP_GREATER : D3DCMP_LESSEQUAL);

	m_volumeEffect->SetFloat("opacity", lineColor.a);
	m_volumeEffect->SetFloat("lineWidth", lineWidth);
	m_volumeEffect->SetVector("lineColor", reinterpret_cast<const D3DXVECTOR4*>(&lineColor));
	m_volumeEffect->SetVector("linkColor", reinterpret_cast<const D3DXVECTOR4*>(&linkColor));
	m_volumeEffect->CommitChanges();
}

void RenderDX9::DrawVolumeLine(UINT startVertex, UINT primitiveCount)
{
	m_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, startVertex, primitiveCount);
}

void RenderDX9::EndVolumeLines()
{
	if (m_inVolumePass)
		m_volumeEffect->EndPass();
	m_inVolumePass = false;

	m_volumeEffect->End();

	m_device->SetRenderState(D3DRS_ZENABLE, m_savedZEnable);
	m_device->SetRenderState(D3DRS_ZFUNC, m_savedZFunc);
}

#endif // HAS_DIRECTX_9
