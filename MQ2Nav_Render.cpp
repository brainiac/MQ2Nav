//
// MQ2Nav_Render.cpp
//

#include "MQ2Nav_Render.h"
#include "MQ2Nav_Input.h"
#include "MQ2Navigation.h"

#include "EQDraw.h"
#include "EQGraphics.h"
#include "FindPattern.h"
#include "imgui_impl_dx9.h"

#include <cassert>

//============================================================================

void MQ2Navigation_PerformRender();
void MQ2Navigation_ReleaseResources();
void MQ2Navigation_PerformRenderUI();

//============================================================================

// This doesn't change during the execution of the program. This can be loaded
// at static initialization time because of this.
DWORD EQGraphicsBaseAddress = (DWORD)GetModuleHandle("EQGraphicsDX9.dll");

// This macro is used for statically building offsets. If using dynamic offset generation
// with the pattern matching, don't use the macro.
#define INITIALIZE_EQGRAPHICS_OFFSET(var) DWORD var = (((DWORD)var##_x - 0x10000000) + EQGraphicsBaseAddress)

// RenderFlames_sub_10072EB0 in EQGraphicsDX9.dll ~ 2015-08-20
#define ZoneRender_InjectionOffset_x                        0x10072EB0
INITIALIZE_EQGRAPHICS_OFFSET(ZoneRender_InjectionOffset);

const char* ZoneRender_InjectionMask = "xxxxxxxxxxxx????xxx?xxxxxxx????xxx?xxxxxxxxxx????xxx?xxxxxxx????xxx?xxxxx";
const unsigned char* ZoneRender_InjectionPattern = (const unsigned char*)"\x56\x8b\xf1\x57\x8d\x46\x14\x50\x83\xcf\xff\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x4e\x5c\x51\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x96\x80\x00\x00\x00\x52\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x46\x38\x50\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x5f\x33\xc0\x5e\xc3";
static inline DWORD FixEQGraphicsOffset(DWORD nOffset)
{
	return (nOffset - 0x10000000) + EQGraphicsBaseAddress;
}

bool GetOffsets()
{
	if ((ZoneRender_InjectionOffset = FindPattern(FixEQGraphicsOffset(0x10000000), 0x100000,
		ZoneRender_InjectionPattern, ZoneRender_InjectionMask)) == 0)
		return false;

	return true;
}

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

std::shared_ptr<MQ2NavigationRender> g_render;

static IDirect3DDevice9* GetDeviceFromEverquest()
{
	if (CRender* pRender = (CRender*)g_pDrawHandler) {
		return  pRender->pDevice;
	}
	return nullptr;
}

MQ2NavigationRender::MQ2NavigationRender()
{
	// This is so that we don't lose the dll before we've unloaded detours
	m_dx9Module = LoadLibraryA("EQGraphicsDX9.dll");

	Initialize();
}

MQ2NavigationRender::~MQ2NavigationRender()
{
	Cleanup();
	RemoveHooks();

	FreeLibrary(m_dx9Module);
}

void MQ2NavigationRender::InstallHooks()
{
	if (m_hooksInstalled)
		return;

	// make sure that the offsets are up to date before we try to make our hooks
	if (!GetOffsets())
	{
		WriteChatf(PLUGIN_MSG "\arRendering support failed! We won't be able to draw to the 3D world.");
		return;
	}

	InstallInputHooks();

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

	RemoveInputHooks();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// These are defined in imgui
unsigned int GetDroidSansCompressedSize();
const unsigned int* GetDroidSansCompressedData();

void MQ2NavigationRender::Initialize()
{
	m_pDevice = GetDeviceFromEverquest();
	if (!m_pDevice) return;
	if (m_pDevice->TestCooperativeLevel() != D3D_OK) return;

	m_pDevice->AddRef();

	InstallHooks();

	HWND eqhwnd = *(HWND*)EQADDR_HWND;
	ImGui_ImplDX9_Init(eqhwnd, m_pDevice);

	ImGuiIO& io = ImGui::GetIO();
	//io.Fonts->AddFontFromMemoryCompressedTTF(GetDroidSansCompressedData(),
	//	GetDroidSansCompressedSize(), 16);

	m_prevHistoryPoint = std::chrono::system_clock::now();
	m_renderFrameRateHistory.clear();

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

	auto now = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_prevHistoryPoint).count() >= 100)
	{
		if (m_renderFrameRateHistory.size() > 1200) {
			m_renderFrameRateHistory.erase(m_renderFrameRateHistory.begin(), m_renderFrameRateHistory.begin() + 200);
		}
		m_renderFrameRateHistory.push_back(ImGui::GetIO().Framerate);
		m_prevHistoryPoint = now;
	}

	// 1. Show a simple window
	// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiSetCond_Once);
	ImGui::Begin("Debug");
	{
		if (m_renderFrameRateHistory.size() > 0)
		{
			ImGui::PlotLines("", &m_renderFrameRateHistory[0], (int)m_renderFrameRateHistory.size(), 0, nullptr, 0.0);
		}

		ImGui::LabelText("FPS", "%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		PSPAWNINFO me = GetCharInfo()->pSpawn;
		if (me)
		{
			float rotation = (float)me->Heading / 256.0 * PI;
			ImGui::LabelText("Position", "%.2f %.2f %.2f", me->X, me->Y, me->Z);
			ImGui::LabelText("Heading", "%.2f", rotation);
		}

		RenderInputUI();

		//DrawMatrix(m_worldMatrix, "World Matrix");
		//DrawMatrix(m_viewMatrix, "View Matrix");
		//DrawMatrix(m_projMatrix, "Projection Matrix");
	}
	ImGui::End();

	ImGui::Render();
}

void MQ2NavigationRender::PerformRenderUI()
{
	if (m_pDevice)
	{
		IDirect3DStateBlock9* stateBlock = nullptr;

		m_pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock);

		UpdateUI();

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
			LineVertex::FVF_Flags,
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
	//D3DPERF_BeginEvent(D3DCOLOR_XRGB(128, 128, 128), L"CustomRenderUI");
	g_render->PerformRenderUI();
	//D3DPERF_EndEvent();
}

void MQ2Navigation_ReleaseResources()
{
	g_render->Cleanup();
}
