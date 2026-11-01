//
// RenderDX9.h
//
// Shared Direct3D 9 pipeline objects used by the in-game renderables.
// Mirrors the interface of RenderDX11.
//

#pragma once

#include <mq/Plugin.h>

#if HAS_DIRECTX_9

#include <d3d9.h>
#include <dxsdk-d3dx/d3dx9.h>
#include <wil/com.h>
#include <glm/glm.hpp>

class RenderDX9
{
public:
	bool Initialize(IDirect3DDevice9* device);

	IDirect3DDevice9* GetDevice() const { return m_device.get(); }

	// Resets the device state we depend on. Call around all rendering for the frame.
	void BeginFrame();
	void EndFrame();

	// Binds the position+color pipeline used by RenderList.
	void BindGeometry(const glm::mat4& world, bool depthTest);

	// Binds the volume line effect (VolumeLines.fx) and clears the stencil buffer.
	void BindVolumeLines(IDirect3DVertexBuffer9* vertexBuffer, UINT stride);

	// Selects one of the four volume line passes:
	// 0 = hidden line, 1 = visible line, 2 = hidden border, 3 = visible border.
	void SetVolumeLinePass(int pass, const glm::vec4& lineColor, const glm::vec4& linkColor, float lineWidth);

	// Draws one line segment (a triangle strip) in the current pass.
	void DrawVolumeLine(UINT startVertex, UINT primitiveCount);

	void EndVolumeLines();

private:
	wil::com_ptr<IDirect3DDevice9> m_device;

	// Volume lines
	wil::com_ptr<ID3DXEffect> m_volumeEffect;
	wil::com_ptr<IDirect3DVertexDeclaration9> m_volumeDeclaration;
	bool m_inVolumePass = false;
	DWORD m_savedZEnable = 0;
	DWORD m_savedZFunc = 0;
};

#endif // HAS_DIRECTX_9
