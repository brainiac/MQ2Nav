//
// RenderDX11.h
//
// Shared Direct3D 11 pipeline objects used by the in-game renderables.
//

#pragma once

#include <mq/Plugin.h>

#if HAS_DIRECTX_11

#include <d3d11.h>
#include <wil/com.h>
#include <glm/glm.hpp>

class RenderDX11
{
public:
	bool Initialize(ID3D11Device* device);

	ID3D11Device* GetDevice() const { return m_device.get(); }
	ID3D11DeviceContext* GetContext() const { return m_context.get(); }

	// Captures the camera for the frame. Call around all rendering for the frame.
	void BeginFrame();
	void EndFrame();

	// Binds the position+color pipeline used by RenderList.
	void BindGeometry(const glm::mat4& world, bool depthTest);

	// Binds the volume line pipeline (port of VolumeLines.fx) and clears the stencil buffer.
	void BindVolumeLines(ID3D11Buffer* vertexBuffer, UINT stride);

	// Sets the constants and depth/stencil state for one of the four volume line passes:
	// 0 = hidden line, 1 = visible line, 2 = hidden border, 3 = visible border.
	void SetVolumeLinePass(int pass, const glm::vec4& lineColor, const glm::vec4& linkColor, float lineWidth);

	// Draws one line segment (a triangle strip) in the current pass.
	void DrawVolumeLine(UINT startVertex, UINT primitiveCount) { m_context->Draw(primitiveCount + 2, startVertex); }

	void EndVolumeLines() {}

private:
	template <typename T>
	void UpdateConstants(ID3D11Buffer* buffer, const T& data);

	wil::com_ptr<ID3D11Device> m_device;
	wil::com_ptr<ID3D11DeviceContext> m_context;

	glm::mat4 m_viewProj{ 1.0f }; // glm convention: clip = viewProj * pos

	wil::com_ptr<ID3D11BlendState> m_blendAlpha;

	// Geometry pipeline
	wil::com_ptr<ID3D11VertexShader> m_geometryVS;
	wil::com_ptr<ID3D11PixelShader> m_geometryPS;
	wil::com_ptr<ID3D11InputLayout> m_geometryLayout;
	wil::com_ptr<ID3D11Buffer> m_geometryCB;
	wil::com_ptr<ID3D11RasterizerState> m_geometryRaster;
	wil::com_ptr<ID3D11DepthStencilState> m_depthTestOn;
	wil::com_ptr<ID3D11DepthStencilState> m_depthTestOff;

	// Volume line pipeline
	bool InitializeVolumeLines();

	wil::com_ptr<ID3D11VertexShader> m_volumeVS;
	wil::com_ptr<ID3D11PixelShader> m_volumePS;
	wil::com_ptr<ID3D11InputLayout> m_volumeLayout;
	wil::com_ptr<ID3D11Buffer> m_volumeCB;
	wil::com_ptr<ID3D11RasterizerState> m_volumeRaster;
	wil::com_ptr<ID3D11DepthStencilState> m_volumeDepth[4];
};

#endif // HAS_DIRECTX_11
