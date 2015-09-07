//
// MQ2Nav_Render.h
//

#pragma once

#include <d3dx9.h>
#include <imgui.h>

#include <memory>
#include <vector>

class CEQDraw;
class MQ2NavigationPath;

class MQ2NavigationRender
{
	const float HEIGHT_OFFSET = 2.0;

public:
	MQ2NavigationRender();
	~MQ2NavigationRender();

	void Initialize();
	void Cleanup();

	void PerformRender();
	void PerformRenderUI();

	IDirect3DDevice9* GetDevice() const { return m_pDevice; }

	// testing
	void SetNavigationPath(MQ2NavigationPath* navPath);
	void UpdateNavigationPath();
	void ClearNavigationPath();

protected:
	void Render();
	void UpdateUI();

private:
	void InstallHooks();
	void RemoveHooks();

	bool m_hooksInstalled = false;
	std::vector<DWORD> m_installedHooks;

private:
	IDirect3DDevice9* m_pDevice = nullptr;

	DWORD* m_direct3D_vftable = 0;

	// demo purposes
	IDirect3DVertexBuffer9* m_pVertexBuffer = nullptr;
	IDirect3DTexture9* m_pTexture = nullptr;

	D3DXMATRIX m_worldMatrix;
	D3DXMATRIX m_viewMatrix;
	D3DXMATRIX m_projMatrix;

	// ui states
	bool show_test_window = true;
	bool show_another_window = false;
	ImVec4 clear_col = ImColor(114, 144, 154);

	//----------------------------------------------------------------------------
	// nav path rendering

	D3DMATERIAL9 m_navPathMaterial;

	struct LineVertex
	{
		float x, y, z;

		enum FVF
		{
			FVF_Flags = D3DFVF_XYZ
		};
	};

	int m_navpathLen = 0;
	MQ2NavigationPath* m_navPath = nullptr;

	IDirect3DVertexBuffer9* m_pLines = nullptr;
};

extern std::unique_ptr<MQ2NavigationRender> g_render;
