//
// RenderDX11.h
//
// Shared Direct3D 11 pipeline objects used by the in-game renderables.
//

#pragma once

#include <mq/Plugin.h>

#if HAS_DIRECTX_11

#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>

class RenderDX11
{
public:
	bool Initialize(ID3D11Device* device);

	ID3D11Device* GetDevice() const { return m_device.Get(); }
	ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

	// Camera for the current frame (glm convention: clip = viewProj * pos).
	void SetViewProj(const glm::mat4& viewProj) { m_viewProj = viewProj; }
	const glm::mat4& GetViewProj() const { return m_viewProj; }

	// Binds the position+color pipeline used by RenderList.
	void BindGeometry(const glm::mat4& world, bool depthTest);

	// Binds the volume line pipeline (port of VolumeLines.fx) and clears the stencil buffer.
	void BindVolumeLines(ID3D11Buffer* vertexBuffer, UINT stride);

	// Sets the constants and depth/stencil state for one of the four volume line passes:
	// 0 = hidden line, 1 = visible line, 2 = hidden border, 3 = visible border.
	// Colors carry the opacity in alpha.
	void SetVolumeLinePass(int pass, const glm::vec4& lineColor, const glm::vec4& linkColor, float lineWidth);

private:
	template <typename T>
	void UpdateConstants(ID3D11Buffer* buffer, const T& data);

	Microsoft::WRL::ComPtr<ID3D11Device> m_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;

	glm::mat4 m_viewProj{ 1.0f };

	Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendAlpha;

	// Geometry pipeline
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_geometryVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_geometryPS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_geometryLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_geometryCB;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_geometryRaster;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthTestOn;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthTestOff;

	// Volume line pipeline
	bool InitializeVolumeLines();

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_volumeVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_volumePS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_volumeLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_volumeCB;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_volumeRaster;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_volumeDepth[4];
};

// Returns the active pipeline, or nullptr if the device is not acquired.
RenderDX11* GetRenderDX11();

#endif // HAS_DIRECTX_11
