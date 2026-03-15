//
// DX11Renderer.cpp
//
// Implementation of NavDX11Resources — centralized DX11 rendering
// resources for MQ2Nav.  Replaces the DX9 fixed-function pipeline
// and D3DXEffect-based rendering that was disabled when EQ moved to DX11.
//

#include "pch.h"
#include "DX11Renderer.h"

#include <mq/Plugin.h>
#include <spdlog/spdlog.h>

//============================================================================
// HLSL shader source strings
//============================================================================

//--- Simple geometry vertex shader -------------------------------------------
static const char* s_SimpleVS =
	"cbuffer CB : register(b0)\n"
	"{\n"
	"    row_major float4x4 WorldViewProj;\n"
	"    row_major float4x4 World;\n"
	"};\n"
	"\n"
	"struct VS_INPUT\n"
	"{\n"
	"    float3 pos : POSITION;\n"
	"    float4 col : COLOR;\n"
	"    float2 uv  : TEXCOORD0;\n"
	"};\n"
	"\n"
	"struct VS_OUTPUT\n"
	"{\n"
	"    float4 Pos   : SV_Position;\n"
	"    float4 Color : COLOR;\n"
	"    float2 UV    : TEXCOORD0;\n"
	"};\n"
	"\n"
	"VS_OUTPUT main(VS_INPUT IN)\n"
	"{\n"
	"    VS_OUTPUT OUT;\n"
	"    OUT.Pos   = mul(float4(IN.pos, 1.0f), WorldViewProj);\n"
	"    OUT.Color = IN.col;\n"
	"    OUT.UV    = IN.uv;\n"
	"    return OUT;\n"
	"}\n";

//--- Simple geometry pixel shader --------------------------------------------
static const char* s_SimplePS =
	"struct PS_INPUT\n"
	"{\n"
	"    float4 Color : COLOR;\n"
	"    float2 UV    : TEXCOORD0;\n"
	"};\n"
	"\n"
	"float4 main(PS_INPUT IN) : SV_Target\n"
	"{\n"
	"    return IN.Color;\n"
	"}\n";

//--- Volume line vertex shader (port of VolumeLines.fx) ----------------------
static const char* s_VolumeVS =
	"cbuffer CB : register(b0)\n"
	"{\n"
	"    row_major float4x4 mWVP;\n"
	"    row_major float4x4 mWV;\n"
	"    float4   lineColor;\n"
	"    float4   linkColor;\n"
	"    float    opacity;\n"
	"    float    lineWidth;\n"
	"    float    _pad0;\n"
	"    float    _pad1;\n"
	"};\n"
	"\n"
	"struct VS_INPUT\n"
	"{\n"
	"    float3 pos       : POSITION;\n"
	"    float3 otherPos  : NORMAL;\n"
	"    float3 adjPos    : TEXCOORD0;\n"
	"    float  thickness : TEXCOORD1;\n"
	"    float  adjHint   : TEXCOORD2;\n"
	"    float  type      : TEXCOORD3;\n"
	"};\n"
	"\n"
	"struct VS_OUTPUT\n"
	"{\n"
	"    float4 Pos  : SV_Position;\n"
	"    float  Type : TEXCOORD0;\n"
	"};\n"
	"\n"
	"VS_OUTPUT main(VS_INPUT IN)\n"
	"{\n"
	"    VS_OUTPUT OUT;\n"
	"\n"
	"    float thickness = IN.thickness * lineWidth * 4;\n"
	"\n"
	"    float4 posStart = float4(IN.pos, 1);\n"
	"    float4 posEnd   = float4(IN.otherPos, 1);\n"
	"    float4 posAdj   = float4(IN.adjPos, 1);\n"
	"\n"
	"    posStart = mul(posStart, mWVP);\n"
	"    posEnd   = mul(posEnd,   mWVP);\n"
	"    posAdj   = mul(posAdj,   mWVP);\n"
	"\n"
	"    float2 startPos2D = posStart.xy / posStart.w;\n"
	"    float2 endPos2D   = posEnd.xy   / posEnd.w;\n"
	"    float2 adjPos2D   = posAdj.xy   / posAdj.w;\n"
	"\n"
	"    float2 v0_ = normalize(adjPos2D - startPos2D);\n"
	"    float2 v1_ = normalize(startPos2D - endPos2D);\n"
	"    float2 v2_ = normalize(adjPos2D - startPos2D);\n"
	"\n"
	"    bool isEnd = (IN.adjHint == 0);\n"
	"\n"
	"    float2 adjustment;\n"
	"    if (!isEnd)\n"
	"    {\n"
	"        float2 tangent;\n"
	"        if (IN.adjHint > 0)\n"
	"            tangent = normalize(v1_ + v2_);\n"
	"        else\n"
	"            tangent = normalize(v0_ + v1_);\n"
	"        adjustment = float2(tangent.y, -tangent.x) * thickness;\n"
	"    }\n"
	"    else\n"
	"    {\n"
	"        adjustment = float2(v1_.y, -v1_.x) * thickness;\n"
	"    }\n"
	"\n"
	"    posStart.xy += adjustment;\n"
	"\n"
	"    OUT.Pos  = posStart;\n"
	"    OUT.Type = IN.type;\n"
	"    return OUT;\n"
	"}\n";

//--- Volume line pixel shader ------------------------------------------------
static const char* s_VolumePS =
	"cbuffer CB : register(b0)\n"
	"{\n"
	"    row_major float4x4 mWVP;\n"
	"    row_major float4x4 mWV;\n"
	"    float4   lineColor;\n"
	"    float4   linkColor;\n"
	"    float    opacity;\n"
	"    float    lineWidth;\n"
	"    float    _pad0;\n"
	"    float    _pad1;\n"
	"};\n"
	"\n"
	"struct PS_INPUT\n"
	"{\n"
	"    float Type : TEXCOORD0;\n"
	"};\n"
	"\n"
	"float4 main(PS_INPUT IN) : SV_Target\n"
	"{\n"
	"    if (IN.Type > 0.0f)\n"
	"        return float4(linkColor.rgb, opacity);\n"
	"    else\n"
	"        return float4(lineColor.rgb, opacity);\n"
	"}\n";

//============================================================================
// Shader compilation helper
//============================================================================

static bool CompileShader(const char* source, const char* entryPoint,
	const char* target, ComPtr<ID3DBlob>& blobOut)
{
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3DCompile(
		source, strlen(source),
		nullptr,   // sourceName
		nullptr,   // defines
		nullptr,   // includes
		entryPoint,
		target,
		0,         // flags1
		0,         // flags2
		&blobOut,
		&errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			SPDLOG_ERROR("MQ2Nav: Shader compile failed ({}): {}",
				target,
				static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		else
		{
			SPDLOG_ERROR("MQ2Nav: Shader compile failed ({}) hr=0x{:08X}",
				target, static_cast<unsigned>(hr));
		}
		return false;
	}

	return true;
}

//============================================================================
// NavDX11Resources implementation
//============================================================================

bool NavDX11Resources::Initialize(ID3D11Device* dev)
{
	device = dev;
	device->GetImmediateContext(&context);

	if (!CompileShaders())
		return false;

	if (!CreateStateObjects())
		return false;

	if (!CreateConstantBuffers())
		return false;

	return true;
}

void NavDX11Resources::Shutdown()
{
	simpleVS.Reset();
	simplePS.Reset();
	simpleInputLayout.Reset();
	simpleCB.Reset();

	volumeVS.Reset();
	volumePS.Reset();
	volumeInputLayout.Reset();
	volumeCB.Reset();

	blendAlpha.Reset();
	rasterizerNoCull.Reset();
	rasterizerCWCull.Reset();
	dsNavMesh.Reset();

	for (int i = 0; i < 4; ++i)
		dsVolumeLine[i].Reset();

	if (context)
	{
		context->Release();
		context = nullptr;
	}
	device = nullptr;
}

//============================================================================
// CompileShaders
//============================================================================

bool NavDX11Resources::CompileShaders()
{
	HRESULT hr;

	//--- Simple geometry shaders ---
	{
		ComPtr<ID3DBlob> vsBlob;
		if (!CompileShader(s_SimpleVS, "main", "vs_4_0", vsBlob))
			return false;

		hr = device->CreateVertexShader(
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			nullptr, &simpleVS);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateVertexShader (simple) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}

		// Create input layout from the same VS blob
		D3D11_INPUT_ELEMENT_DESC simpleLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = device->CreateInputLayout(
			simpleLayout, _countof(simpleLayout),
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&simpleInputLayout);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateInputLayout (simple) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}

		ComPtr<ID3DBlob> psBlob;
		if (!CompileShader(s_SimplePS, "main", "ps_4_0", psBlob))
			return false;

		hr = device->CreatePixelShader(
			psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr, &simplePS);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreatePixelShader (simple) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Volume line shaders ---
	{
		ComPtr<ID3DBlob> vsBlob;
		if (!CompileShader(s_VolumeVS, "main", "vs_4_0", vsBlob))
			return false;

		hr = device->CreateVertexShader(
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			nullptr, &volumeVS);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateVertexShader (volume) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}

		// Create input layout from the same VS blob
		D3D11_INPUT_ELEMENT_DESC volumeLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,       0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 3, DXGI_FORMAT_R32_FLOAT,       0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = device->CreateInputLayout(
			volumeLayout, _countof(volumeLayout),
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&volumeInputLayout);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateInputLayout (volume) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}

		ComPtr<ID3DBlob> psBlob;
		if (!CompileShader(s_VolumePS, "main", "ps_4_0", psBlob))
			return false;

		hr = device->CreatePixelShader(
			psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr, &volumePS);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreatePixelShader (volume) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	return true;
}

//============================================================================
// CreateStateObjects
//============================================================================

bool NavDX11Resources::CreateStateObjects()
{
	HRESULT hr;

	//--- Blend state: standard alpha blending ---
	{
		D3D11_BLEND_DESC desc = {};
		desc.RenderTarget[0].BlendEnable           = TRUE;
		desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED
		                                           | D3D11_COLOR_WRITE_ENABLE_GREEN
		                                           | D3D11_COLOR_WRITE_ENABLE_BLUE;

		hr = device->CreateBlendState(&desc, &blendAlpha);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateBlendState failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Rasterizer: no culling ---
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode        = D3D11_FILL_SOLID;
		desc.CullMode        = D3D11_CULL_NONE;
		desc.ScissorEnable   = FALSE;
		desc.DepthClipEnable = TRUE;

		hr = device->CreateRasterizerState(&desc, &rasterizerNoCull);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateRasterizerState (noCull) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Rasterizer: CW cull (navmesh) ---
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode              = D3D11_FILL_SOLID;
		desc.CullMode              = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = TRUE;   // front = CCW, so back = CW is culled
		desc.ScissorEnable         = FALSE;
		desc.DepthClipEnable       = TRUE;

		hr = device->CreateRasterizerState(&desc, &rasterizerCWCull);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateRasterizerState (cwCull) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Depth-stencil: navmesh (depth test only, no depth write, no stencil) ---
	//    We only READ the depth buffer to occlude behind walls; we must NOT write
	//    to it because that corrupts subsequent game rendering (terrain goes black).
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable    = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable  = FALSE;

		hr = device->CreateDepthStencilState(&desc, &dsNavMesh);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateDepthStencilState (navmesh) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Depth-stencil: volume line pass 0 (hidden line) ---
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable      = TRUE;
		desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc        = D3D11_COMPARISON_GREATER;
		desc.StencilEnable    = TRUE;
		desc.StencilReadMask  = 0xFF;
		desc.StencilWriteMask = 0xFF;

		desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
		desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_REPLACE;
		desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		desc.BackFace = desc.FrontFace;

		hr = device->CreateDepthStencilState(&desc, &dsVolumeLine[0]);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateDepthStencilState (vol pass 0) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Depth-stencil: volume line pass 1 (visible line) ---
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable      = TRUE;
		desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc        = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable    = TRUE;
		desc.StencilReadMask  = 0xFF;
		desc.StencilWriteMask = 0xFF;

		desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
		desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_REPLACE;
		desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		desc.BackFace = desc.FrontFace;

		hr = device->CreateDepthStencilState(&desc, &dsVolumeLine[1]);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateDepthStencilState (vol pass 1) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Depth-stencil: volume line pass 2 (hidden border) ---
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable      = TRUE;
		desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc        = D3D11_COMPARISON_GREATER;
		desc.StencilEnable    = TRUE;
		desc.StencilReadMask  = 0xFF;
		desc.StencilWriteMask = 0xFF;

		desc.FrontFace.StencilFunc        = D3D11_COMPARISON_EQUAL;
		desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_ZERO;
		desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		desc.BackFace = desc.FrontFace;

		hr = device->CreateDepthStencilState(&desc, &dsVolumeLine[2]);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateDepthStencilState (vol pass 2) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Depth-stencil: volume line pass 3 (visible border) ---
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable      = TRUE;
		desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc        = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable    = TRUE;
		desc.StencilReadMask  = 0xFF;
		desc.StencilWriteMask = 0xFF;

		desc.FrontFace.StencilFunc        = D3D11_COMPARISON_EQUAL;
		desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_ZERO;
		desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		desc.BackFace = desc.FrontFace;

		hr = device->CreateDepthStencilState(&desc, &dsVolumeLine[3]);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateDepthStencilState (vol pass 3) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	return true;
}

//============================================================================
// CreateConstantBuffers
//============================================================================

bool NavDX11Resources::CreateConstantBuffers()
{
	HRESULT hr;

	//--- Simple geometry constant buffer ---
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth      = sizeof(SimpleGeometryCB);
		desc.Usage          = D3D11_USAGE_DYNAMIC;
		desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = device->CreateBuffer(&desc, nullptr, &simpleCB);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateBuffer (simpleCB) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	//--- Volume line constant buffer ---
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth      = sizeof(VolumeLineCB);
		desc.Usage          = D3D11_USAGE_DYNAMIC;
		desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = device->CreateBuffer(&desc, nullptr, &volumeCB);
		if (FAILED(hr))
		{
			SPDLOG_ERROR("MQ2Nav: CreateBuffer (volumeCB) failed hr=0x{:08X}",
				static_cast<unsigned>(hr));
			return false;
		}
	}

	return true;
}

//============================================================================
// UpdateSimpleCB / UpdateVolumeCB
//============================================================================

void NavDX11Resources::UpdateSimpleCB(const SimpleGeometryCB& data)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = context->Map(simpleCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy(mapped.pData, &data, sizeof(data));
		context->Unmap(simpleCB.Get(), 0);
	}
}

void NavDX11Resources::UpdateVolumeCB(const VolumeLineCB& data)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = context->Map(volumeCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy(mapped.pData, &data, sizeof(data));
		context->Unmap(volumeCB.Get(), 0);
	}
}

//============================================================================
// BindSimplePipeline / BindVolumePipeline
//============================================================================

void NavDX11Resources::BindSimplePipeline()
{
	// ScopedStateBlock only saves/restores ONE render target (slot 0).
	// If EQ has MRTs bound (deferred rendering), our draws would write to all of them.
	// Force only RT[0] + DSV — unbinds slots 1-7.
	{
		ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		ID3D11DepthStencilView* dsv = nullptr;
		context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);

		static bool loggedOnce = false;
		if (!loggedOnce)
		{
			loggedOnce = true;
			int rtCount = 0;
			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
				if (rtvs[i]) rtCount++;
			WriteChatf("\ag[MQ2Nav]\ax RT diagnostic: %d render target(s) bound", rtCount);
		}

		// Re-bind only RT[0], unbinding any extras
		context->OMSetRenderTargets(1, &rtvs[0], dsv);

		// Release all refs from OMGetRenderTargets
		for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			if (rtvs[i]) rtvs[i]->Release();
		if (dsv) dsv->Release();
	}

	context->VSSetShader(simpleVS.Get(), nullptr, 0);
	context->PSSetShader(simplePS.Get(), nullptr, 0);
	context->GSSetShader(nullptr, nullptr, 0);
	// Note: Do NOT clear HS/DS — ScopedStateBlock doesn't save/restore them.
	context->IASetInputLayout(simpleInputLayout.Get());

	ID3D11Buffer* cbs[] = { simpleCB.Get() };
	context->VSSetConstantBuffers(0, 1, cbs);
	// Note: Do NOT set PS constant buffers — simple PS doesn't use them,
	// and ScopedStateBlock doesn't save/restore PS CBs.

	const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(blendAlpha.Get(), blendFactor, 0xFFFFFFFF);
	context->RSSetState(rasterizerCWCull.Get());
	context->OMSetDepthStencilState(dsNavMesh.Get(), 0);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void NavDX11Resources::BindVolumePipeline()
{
	// Force only RT[0] — same MRT safety as BindSimplePipeline
	{
		ID3D11RenderTargetView* rtv = nullptr;
		ID3D11DepthStencilView* dsv = nullptr;
		context->OMGetRenderTargets(1, &rtv, &dsv);
		context->OMSetRenderTargets(1, &rtv, dsv);
		if (rtv) rtv->Release();
		if (dsv) dsv->Release();
	}

	context->VSSetShader(volumeVS.Get(), nullptr, 0);
	context->PSSetShader(volumePS.Get(), nullptr, 0);
	context->GSSetShader(nullptr, nullptr, 0);
	// Note: Do NOT clear HS/DS — ScopedStateBlock doesn't save/restore them.
	context->IASetInputLayout(volumeInputLayout.Get());

	ID3D11Buffer* cbs[] = { volumeCB.Get() };
	context->VSSetConstantBuffers(0, 1, cbs);

	// Save the game's PS CB slot 0 before overwriting — ScopedStateBlock doesn't
	// save/restore PS constant buffers, so we must do it ourselves.
	if (m_savedPSCB) { m_savedPSCB->Release(); m_savedPSCB = nullptr; }
	context->PSGetConstantBuffers(0, 1, &m_savedPSCB);
	m_psCBDirty = true;
	context->PSSetConstantBuffers(0, 1, cbs);

	const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(blendAlpha.Get(), blendFactor, 0xFFFFFFFF);
	context->RSSetState(rasterizerNoCull.Get());

	// Note: depth-stencil state is set per-pass by the caller using dsVolumeLine[0..3]
}

void NavDX11Resources::RestoreState()
{
	// Restore PS CB slot 0 that we saved in BindVolumePipeline.
	// Only touch PS CB if we actually saved it (m_psCBDirty flag).
	if (m_psCBDirty)
	{
		context->PSSetConstantBuffers(0, 1, &m_savedPSCB);
		if (m_savedPSCB)
		{
			m_savedPSCB->Release();
			m_savedPSCB = nullptr;
		}
		m_psCBDirty = false;
	}
}
