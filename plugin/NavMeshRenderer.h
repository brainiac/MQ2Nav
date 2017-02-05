//
// NavMeshRenderer.h
//

#pragma once

#include "Renderable.h"
#include "RenderList.h"

#include "common/NavModule.h"
#include "common/Signal.h"

#include <d3dx9.h>
#include <d3d9caps.h>
#include <cassert>
#include <thread>
#include <mutex>

class dtNavMesh;
class NavMesh;
class ConfigurableRenderState;

//----------------------------------------------------------------------------

class NavMeshRenderer : public Renderable, public NavModule
{
public:
	NavMeshRenderer();
	~NavMeshRenderer();

	virtual void Initialize() override;
	virtual void Shutdown() override;

	virtual void InvalidateDeviceObjects() override;
	virtual bool CreateDeviceObjects() override;
	virtual void Render(RenderPhase phase) override;

	void UpdateNavMesh();

	void OnUpdateUI();

private:
	void CleanupObjects();

	// stop any pending geometry load
	void StopLoad();
	void StartLoad();

private:
	IDirect3DDevice9* m_pDevice = nullptr;
	NavMesh* m_navMesh = nullptr;

	bool m_enabled = false;
	bool m_loaded = false;

	std::unique_ptr<RenderGroup> m_primGroup;
	Signal<>::ScopedConnection m_meshConn;

	std::unique_ptr<ConfigurableRenderState> m_state;
	bool m_useStateEditor = false;

	// loading progress
	bool m_loading = false;
	bool m_stopLoading = false;
	float m_progress = 0.0f;
	std::thread m_loadThread;
};

//----------------------------------------------------------------------------

class ConfigurableRenderState
{
public:
	void RenderDebugUI();
	void ApplyState(IDirect3DDevice9* device);

	ConfigurableRenderState()
	{
		for (int i = 0; i < 8; i++)
		{
			m_textureData.emplace_back(TextureStageData{ i });
		}
	}

private:
	//----------------------------------------------------------------------------
	// render settings

	DWORD m_cullMode = D3DCULL_CW; int m_cullModeIndex = 1; // D3DRS_CULLMODE
	bool m_lighting = false; // D3DRS_LIGHTING
	bool m_depthBuffer = true; // D3DRS_ZENABLE
	bool m_dephtBufferWriteable = true; // D3DRS_ZWRITEENABLE
	bool m_alphaBlend = true; // D3DRS_ALPHABLENDENABLE
	DWORD m_blendOp = D3DBLENDOP_ADD; int m_blendOpIndex = 0; // D3DRS_BLENDOP
	DWORD m_sourceBlend = D3DBLEND_SRCALPHA; int m_sourceBlendIndex = 4; // D3DRS_SRCBLEND
	DWORD m_destBlend = D3DBLEND_INVSRCALPHA; int m_destBlendIndex = 5; // D3DRS_DESTBLEND
	bool m_scissorTestEnable = true; // D3DRS_SCISSORTESTENABLE
	DWORD m_zFunc = D3DCMP_LESSEQUAL; int m_zfuncIndex = 3; // D3DRS_ZFUNC
	bool m_clipping = true; // D3DRS_CLIPPING
	bool m_alphaTestEnable = true; // D3DRS_ALPHATESTENABLE
	DWORD m_alphaFunc = D3DCMP_ALWAYS; int m_alphaFuncIndex = 7; // D3DRS_ALPHAFUNC
	int m_alphaRef = 192; // D3DRS_ALPHAREF

	bool m_stencilBuffer = false; // D3DRS_STENCILENABLE
	DWORD m_stencilFail = D3DSTENCILOP_KEEP; int m_stencilFailIndex = 0; // D3DRS_STENCILFAIL
	DWORD m_stencilZFail = D3DSTENCILOP_KEEP; int m_stencilZFailIndex = 0; // D3DRS_STENCILZFAIL
	DWORD m_stencilPass = D3DSTENCILOP_KEEP; int m_stencilPassIndex = 0; // D3DRS_STENCILPASS
	DWORD m_stencilFunc = D3DCMP_ALWAYS; int m_stencilFuncIndex = 7; // D3DRS_STENCILFUNC
	int m_stencilRef = 0; // D3DRS_STENCILREF
	DWORD m_stencilMask = 0xffffffff; // D3DRS_STENCILMASK
	DWORD m_stencilWriteMask = 0xffffffff; // D3DRS_STENCILWRITEMASK

	D3DCAPS9 m_caps;

	struct TextureStageData
	{
		DWORD m_colorOp; int m_colorOpIndex = 0; // D3DTSS_COLOROP
		DWORD m_colorArg1; int m_colorArg1Index = 6; // D3DTSS_COLORARG1
		DWORD m_colorArg2; int m_colorArg2Index = 1; // D3DTSS_COLORARG2
		DWORD m_alphaOp; int m_alphaOpIndex = 0; // D3DTSS_ALPHAOP
		DWORD m_alphaArg1; int m_alphaArg1Index = 6; // D3DTSS_ALPHAARG1
		DWORD m_alphaArg2; int m_alphaArg2Index = 0; // D3DTSS_ALPHAARG2

		DWORD m_colorConstant = 0;

		TextureStageData(int index)
		{
			if (index == 0)
			{
				m_colorOp = D3DTOP_SELECTARG1; // default is modulate
				m_colorOpIndex = 1;

				m_colorArg1 = 2;
				m_colorArg2 = 1;

				m_alphaOp = D3DTOP_MODULATE;
				m_alphaOp = 3;

				m_alphaArg1Index = 6;
				m_alphaArg2Index = 2;
			}
			else
			{
				m_colorOp = D3DTOP_DISABLE;
				m_colorOpIndex = 0;

				m_alphaOp = D3DTOP_DISABLE;
				m_alphaOp = 0;
				m_colorArg1 = 2;
				m_colorArg2 = 1;
			}
		}
	};

	std::vector<TextureStageData> m_textureData;
};
