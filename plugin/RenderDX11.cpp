//
// RenderDX11.cpp
//

#include "pch.h"
#include "RenderDX11.h"

#if HAS_DIRECTX_11

#include <d3dcompiler.h>
#include <mq/api/RenderDoc.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

//----------------------------------------------------------------------------
// Shaders

// Matrices are uploaded straight from glm (column-major), which matches
// HLSL's default packing, so we multiply as mul(matrix, vector).

static const char s_geometryShader[] = R"(
cbuffer Constants : register(b0)
{
	float4x4 WorldViewProj;
};

struct VSInput
{
	float3 pos   : POSITION;
	float4 color : COLOR;
};

struct VSOutput
{
	float4 pos   : SV_Position;
	float4 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.pos = mul(WorldViewProj, float4(input.pos, 1.0f));
	output.color = input.color;
	return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
	return input.color;
}
)";

struct GeometryConstants
{
	glm::mat4 worldViewProj;
};

// Port of resources/VolumeLines.fx. Each segment is a 4 vertex strip; the vertex
// shader pushes vertices apart in screen space to give the line its width.
static const char s_volumeLineShader[] = R"(
cbuffer Constants : register(b0)
{
	float4x4 WorldViewProj;
	float4 lineColor;   // rgb + opacity
	float4 linkColor;   // rgb + opacity
	float lineWidth;
};

struct VSInput
{
	float3 pos       : POSITION;   // this end of the segment
	float3 otherPos  : NORMAL;     // the other end of the segment
	float3 adjPos    : TEXCOORD0;  // end of the previous/next segment
	float  thickness : TEXCOORD1;
	float  adjHint   : TEXCOORD2;
	float  type      : TEXCOORD3;  // > 0 for off-mesh links
};

struct VSOutput
{
	float4 pos   : SV_Position;
	float4 color : COLOR;
};

float2 perp(float2 p)
{
	return float2(p.y, -p.x);
}

VSOutput VSMain(VSInput input)
{
	float thickness = input.thickness * lineWidth * 4;

	float4 posStart = mul(WorldViewProj, float4(input.pos, 1.0f));
	float4 posEnd = mul(WorldViewProj, float4(input.otherPos, 1.0f));
	float4 posAdj = mul(WorldViewProj, float4(input.adjPos, 1.0f));

	float2 startPos2D = posStart.xy / posStart.w;
	float2 endPos2D = posEnd.xy / posEnd.w;
	float2 adjPos2D = posAdj.xy / posAdj.w;

	float2 v0 = normalize(adjPos2D - startPos2D);
	float2 v1 = normalize(startPos2D - endPos2D);

	float2 adjustment;
	if (input.adjHint != 0)
	{
		// miter: average the directions of the adjoining segments
		float2 tangent = normalize(v0 + v1);
		adjustment = perp(tangent) * thickness;
	}
	else
	{
		adjustment = perp(v1) * thickness;
	}

	posStart.xy += adjustment;

	VSOutput output;
	output.pos = posStart;
	output.color = input.type > 0 ? linkColor : lineColor;
	return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
	return input.color;
}
)";

struct VolumeLineConstants
{
	glm::mat4 worldViewProj;
	glm::vec4 lineColor;
	glm::vec4 linkColor;
	float lineWidth;
	float padding[3];
};

//----------------------------------------------------------------------------

static wil::com_ptr<ID3DBlob> CompileShader(const char* source, size_t length, const char* name,
	const char* entryPoint, const char* target)
{
	wil::com_ptr<ID3DBlob> blob, errors;

	HRESULT hr = D3DCompile(source, length, name, nullptr, nullptr, entryPoint, target,
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
	if (FAILED(hr))
	{
		SPDLOG_ERROR("Failed to compile shader {} ({}): {}", name, entryPoint,
			errors ? std::string_view{ (const char*)errors->GetBufferPointer(), errors->GetBufferSize() } : "");
		return nullptr;
	}

	return blob;
}

static wil::com_ptr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device, UINT size)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	wil::com_ptr<ID3D11Buffer> buffer;
	device->CreateBuffer(&desc, nullptr, &buffer);
	return buffer;
}

bool RenderDX11::Initialize(ID3D11Device* device)
{
	m_device = device;
	m_device->GetImmediateContext(&m_context);

	// Geometry shaders + input layout matching RenderList::Vertex (pos, D3DCOLOR, uv)
	wil::com_ptr<ID3DBlob> vs = CompileShader(s_geometryShader, sizeof(s_geometryShader) - 1, "MQ2Nav_Geometry", "VSMain", "vs_4_0");
	wil::com_ptr<ID3DBlob> ps = CompileShader(s_geometryShader, sizeof(s_geometryShader) - 1, "MQ2Nav_Geometry", "PSMain", "ps_4_0");
	if (!vs || !ps)
		return false;

	if (FAILED(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_geometryVS))
		|| FAILED(m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_geometryPS)))
	{
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC geometryLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // D3DCOLOR
	};
	if (FAILED(m_device->CreateInputLayout(geometryLayout, (UINT)std::size(geometryLayout),
		vs->GetBufferPointer(), vs->GetBufferSize(), &m_geometryLayout)))
	{
		return false;
	}

	m_geometryCB = CreateConstantBuffer(m_device.get(), sizeof(GeometryConstants));
	if (!m_geometryCB)
		return false;

	// Standard alpha blending. Leave the destination alpha alone, the game may depend on it.
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED
		| D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	if (FAILED(m_device->CreateBlendState(&blendDesc, &m_blendAlpha)))
		return false;

	// Same as the DX9 path (D3DCULL_CW): clockwise triangles are culled.
	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.FrontCounterClockwise = TRUE;
	rasterDesc.DepthClipEnable = TRUE;
	if (FAILED(m_device->CreateRasterizerState(&rasterDesc, &m_geometryRaster)))
		return false;

	// Test against the scene depth, but never write it: the game samples its
	// depth buffer later in the frame.
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(m_device->CreateDepthStencilState(&depthDesc, &m_depthTestOn)))
		return false;

	depthDesc.DepthEnable = FALSE;
	if (FAILED(m_device->CreateDepthStencilState(&depthDesc, &m_depthTestOff)))
		return false;

	return InitializeVolumeLines();
}

bool RenderDX11::InitializeVolumeLines()
{
	wil::com_ptr<ID3DBlob> vs = CompileShader(s_volumeLineShader, sizeof(s_volumeLineShader) - 1, "MQ2Nav_VolumeLines", "VSMain", "vs_4_0");
	wil::com_ptr<ID3DBlob> ps = CompileShader(s_volumeLineShader, sizeof(s_volumeLineShader) - 1, "MQ2Nav_VolumeLines", "PSMain", "ps_4_0");
	if (!vs || !ps)
		return false;

	if (FAILED(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_volumeVS))
		|| FAILED(m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_volumePS)))
	{
		return false;
	}

	// Matches NavigationLine::TVertex
	const D3D11_INPUT_ELEMENT_DESC volumeLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,       0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 3, DXGI_FORMAT_R32_FLOAT,       0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(m_device->CreateInputLayout(volumeLayout, (UINT)std::size(volumeLayout),
		vs->GetBufferPointer(), vs->GetBufferSize(), &m_volumeLayout)))
	{
		return false;
	}

	m_volumeCB = CreateConstantBuffer(m_device.get(), sizeof(VolumeLineConstants));
	if (!m_volumeCB)
		return false;

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	if (FAILED(m_device->CreateRasterizerState(&rasterDesc, &m_volumeRaster)))
		return false;

	// The line passes mark their pixels in the stencil buffer, and the border
	// passes only draw where the stencil is still clear.
	for (int pass = 0; pass < 4; ++pass)
	{
		bool hidden = pass == 0 || pass == 2;
		bool border = pass >= 2;

		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = hidden ? D3D11_COMPARISON_GREATER : D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = TRUE;
		desc.StencilReadMask = 0xff;
		desc.StencilWriteMask = 0xff;
		desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = border ? D3D11_STENCIL_OP_ZERO : D3D11_STENCIL_OP_REPLACE;
		desc.FrontFace.StencilFunc = border ? D3D11_COMPARISON_EQUAL : D3D11_COMPARISON_ALWAYS;
		desc.BackFace = desc.FrontFace;

		if (FAILED(m_device->CreateDepthStencilState(&desc, &m_volumeDepth[pass])))
			return false;
	}

	return true;
}

void RenderDX11::BeginFrame()
{
	RenderDoc_BeginEvent(MQColor(255, 0, 0), L"RenderHandler::PerformRender");

	// EQ's renderer still tracks the D3D9-style transforms.
	glm::mat4 view, proj;
	gpD3D9Device->GetTransform(D3DTS_VIEW, reinterpret_cast<D3DMATRIX*>(&view));
	gpD3D9Device->GetTransform(D3DTS_PROJECTION, reinterpret_cast<D3DMATRIX*>(&proj));

	// Row-major D3D matrices read as column-major glm are transposed, so the order flips.
	m_viewProj = proj * view;
}

void RenderDX11::EndFrame()
{
	RenderDoc_EndEvent();
}

template <typename T>
void RenderDX11::UpdateConstants(ID3D11Buffer* buffer, const T& data)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &data, sizeof(T));
		m_context->Unmap(buffer, 0);
	}
}

void RenderDX11::BindGeometry(const glm::mat4& world, bool depthTest)
{
	UpdateConstants(m_geometryCB.get(), GeometryConstants{ m_viewProj * world });

	// Everything bound here is saved and restored by MQ around the render callback.
	m_context->IASetInputLayout(m_geometryLayout.get());
	m_context->VSSetShader(m_geometryVS.get(), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_geometryCB.addressof());
	m_context->GSSetShader(nullptr, nullptr, 0);
	m_context->PSSetShader(m_geometryPS.get(), nullptr, 0);

	m_context->OMSetBlendState(m_blendAlpha.get(), nullptr, 0xffffffff);
	m_context->OMSetDepthStencilState(depthTest ? m_depthTestOn.get() : m_depthTestOff.get(), 0);
	m_context->RSSetState(m_geometryRaster.get());
}

void RenderDX11::BindVolumeLines(ID3D11Buffer* vertexBuffer, UINT stride)
{
	UINT offset = 0;
	m_context->IASetInputLayout(m_volumeLayout.get());
	m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_context->VSSetShader(m_volumeVS.get(), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_volumeCB.addressof());
	m_context->GSSetShader(nullptr, nullptr, 0);
	m_context->PSSetShader(m_volumePS.get(), nullptr, 0);

	m_context->OMSetBlendState(m_blendAlpha.get(), nullptr, 0xffffffff);
	m_context->RSSetState(m_volumeRaster.get());

	wil::com_ptr<ID3D11DepthStencilView> dsv;
	m_context->OMGetRenderTargets(0, nullptr, &dsv);
	if (dsv)
	{
		m_context->ClearDepthStencilView(dsv.get(), D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}

void RenderDX11::SetVolumeLinePass(int pass, const glm::vec4& lineColor, const glm::vec4& linkColor, float lineWidth)
{
	UpdateConstants(m_volumeCB.get(), VolumeLineConstants{ m_viewProj, lineColor, linkColor, lineWidth });

	m_context->OMSetDepthStencilState(m_volumeDepth[pass].get(), pass < 2 ? 1 : 0);
}

#endif // HAS_DIRECTX_11
