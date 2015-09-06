
#include "MQ2Navigation_Render.h"
#include "MQ2Navigation.h"

#include "EQDraw.h"
#include "EQGraphics.h"
#include "imgui_impl_dx9.h"


#include <cassert>

//============================================================================

// TODO: Provide a fingerprint for pattern recognition
#define ZoneRender_InjectionOffset_x                        0x10072EB0

DWORD EQGraphicsBaseAddress = (DWORD)GetModuleHandle("EQGraphicsDX9.dll");

#define INITIALIZE_EQGRAPHICS_OFFSET(var) DWORD var = (((DWORD)var##_x - 0x10000000) + EQGraphicsBaseAddress)

INITIALIZE_EQGRAPHICS_OFFSET(ZoneRender_InjectionOffset);

void MQ2Navigation_PerformRender();
void MQ2Navigation_ReleaseResources();
void MQ2Navigation_PerformRenderUI();

class RenderHooks
{
public:
	//------------------------------------------------------------------------
	// d3d9 hooks

	// this is only valid during a d3d9 hook detour
	IDirect3DDevice9* GetDevice() { return reinterpret_cast<IDirect3DDevice9*>(this); }

	HRESULT WINAPI Reset_Trampoline(D3DPRESENT_PARAMETERS* pPresentationParameters);
	HRESULT WINAPI Reset_Detour(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		MQ2Navigation_ReleaseResources();
		return Reset_Trampoline(pPresentationParameters);
	}

	HRESULT WINAPI BeginScene_Trampoline();
	HRESULT WINAPI BeginScene_Detour()
	{
		return BeginScene_Trampoline();
	}

	HRESULT WINAPI EndScene_Trampoline();
	HRESULT WINAPI EndScene_Detour()
	{
		MQ2Navigation_PerformRenderUI();
		return EndScene_Trampoline();
	}

	//------------------------------------------------------------------------
	// EQGraphicsDX9.dll hooks
	void ZoneRender_Injection_Trampoline();
	void ZoneRender_Injection_Detour()
	{
		MQ2Navigation_PerformRender();
		ZoneRender_Injection_Trampoline();
	}
};

DETOUR_TRAMPOLINE_EMPTY(void RenderHooks::ZoneRender_Injection_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::Reset_Trampoline(D3DPRESENT_PARAMETERS* pPresentationParameters));
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::BeginScene_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::EndScene_Trampoline());

//----------------------------------------------------------------------------

std::unique_ptr<MQ2NavigationRender> g_render;

static IDirect3DDevice9* GetDeviceFromEverquest()
{
	if (CRender* pRender = (CRender*)g_pDrawHandler) {
		return  pRender->pDevice;
	}
	return nullptr;
}

MQ2NavigationRender::MQ2NavigationRender()
{
	Initialize();
}

MQ2NavigationRender::~MQ2NavigationRender()
{
	Cleanup();
	RemoveHooks();
}

void MQ2NavigationRender::InstallHooks()
{
	if (m_hooksInstalled)
		return;

#define InstallDetour(offset, detour, trampoline) \
	AddDetourf((DWORD)offset, detour, trampoline); \
	m_installedHooks.push_back((DWORD)offset);

	InstallDetour(ZoneRender_InjectionOffset, &RenderHooks::ZoneRender_Injection_Detour,
		&RenderHooks::ZoneRender_Injection_Trampoline);

	// Add hooks on IDirect3DDevice9 virtual functions
	DWORD* d3dDevice_vftable = *(DWORD**)m_pDevice;
	InstallDetour(d3dDevice_vftable[0x10], &RenderHooks::Reset_Detour, &RenderHooks::Reset_Trampoline);
	InstallDetour(d3dDevice_vftable[0x29], &RenderHooks::BeginScene_Detour, &RenderHooks::BeginScene_Trampoline);
	InstallDetour(d3dDevice_vftable[0x2A], &RenderHooks::EndScene_Detour, &RenderHooks::EndScene_Trampoline)

	m_hooksInstalled = true;
#undef InstallDetour
}

void MQ2NavigationRender::RemoveHooks()
{
	for (DWORD addr : m_installedHooks)
	{
		RemoveDetour(addr);
	}

	m_installedHooks.clear();
	m_hooksInstalled = false;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

struct Vertex
{
	float x, y, z;
	float nx, ny, nz;
	float tu, tv;

	enum FVF
	{
		FVF_Flags = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1
	};
};

Vertex g_cubeVertices[] =
{
	//     x     y     z      nx    ny    nz     tu   tv    
	// Front Face
	{ -1.0f, 1.0f,-1.0f,  0.0f, 0.0f,-1.0f,  0.0f,0.0f, },
	{ 1.0f, 1.0f,-1.0f,  0.0f, 0.0f,-1.0f,  1.0f,0.0f, },
	{ -1.0f,-1.0f,-1.0f,  0.0f, 0.0f,-1.0f,  0.0f,1.0f, },
	{ 1.0f,-1.0f,-1.0f,  0.0f, 0.0f,-1.0f,  1.0f,1.0f, },
	// Back Face
	{ -1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,0.0f, },
	{ -1.0f,-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,1.0f, },
	{ 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f,0.0f, },
	{ 1.0f,-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f,1.0f, },
	// Top Face
	{ -1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  0.0f,0.0f, },
	{ 1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,  1.0f,0.0f, },
	{ -1.0f, 1.0f,-1.0f,  0.0f, 1.0f, 0.0f,  0.0f,1.0f, },
	{ 1.0f, 1.0f,-1.0f,  0.0f, 1.0f, 0.0f,  1.0f,1.0f, },
	// Bottom Face
	{ -1.0f,-1.0f, 1.0f,  0.0f,-1.0f, 0.0f,  0.0f,1.0f, },
	{ -1.0f,-1.0f,-1.0f,  0.0f,-1.0f, 0.0f,  0.0f,0.0f, },
	{ 1.0f,-1.0f, 1.0f,  0.0f,-1.0f, 0.0f,  1.0f,1.0f, },
	{ 1.0f,-1.0f,-1.0f,  0.0f,-1.0f, 0.0f,  1.0f,0.0f, },
	// Right Face
	{ 1.0f, 1.0f,-1.0f,  1.0f, 0.0f, 0.0f,  0.0f,0.0f, },
	{ 1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  1.0f,0.0f, },
	{ 1.0f,-1.0f,-1.0f,  1.0f, 0.0f, 0.0f,  0.0f,1.0f, },
	{ 1.0f,-1.0f, 1.0f,  1.0f, 0.0f, 0.0f,  1.0f,1.0f, },
	// Left Face
	{ -1.0f, 1.0f,-1.0f, -1.0f, 0.0f, 0.0f,  1.0f,0.0f, },
	{ -1.0f,-1.0f,-1.0f, -1.0f, 0.0f, 0.0f,  1.0f,1.0f, },
	{ -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,  0.0f,0.0f, },
	{ -1.0f,-1.0f, 1.0f, -1.0f, 0.0f, 0.0f,  0.0f,1.0f, }
};



void MQ2NavigationRender::Initialize()
{
	m_pDevice = GetDeviceFromEverquest();
	if (!m_pDevice) return;
	if (m_pDevice->TestCooperativeLevel() != D3D_OK) return;

	m_pDevice->AddRef();

	InstallHooks();

	HWND eqhwnd = *(HWND*)EQADDR_HWND;
	ImGui_ImplDX9_Init(eqhwnd, m_pDevice);

	D3DXMatrixIdentity(&m_worldMatrix);
	D3DXMatrixIdentity(&m_viewMatrix);
	D3DXMatrixIdentity(&m_projMatrix);

	ZeroMemory(&m_navPathMaterial, sizeof(D3DMATERIAL9));
	m_navPathMaterial.Diffuse.r = 1.0f;
	m_navPathMaterial.Diffuse.g = 1.0f;
	m_navPathMaterial.Diffuse.b = 1.0f;
	m_navPathMaterial.Diffuse.a = 1.0f;

	UpdateNavigationPath();
}

void MQ2NavigationRender::Cleanup()
{
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}

	ClearNavigationPath();

	if (m_pDevice)
	{
		ImGui_ImplDX9_Shutdown();

		m_pDevice->Release();
		m_pDevice = nullptr;
	}
}

void MQ2NavigationRender::PerformRender()
{
	// If we don't have a device, try to acquire it
	if (!m_pDevice)
	{
		Initialize();
	}

	// If we have a device, we can attempt to render
	if (m_pDevice)
	{
		// Check to see if the current device is valid to use before
		// doing any rendering. If the device is not valid, then we
		// will release everything and try again next frame.
		HRESULT result = m_pDevice->TestCooperativeLevel();

		if (result != D3D_OK)
		{
			// device was lost
			Cleanup();
			return;
		}

		D3DPERF_BeginEvent(D3DCOLOR_XRGB(128, 128, 128), L"CustomRender");

		m_pDevice->GetTransform(D3DTS_WORLD, &m_worldMatrix);
		m_pDevice->GetTransform(D3DTS_PROJECTION, &m_projMatrix);
		m_pDevice->GetTransform(D3DTS_VIEW, &m_viewMatrix);

		IDirect3DStateBlock9* stateBlock = nullptr;

		m_pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock);

		Render();

		stateBlock->Apply();
		stateBlock->Release();

		D3DPERF_EndEvent();
	}
}

void MQ2NavigationRender::Render()
{
#if 0
	//m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	//m_pDevice->SetMaterial(&m_alphaMaterial);

	// Use material's alpha
	//m_pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	// Use alpha for transparency
	//m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	//m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	D3DXMATRIX matrix;
	D3DXMatrixIdentity(&matrix);
	//D3DXMatrixTranslation(&matrix, 200, -115, -150);
		
	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me)
	{
		// convert to radians
		D3DXMATRIX rotation;
		D3DXMatrixRotationZ(&rotation, (float)me->Heading / 256.0 * PI);

		D3DXMatrixTranslation(&matrix, me->Y, me->X, me->Z);
		matrix = rotation * matrix;
	}

	m_pDevice->SetTransform(D3DTS_WORLD, &matrix);

	m_pDevice->SetTexture(0, m_pTexture);
	m_pDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(Vertex));
	m_pDevice->SetFVF(Vertex::FVF_Flags);

	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 4, 2);
	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 8, 2);
	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 12, 2);
	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 16, 2);
	m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 20, 2);

	//m_pDevice->SetTransform(D3DTS_WORLD, &m_worldMatrix);
#endif

	UpdateNavigationPath();

	if (m_pLines)
	{
		m_pDevice->SetMaterial(&m_navPathMaterial);
		m_pDevice->SetStreamSource(0, m_pLines, 0, sizeof(LineVertex));
		m_pDevice->SetFVF(LineVertex::FVF_Flags);

		m_pDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, m_navpathLen - 1);
	}
}

static void DrawMatrix(D3DXMATRIX& matrix, const char* name)
{
	if (ImGui::CollapsingHeader(name, 0, true, true))
	{
		ImGui::Columns(4, name);
		ImGui::Separator();
		for (int row = 0; row < 4; row++)
		{
			for (int col = 0; col < 4; col++)
			{
				FLOAT f = matrix(row, col);

				char label[32];
				sprintf(label, "%.2f", f);
				ImGui::Text(label); ImGui::NextColumn();
			}
		}
		ImGui::Columns(1);
	}
}

void MQ2NavigationRender::UpdateUI()
{
	if (!m_pDevice)
		return;

	ImGui_ImplDX9_NewFrame();

	// 1. Show a simple window
	// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
	{
		//static float f = 0.0f;
		//ImGui::Text("Hello, world!");
		//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
		//ImGui::ColorEdit3("clear color", (float*)&clear_col);
		//if (ImGui::Button("Test Window")) show_test_window ^= 1;
		//if (ImGui::Button("Another Window")) show_another_window ^= 1;

		//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		PSPAWNINFO me = GetCharInfo()->pSpawn;
		if (me)
		{
			float rotation = (float)me->Heading / 256.0 * PI;
			//ImGui::Text("Position: %.2f %.2f %.2f  Heading: %.2f", me->X, me->Y, me->Z, rotation);
		}

		//DrawMatrix(m_worldMatrix, "World Matrix");
		//DrawMatrix(m_viewMatrix, "View Matrix");
		//DrawMatrix(m_projMatrix, "Projection Matrix");
	}
}

void MQ2NavigationRender::PerformRenderUI()
{
	if (m_pDevice)
	{
		IDirect3DStateBlock9* stateBlock = nullptr;

		m_pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock);

		UpdateUI();

		ImGui::Render();

		stateBlock->Apply();
		stateBlock->Release();
	}
}

void MQ2NavigationRender::SetNavigationPath(MQ2NavigationPath* navPath)
{
	ClearNavigationPath();

	m_navPath = navPath;

	UpdateNavigationPath();
}

void MQ2NavigationRender::UpdateNavigationPath()
{
	if (m_navPath == nullptr)
		return;

	int newLength = m_navPath->GetPathSize() + 1;
	const float* navPathPoints = m_navPath->GetCurrentPath();

	if (newLength != m_navpathLen)
	{
		// changed length, need to create new buffer?
		m_navpathLen = newLength;

		if (m_pLines)
		{
			m_pLines->Release();
			m_pLines = nullptr;
		}

		// nothing to render if there is no path
		if (m_navpathLen == 1)
			return;
	}

	PSPAWNINFO me = GetCharInfo()->pSpawn;

	LineVertex* vertices = new LineVertex[m_navpathLen];
	vertices[0] = { me->Y, me->X, me->Z + HEIGHT_OFFSET };
	for (int i = 0; i < m_navpathLen - 1; i++)
	{
		vertices[i + 1].x = navPathPoints[(i * 3) + 2];
		vertices[i + 1].y = navPathPoints[(i * 3) + 0];
		vertices[i + 1].z = navPathPoints[(i * 3) + 1] + HEIGHT_OFFSET;
	}

	HRESULT result = D3D_OK;

	if (m_pLines == nullptr )
	{
		result = m_pDevice->CreateVertexBuffer((m_navpathLen + 1) * sizeof(LineVertex),
			D3DUSAGE_WRITEONLY,
			Vertex::FVF_Flags,
			D3DPOOL_DEFAULT,
			&m_pLines,
			NULL);
	}

	if (result == D3D_OK)
	{
		void* pVertices = NULL;

		// lock v_buffer and load the vertices into it
		m_pLines->Lock(0, m_navpathLen * sizeof(LineVertex), (void**)&pVertices, 0);
		memcpy(pVertices, vertices, m_navpathLen * sizeof(LineVertex));
		m_pLines->Unlock();
	}

	delete[] vertices;
}

void MQ2NavigationRender::ClearNavigationPath()
{
	m_navpathLen = 0;
	m_navPath = nullptr;

	if (m_pLines)
	{
		m_pLines->Release();
		m_pLines = 0;
	}
}

void MQ2Navigation_PerformRender()
{
	D3DPERF_BeginEvent(D3DCOLOR_XRGB(128, 128, 128), L"CustomRender");
	g_render->PerformRender();
	D3DPERF_EndEvent();
}

void MQ2Navigation_PerformRenderUI()
{
	D3DPERF_BeginEvent(D3DCOLOR_XRGB(128, 128, 128), L"CustomRenderUI");
	g_render->PerformRenderUI();
	D3DPERF_EndEvent();
}

void MQ2Navigation_ReleaseResources()
{
	g_render->Cleanup();
}
