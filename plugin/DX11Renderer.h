//
// DX11Renderer.h
//
// Centralized DX11 rendering resources for MQ2Nav.
// Replaces the DX9 fixed-function pipeline and D3DXEffect-based rendering
// that was disabled when EQ moved to DX11.
//

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <glm/glm.hpp>

#pragma comment(lib, "d3dcompiler")

using Microsoft::WRL::ComPtr;

//----------------------------------------------------------------------------
// Constant buffer structures matching our HLSL shaders
//----------------------------------------------------------------------------

struct SimpleGeometryCB
{
	glm::mat4 WorldViewProj;
	glm::mat4 World;
};

struct VolumeLineCB
{
	glm::mat4 WVP;
	glm::mat4 WV;
	glm::vec4 lineColor;
	glm::vec4 linkColor;
	float     opacity;
	float     lineWidth;
	float     _pad[2];
};

//----------------------------------------------------------------------------
// NavDX11Resources — shared rendering infrastructure
//----------------------------------------------------------------------------

struct NavDX11Resources
{
	// Device pointers (not owned — these belong to MQ core)
	ID3D11Device*        device = nullptr;
	ID3D11DeviceContext*  context = nullptr;

	//--- Simple geometry pipeline (navmesh wireframe) ---
	ComPtr<ID3D11VertexShader>   simpleVS;
	ComPtr<ID3D11PixelShader>    simplePS;
	ComPtr<ID3D11InputLayout>    simpleInputLayout;
	ComPtr<ID3D11Buffer>         simpleCB;  // SimpleGeometryCB

	//--- Volume line pipeline (navigation path) ---
	ComPtr<ID3D11VertexShader>   volumeVS;
	ComPtr<ID3D11PixelShader>    volumePS;
	ComPtr<ID3D11InputLayout>    volumeInputLayout;
	ComPtr<ID3D11Buffer>         volumeCB;  // VolumeLineCB

	//--- Shared state objects ---
	ComPtr<ID3D11BlendState>          blendAlpha;       // SrcAlpha / InvSrcAlpha
	ComPtr<ID3D11RasterizerState>     rasterizerNoCull;  // Solid, no culling
	ComPtr<ID3D11RasterizerState>     rasterizerCWCull;  // Solid, CW cull (navmesh)

	// Depth-stencil for navmesh rendering (depth test, depth write, no stencil)
	ComPtr<ID3D11DepthStencilState>   dsNavMesh;

	// Depth-stencil states for the 4-pass volume line technique:
	//   Pass 0: hidden line   — ZFunc=Greater,  Stencil=Always/Replace, Ref=1
	//   Pass 1: visible line  — ZFunc=LessEqual, Stencil=Always/Replace, Ref=1
	//   Pass 2: hidden border — ZFunc=Greater,  Stencil=Equal/Zero,    Ref=0
	//   Pass 3: visible border— ZFunc=LessEqual, Stencil=Equal/Zero,    Ref=0
	ComPtr<ID3D11DepthStencilState>   dsVolumeLine[4];

	// Lifecycle
	bool Initialize(ID3D11Device* dev);
	void Shutdown();

	// Helpers
	void UpdateSimpleCB(const SimpleGeometryCB& data);
	void UpdateVolumeCB(const VolumeLineCB& data);

	void BindSimplePipeline();
	void BindVolumePipeline();
	void RestoreState();    // Restore state not covered by ScopedStateBlock

private:
	bool CompileShaders();
	bool CreateStateObjects();
	bool CreateConstantBuffers();

	// Saved state for manual restore (ScopedStateBlock doesn't cover PS CBs)
	ID3D11Buffer* m_savedPSCB = nullptr;
	bool m_psCBDirty = false;
};
