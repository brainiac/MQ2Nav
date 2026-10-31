//
// RenderDX11.cpp
//

#include "pch.h"
#include "RenderDX11.h"

#if HAS_DIRECTX_11

#include <d3dcompiler.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

using Microsoft::WRL::ComPtr;

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

//----------------------------------------------------------------------------

static ComPtr<ID3DBlob> CompileShader(const char* source, size_t length, const char* name,
	const char* entryPoint, const char* target)
{
	ComPtr<ID3DBlob> blob, errors;

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

static ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device, UINT size)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ComPtr<ID3D11Buffer> buffer;
	device->CreateBuffer(&desc, nullptr, &buffer);
	return buffer;
}

bool RenderDX11::Initialize(ID3D11Device* device)
{
	m_device = device;
	m_device->GetImmediateContext(&m_context);

	// Geometry shaders + input layout matching RenderList::Vertex (pos, D3DCOLOR, uv)
	ComPtr<ID3DBlob> vs = CompileShader(s_geometryShader, sizeof(s_geometryShader) - 1, "MQ2Nav_Geometry", "VSMain", "vs_4_0");
	ComPtr<ID3DBlob> ps = CompileShader(s_geometryShader, sizeof(s_geometryShader) - 1, "MQ2Nav_Geometry", "PSMain", "ps_4_0");
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

	m_geometryCB = CreateConstantBuffer(m_device.Get(), sizeof(GeometryConstants));
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

	return true;
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
	UpdateConstants(m_geometryCB.Get(), GeometryConstants{ m_viewProj * world });

	// Everything bound here is saved and restored by MQ around the render callback.
	m_context->IASetInputLayout(m_geometryLayout.Get());
	m_context->VSSetShader(m_geometryVS.Get(), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, m_geometryCB.GetAddressOf());
	m_context->GSSetShader(nullptr, nullptr, 0);
	m_context->PSSetShader(m_geometryPS.Get(), nullptr, 0);

	m_context->OMSetBlendState(m_blendAlpha.Get(), nullptr, 0xffffffff);
	m_context->OMSetDepthStencilState(depthTest ? m_depthTestOn.Get() : m_depthTestOff.Get(), 0);
	m_context->RSSetState(m_geometryRaster.Get());
}

#endif // HAS_DIRECTX_11
