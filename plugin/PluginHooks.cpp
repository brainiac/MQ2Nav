//
// PluginHooks.cpp
//

#include "PluginHooks.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/ImGuiRenderer.h"
#include "plugin/RenderHandler.h"
#include "common/FindPattern.h"

#ifdef DIRECTINPUT_VERSION
#undef DIRECTINPUT_VERSION
#endif
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <Shlobj.h>
#include <Psapi.h>
#include <imgui.h>

#include <atomic>
#include <thread>
#include <boost/algorithm/string.hpp>
#include <winternl.h>
#include <tchar.h>

//============================================================================

// This doesn't change during the execution of the program. This can be loaded
// at static initialization time because of this.
DWORD EQGraphicsBaseAddress = (DWORD)GetModuleHandle("EQGraphicsDX9.dll");

// This macro is used for statically building offsets. If using dynamic offset generation
// with the pattern matching, don't use the macro.
#define INITIALIZE_EQGRAPHICS_OFFSET(var) DWORD var = (((DWORD)var##_x - 0x10000000) + EQGraphicsBaseAddress)

static inline DWORD FixEQGraphicsOffset(DWORD nOffset)
{
	return (nOffset - 0x10000000) + EQGraphicsBaseAddress;
}

// Copied from MQ2Globals.cpp
#define INITIALIZE_EQGAME_OFFSET(var) DWORD var = (((DWORD)var##_x - 0x400000) + baseAddress)

//----------------------------------------------------------------------------
// EQGraphicsDX9.dll offsets

// RenderFlames_sub_10072EB0 in EQGraphicsDX9.dll ~ 2015-08-20
#define ZoneRender_InjectionOffset_x                        0x10072EB0
INITIALIZE_EQGRAPHICS_OFFSET(ZoneRender_InjectionOffset);

// you can find this one near one of the D3DPERF_EndEvent Calls:
//
// .text:10018E38                 call    sub_10071560 <--
// .text:10018E3D                 mov     eax, dword_101B1E5C
// .text:10018E42                 mov     dword ptr [eax+0BCh], 0FFFFFFFFh
// .text:10018E4C                 mov     ecx, [edi+8]
// .text:10018E4F                 call    sub_10023A60
// .text:10018E54                 call    D3DPERF_EndEvent

#if defined(ROF2EMU) || defined(UFEMU)
// 0x10072110 rof2
// Sig: 56 8B F1 57 8D 46 14 50 83 CF FF E8 ? ? ? ? 85 C0 78 35 8D 4E 5C 51 8B CE E8 ? ? ? ?
// Sig: \x56\x8B\xF1\x57\x8D\x46\x14\x50\x83\xCF\xFF\xE8\x00\x00\x00\x00\x85\xC0\x78\x35\x8D\x4E\x5C\x51\x8B\xCE\xE8\x00\x00\x00\x00
//
const char* ZoneRender_InjectionMask = "xxxxxxxxxxxx????xxxxxxxxxxx????";
const unsigned char* ZoneRender_InjectionPattern = (const unsigned char*)"\x56\x8B\xF1\x57\x8D\x46\x14\x50\x83\xCF\xFF\xE8\x00\x00\x00\x00\x85\xC0\x78\x35\x8D\x4E\x5C\x51\x8B\xCE\xE8\x00\x00\x00\x00";
#else
//56 8B F1 8D 46 14 50 E8 ? ? ? ? 85 C0 78 38 8D 46 5C 8B CE 50 E8 ? ? ? ? 85 C0 78 29 8D 86 ? ? ? ? 8B CE 50 E8 ? ? ? ? 85 C0 78 17 8D 46 38 8B CE 50 E8 ? ? ? ? 33 C9 85 C0 5E 0F 99 C1 8D 41 FF C3
const char* ZoneRender_InjectionMask = "xxxxxxxx????xxxxxxxxxxx????xxxxxx????xxxx????xxxxxxxxxxx????xxxxxxxxxxxx";
const unsigned char* ZoneRender_InjectionPattern = (const unsigned char*)"\x56\x8B\xF1\x8D\x46\x14\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x38\x8D\x46\x5C\x8B\xCE\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x29\x8D\x86\x00\x00\x00\x00\x8B\xCE\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x17\x8D\x46\x38\x8B\xCE\x50\xE8\x00\x00\x00\x00\x33\xC9\x85\xC0\x5E\x0F\x99\xC1\x8D\x41\xFF\xC3";
#endif

//----------------------------------------------------------------------------
// eqgame.exe offsets

INITIALIZE_EQGAME_OFFSET(__ProcessMouseEvent);
// Calls __FlushDxKeyboard, GetAsyncKeyState, and GetForegroundWindow multiple times. Look for function that
// calls GetAsyncKeyState a bunch, with GetforegroundWindow and __FlushDxKeyboard calls in the first block.

DWORD __ProcessKeyboardEvent = 0;
#if defined(ROF2EMU) || defined(UFEMU)
// Sig: 81 EC ? ? ? ? C7 44 24 ? ? ? ? ?
const char* ProcessKeyboardEvent_Mask = "xx????xxx?????";
const unsigned char* ProcessKeyboardEvent_Pattern = (const unsigned char*)"\x81\xEC\x00\x00\x00\x00\xC7\x44\x24\x00\x00\x00\x00\x00";

// 0x643e16 - 2018-02-18
//const char* ProcessKeyboardEvent_Mask = "xx????xxxxxxxxxx????xxx?xx????x?x????xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxx?x????";
//const unsigned char* ProcessKeyboardEvent_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xc7\x44\x24\x04\x20\x00\x00\x00\xff\x15\x00\x00\x00\x00\x85\xc0\x74\x00\x3b\x05\x00\x00\x00\x00\x75\x00\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x1c\x50\xff\xd2\xa1\x00\x00\x00\x00\x8b\x08\x6a\x00\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\x6a\x14\x50\x8b\x41\x28\xff\xd0\x85\xc0\x74\x00\xe8\x00\x00\x00\x00";
#else
// Sig: 81 EC ? ? ? ? C7 44 24 ? ? ? ? ?
const char* ProcessKeyboardEvent_Mask = "xx????xxx?????";
const unsigned char* ProcessKeyboardEvent_Pattern = (const unsigned char*)"\x81\xEC\x00\x00\x00\x00\xC7\x44\x24\x00\x00\x00\x00\x00";
#endif

DWORD __FlushDxKeyboard = 0;

// Called from inside WndProc, twice with the same pattern of code:
//
// .text:0074191E loc_74191E:                             ; CODE XREF: WndProc+166↑j
// .text:0074191E                 call    __FlushDxKeyboard <--
// .text:00741923                 pop     edi
// .text:00741924                 pop     esi
// .text:00741925                 mov     dword_C668D8, ebp
// .text:0074192B                 mov     eax, ebp
// .text:0074192D                 pop     ebp
// .text:0074192E                 pop     ebx
// .text:0074192F                 mov     ecx, [esp+34h+var_C]
// .text:00741933                 mov     large fs:0, ecx
// .text:0074193A                 add     esp, 34h
// .text:0074193D                 retn    10h

#if defined(ROF2EMU) || defined(UFEMU)
// 0x640f00 - 2018-02-18
// 0x81 0xEC ? ? ? ? 0xA1 ? ? ? ? 0x53 0x33 0xDB 0x53 0x8D 0x54 0x24 0x08 0x52 0x8D 0x54 0x24 0x10 0x52 0xC7 0x44 0x24 0x10 0x20 0x00 0x00 0x00 0x8B 0x08 0x6A 0x14 0x50 0x8B 0x41 0x28 0xFF 0xD0 0x8B 0x0D ? ? ? ? 0x89 0x1D ? ? ? ? 0x89 0x1D ? ? ? ? 0x88 0x1D ? ? ? ? 0x3B 0xCB 0x5B 0x74 ? 0xE8 ? ? ? ? 0x8B 0x04 0x24 0x81 0xC4 ? ? ? ? 0xC3
const char* FlushDxKeyboard_Mask = "xx????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx????xx????xx????xxxx?x????xxxxx????x";
const unsigned char* FlushDxKeyboard_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x53\x33\xdb\x53\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\xc7\x44\x24\x10\x20\x00\x00\x00\x8b\x08\x6a\x14\x50\x8b\x41\x28\xff\xd0\x8b\x0d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x88\x1d\x00\x00\x00\x00\x3b\xcb\x5b\x74\x00\xe8\x00\x00\x00\x00\x8b\x04\x24\x81\xc4\x00\x00\x00\x00\xc3";
#else
// 0x73e9b0 - 2018-02-16
const char* FlushDxKeyboard_Mask = "xx????x????xxxxxxxxxxxxx?????xxxxxxxxx";
const unsigned char* FlushDxKeyboard_Pattern = (const unsigned char*)"\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x8D\x14\x24\x6A\x00\x52\x8D\x54\x24\x0C\xC7\x44\x24\x00\x00\x00\x00\x00\x8B\x08\x52\x6A\x14\x50\xFF\x51\x28";
#endif

FUNCTION_AT_ADDRESS(int FlushDxKeyboard(), __FlushDxKeyboard);

// Don't need a signature for this, can get it programmatically
// Unless we're attached to a session of innerspace, then the wndproc is all fubar.
DWORD __WndProc = 0;

// Look for construction of WndClass
//
// .text:0073FC95                 push    6Bh             ; lpIconName
// .text:0073FC97                 push    ebp             ; hInstance
// .text:0073FC98                 mov     [esp+488h+WndClass.hCursor], 0Bh
// .text:0073FCA0                 mov     [esp+488h+WndClass.hbrBackground], offset WndProc <--
// .text:0073FCA8                 mov     [esp+488h+WndClass.lpszMenuName], 0
// .text:0073FCB0                 mov     [esp+488h+WndClass.lpszClassName], 0
// .text:0073FCB8                 mov     [esp+488h+var_41C], ebp
// .text:0073FCBC                 call    ds:LoadIconA

#if defined(ROF2EMU) || defined(UFEMU)
// Sig: 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 40 8B 0D ? ? ? ? 55 8B 6C 24 5C 56 8B 74 24 5C 57
const char* WndProc_Mask = "xxx????xx????xxxx????xxxxx????xxxxxxxxxxx";
const unsigned char* WndProc_Pattern = (const unsigned char*)"\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x40\x8B\x0D\x00\x00\x00\x00\x55\x8B\x6C\x24\x5C\x56\x8B\x74\x24\x5C\x57";
// 0x6431f0 - 2018-02-18
//const char* WndProc_Mask = "xxx????xx????xxxx????xxxxx????xxxxxxxxxxxxxxxxxxxxxxxxx?x????xxx?xx????xxxx????xxxx????xx????";
//const unsigned char* WndProc_Pattern = (const unsigned char*)"\x6a\xff\x68\x00\x00\x00\x00\x64\xa1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xec\x2c\x8b\x0d\x00\x00\x00\x00\x53\x55\x8b\x6c\x24\x4c\x56\x8b\x74\x24\x54\x33\xdb\x57\x8b\x7c\x24\x50\x89\x5c\x24\x10\x3b\xcb\x74\x00\xe8\x00\x00\x00\x00\x84\xc0\x74\x00\x8b\x0d\x00\x00\x00\x00\x56\x55\x57\xe8\x00\x00\x00\x00\x85\xc0\x0f\x84\x00\x00\x00\x00\x39\x1d\x00\x00\x00\x00";
#else
// apr 10 2018 test eqgame
// 55 8B EC 64 A1 ? ? ? ? 6A FF 68 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 1C 8B 0D ? ? ? ?
const char* WndProc_Mask = "xxxxx????xxx????xxxx????xxxxx????";
const unsigned char* WndProc_Pattern = (const unsigned char*)"\x55\x8B\xEC\x64\xA1\x00\x00\x00\x00\x6A\xFF\x68\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x1C\x8B\x0D\x00\x00\x00\x00";
#endif

//----------------------------------------------------------------------------

bool GetOffsets()
{
	//----------------------------------------------------------------------------
	// EQGraphicsDX9.dll Offset

	// Find the EQGraphicsDX9.dll image size, and use it to calculate the upper bound of
	// each signature search range.

	MODULEINFO EQGraphicsModInfo = { 0 };
	GetModuleInformation(GetCurrentProcess(), (HMODULE)EQGraphicsBaseAddress, &EQGraphicsModInfo, sizeof(MODULEINFO));
	int EQGraphicsUpperBound = EQGraphicsBaseAddress + EQGraphicsModInfo.SizeOfImage;

	if ((ZoneRender_InjectionOffset = FindPattern(FixEQGraphicsOffset(0x10000000), 0x500000, EQGraphicsUpperBound,
		ZoneRender_InjectionPattern, ZoneRender_InjectionMask)) == 0)
		return false;

	//----------------------------------------------------------------------------
	// EQGame.exe Offsets

	MODULEINFO EQGameModInfo = { 0 };
	GetModuleInformation(GetCurrentProcess(), (HMODULE)baseAddress, &EQGameModInfo, sizeof(MODULEINFO));
	int EQGameUpperBound = baseAddress + EQGameModInfo.SizeOfImage;

	if ((__ProcessKeyboardEvent = FindPattern(FixOffset(0x550000), 0x600000, EQGameUpperBound,
		ProcessKeyboardEvent_Pattern, ProcessKeyboardEvent_Mask)) == 0)
		return false;

	if ((__FlushDxKeyboard = FindPattern(FixOffset(0x550000), 0x600000, EQGameUpperBound,
		FlushDxKeyboard_Pattern, FlushDxKeyboard_Mask)) == 0)
		return false;

	if ((__WndProc = FindPattern(FixOffset(0x550000), 0x600000, EQGameUpperBound,
		WndProc_Pattern, WndProc_Mask)) == 0)
		return false;

	return true;
}

//----------------------------------------------------------------------------

// have detours been installed already?
bool g_hooksInstalled = false;

// the handle to the graphics dll
HMODULE g_dx9Module = 0;

struct HookInfo
{
	std::string name;
	DWORD address = 0;

	std::function<void(HookInfo&)> patch = nullptr;
};

// list of installed hooks at their address, if they are already patched in
std::vector<HookInfo> g_hooks;

void InstallHook(HookInfo hi)
{
	auto iter = std::find_if(std::begin(g_hooks), std::end(g_hooks),
		[&hi](const HookInfo& hookInfo)
	{
		return hi.name == hookInfo.name;
	});

	if (iter != std::end(g_hooks))
	{
		// hook already installed. Remove it.
		if (iter->address != 0)
		{
			RemoveDetour(iter->address);
		}
		g_hooks.erase(iter);
	}

	hi.patch(hi);
	g_hooks.push_back(hi);
}

template <typename T>
void InstallDetour(DWORD address, const T& detour, const T& trampoline, PCHAR name)
{
	HookInfo hookInfo;
	hookInfo.name = name;
	hookInfo.address = 0;
	hookInfo.patch = [&detour, &trampoline, address](HookInfo& hi)
	{
		hi.address = address;
		AddDetourf(hi.address, detour, trampoline, hi.name.c_str());
	};

	InstallHook(hookInfo);
}

// the global direct3d device that we are "borrowing"
IDirect3DDevice9* g_pDevice = nullptr;

// represents whether the device has been acquired and is good to use.
bool g_deviceAcquired = false;

HMODULE g_d3d9Module = 0;
using D3D9CREATEEXPROC = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);

// Address of the Reset() function
DWORD g_resetDeviceAddress = 0;

// check if we are in the keyboard event handler during shutdown
std::atomic_bool g_inKeyboardEventHandler = false;

class RenderHooks
{
public:
	//------------------------------------------------------------------------
	// d3d9 hooks

	// this is only valid during a d3d9 hook detour
	IDirect3DDevice9* GetThisDevice() { return reinterpret_cast<IDirect3DDevice9*>(this); }

	// Install hooks on actual instance of the device once we have it.
	bool DetectResetDeviceHook()
	{
		bool changed = false;

		// IDirect3DDevice9 virtual function hooks
		DWORD* d3dDevice_vftable = *(DWORD**)this;

		DWORD resetDevice = d3dDevice_vftable[0x10];

		if (resetDevice != g_resetDeviceAddress)
		{
			if (g_resetDeviceAddress != 0)
			{
				WriteChatf(PLUGIN_MSG "\arDetected a change in the rendering device. Attempting to recover. Please check for compat flags...");
			}
			g_resetDeviceAddress = resetDevice;

			InstallDetour(d3dDevice_vftable[0x10],
				&RenderHooks::Reset_Detour,
				&RenderHooks::Reset_Trampoline,
				"d3dDevice_Reset");

			changed = true;
		}

		return changed;
	}

	HRESULT WINAPI Reset_Trampoline(D3DPRESENT_PARAMETERS* pPresentationParameters);
	HRESULT WINAPI Reset_Detour(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		if (g_pDevice != GetThisDevice())
		{
			return Reset_Trampoline(pPresentationParameters);
		}

		g_deviceAcquired = false;

		if (g_renderHandler)
		{
			g_renderHandler->InvalidateDeviceObjects();
		}

		return Reset_Trampoline(pPresentationParameters);
	}

	HRESULT WINAPI BeginScene_Trampoline();
	HRESULT WINAPI BeginScene_Detour()
	{
		g_pDevice = GetThisDevice();

		return BeginScene_Trampoline();
	}

	HRESULT WINAPI EndScene_Trampoline();
	HRESULT WINAPI EndScene_Detour()
	{
		if (GetThisDevice() != g_pDevice)
		{
			return EndScene_Trampoline();
		}

		// When TestCooperativeLevel returns all good, then we can reinitialize.
		// This will let the renderer control our flow instead of having to
		// poll for the state ourselves.
		if (!g_deviceAcquired)
		{
			HRESULT result = GetThisDevice()->TestCooperativeLevel();

			if (result == D3D_OK)
			{
				g_deviceAcquired = true;

				if (DetectResetDeviceHook())
				{
					if (g_renderHandler)
					{
						g_renderHandler->InvalidateDeviceObjects();
					}
				}

				if (g_renderHandler)
				{
					g_renderHandler->CreateDeviceObjects();
				}
			}
		}

		// Perform the render within a stateblock so we don't upset the
		// rest of the rendering pipeline
		if (g_deviceAcquired)
		{
			IDirect3DStateBlock9* stateBlock = nullptr;
			g_pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock);

			if (g_renderHandler)
			{
				g_renderHandler->PerformRender(Renderable::Render_UI);
			}

			stateBlock->Apply();
			stateBlock->Release();
		}

		return EndScene_Trampoline();
	}

	//------------------------------------------------------------------------
	// EQGraphicsDX9.dll hooks
	void ZoneRender_Injection_Trampoline();
	void ZoneRender_Injection_Detour()
	{
		// Perform the render within a stateblock so we don't upset the
		// rest of the rendering pipeline
		if (g_deviceAcquired)
		{
			IDirect3DStateBlock9* stateBlock = nullptr;
			g_pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock);

			if (g_renderHandler)
			{
				g_renderHandler->PerformRender(Renderable::Render_Geometry);
			}

			stateBlock->Apply();
			stateBlock->Release();
		}

		ZoneRender_Injection_Trampoline();
	}
};

DETOUR_TRAMPOLINE_EMPTY(void RenderHooks::ZoneRender_Injection_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::Reset_Trampoline(D3DPRESENT_PARAMETERS* pPresentationParameters));
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::BeginScene_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::EndScene_Trampoline());

bool InstallD3D9Hooks()
{
	bool success = false;

	WCHAR lpD3D9Path[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, lpD3D9Path);
	wcscat_s(lpD3D9Path, MAX_PATH, L"\\d3d9.dll");

	if (g_d3d9Module = LoadLibraryW(lpD3D9Path))
	{
		D3D9CREATEEXPROC d3d9CreateEx = (D3D9CREATEEXPROC)GetProcAddress(g_d3d9Module, "Direct3DCreate9Ex");
		if (d3d9CreateEx)
		{
			HRESULT hRes;
			IDirect3D9Ex* d3d9ex;

			if (SUCCEEDED(hRes = (*d3d9CreateEx)(D3D_SDK_VERSION, &d3d9ex)))
			{
				D3DPRESENT_PARAMETERS pp;
				ZeroMemory(&pp, sizeof(pp));
				pp.Windowed = 1;
				pp.SwapEffect = D3DSWAPEFFECT_FLIP;
				pp.BackBufferFormat = D3DFMT_A8R8G8B8;
				pp.BackBufferCount = 1;
				pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

				IDirect3DDevice9Ex* deviceEx;
				if (SUCCEEDED(hRes = d3d9ex->CreateDeviceEx(
					D3DADAPTER_DEFAULT,
					D3DDEVTYPE_NULLREF,
					0,
					D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES,
					&pp, NULL, &deviceEx)))
				{
					success = true;

					// IDirect3DDevice9 virtual function hooks
					DWORD* d3dDevice_vftable = *(DWORD**)deviceEx;

					InstallDetour(d3dDevice_vftable[0x29],
						&RenderHooks::BeginScene_Detour,
						&RenderHooks::BeginScene_Trampoline,
						"d3dDevice_BeginScene");
					InstallDetour(d3dDevice_vftable[0x2a],
						&RenderHooks::EndScene_Detour,
						&RenderHooks::EndScene_Trampoline,
						"d3dDevice_EndScene");

					deviceEx->Release();
				}
				else
				{
					DebugSpewAlways("InstallD3D9Hooks: failed to CreateDeviceEx. Result=%x", hRes);
				}

				d3d9ex->Release();
			}
			else
			{
				DebugSpewAlways("InstallD3D9Hooks: failed Direct3DCreate9Ex failed. Result=%x", hRes);
			}
		}
		else
		{
			DebugSpewAlways("InitD3D9CApture: failed to get address of Direct3DCreate9Ex");
		}
	}

	return success;
}


//----------------------------------------------------------------------------
// mouse / keyboard handling

POINT MouseLocation;
bool MouseBlocked = false; // blocked if EQ is dragging mouse

MouseStateData* MouseState = (MouseStateData*)EQADDR_DIMOUSECOPY;
DIMOUSESTATE* DIMouseState = (DIMOUSESTATE*)EQADDR_DIMOUSECHECK;
MOUSEINFO2* MouseInfo = (MOUSEINFO2*)EQADDR_MOUSE;

#define g_pMouse ((IDirectInputDevice8A*)(*EQADDR_DIMOUSE))

bool GetMouseLocation(POINT& point)
{
	// This might mess up if we're in fullscreen mode.
	HWND eqHWnd = *(HWND*)EQADDR_HWND;

	GetCursorPos(&point);
	ScreenToClient(eqHWnd, &point);

	RECT r = { 0, 0, 0, 0 };
	GetClientRect(eqHWnd, &r);

	return point.x >= r.left && point.x < r.right
		&& point.y >= r.top && point.y < r.bottom;
}

inline bool IsMouseBlocked()
{
	PMOUSECLICK clicks = EQADDR_MOUSECLICK;

	// If a mouse button is down, we will not send it to imgui
	bool mouseDown = false;
	for (int i = 0; i < 8; i++)
	{
		if (clicks->Click[i] || clicks->Confirm[i])
			return true;
	}
	if (MouseState->Scroll != 0)
		return true;

	return false;
}

DETOUR_TRAMPOLINE_EMPTY(void ProcessMouseEvent_Trampoline())
void ProcessMouseEvent_Detour()
{
	// Get mouse position. Returns true if the mouse is within the window bounds
	GetMouseLocation(MouseLocation);

	// only process the mouse events if we are the foreground window.
	if (!gbInForeground)
	{
		ProcessMouseEvent_Trampoline();
		return;
	}

	// If a mouse button is down, we will not send it to imgui
	if (IsMouseBlocked())
	{
		MouseBlocked = true;
		ProcessMouseEvent_Trampoline();
		return;
	}

	if (!nav::GetSettings().show_ui)
	{
		ProcessMouseEvent_Trampoline();
		return;
	}

	MouseBlocked = false;	
		
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos.x = static_cast<float>(MouseLocation.x);
	io.MousePos.y = static_cast<float>(MouseLocation.y);

	if (!io.WantCaptureMouse)
	{
		// If mouse capture leaves, release all buttons
		memset(io.MouseDown, 0, sizeof(io.MouseDown));
		io.MouseWheel = 0;

		ProcessMouseEvent_Trampoline();
	}
	else
	{
		// need to reset relative movement variables
		MouseState->InWindow = 0;
	}
}

DETOUR_TRAMPOLINE_EMPTY(unsigned int ProcessKeyboardEvent_Trampoline());
unsigned int ProcessKeyboardEvent_Detour()
{
	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureKeyboard)
	{
		FlushDxKeyboard();
		return 0;
	}

	g_inKeyboardEventHandler = true;
	unsigned int result = ProcessKeyboardEvent_Trampoline();
	g_inKeyboardEventHandler = false;

	return result;
}

DETOUR_TRAMPOLINE_EMPTY(LRESULT __stdcall WndProc_Trampoline(HWND, UINT, WPARAM, LPARAM));
LRESULT __stdcall WndProc_Detour(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() == nullptr)
		return WndProc_Trampoline(hWnd, msg, wParam, lParam);

	ImGuiIO& io = ImGui::GetIO();

	switch (msg)
	{
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		break;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		break;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		break;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		break;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		break;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16);
		break;
	case WM_KEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = 1;
		{
			BYTE keyState[256];
			if (GetKeyboardState(keyState) == TRUE)
			{
				WORD character = 0;
				if (ToAscii(wParam, (lParam & 0x01FF0000) > 16, keyState, &character, 0) == 1)
				{
					io.AddInputCharacter(character);
				}
			}
		}

		break;
	case WM_KEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = 0;
		break;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		break;
	}

	return WndProc_Trampoline(hWnd, msg, wParam, lParam);
}

#define EQSWITCH_USESWTICH_LOGGING 0
#if EQSWITCH_USESWTICH_LOGGING
class EQSwitch_Detour
{
public:
	void UseSwitch_Detour(UINT SpawnID, int KeyID, int PickSkill, const CVector3* hitloc = 0)
	{
		if (!hitloc)
			DebugSpewAlways("UseSwitch: SpawnID=%d KeyID=%d PickSkill=%d hitloc=null",
				SpawnID, KeyID, PickSkill);
		else
			DebugSpewAlways("UseSwitch: SpawnID=%d KeyID=%d PickSkill=%d hitloc=(%.2f, %.2f, %.2f)",
				SpawnID, KeyID, PickSkill, hitloc->X, hitloc->Y, hitloc->Z);

		UseSwitch_Trampoline(SpawnID, KeyID, PickSkill, hitloc);
	}
	void UseSwitch_Trampoline(UINT SpawnID, int KeyId, int PickSkill, const CVector3* hitlock = 0);
};
DETOUR_TRAMPOLINE_EMPTY(void EQSwitch_Detour::UseSwitch_Trampoline(UINT, int, int, const CVector3*));
#endif

//----------------------------------------------------------------------------

HookStatus InitializeHooks()
{
	if (g_hooksInstalled)
	{
		if (!g_pDevice)
		{
			return HookStatus::MissingDevice;
		}

		return HookStatus::Success;
	}

	if (!GetOffsets()
		|| !InstallD3D9Hooks())
	{
		WriteChatf(PLUGIN_MSG "\arRendering support failed! We won't be able to draw to the 3D world.");
		return HookStatus::MissingOffset;
	}

	g_dx9Module = LoadLibraryA("EQGraphicsDX9.dll");

	// Detour that enables rendering into the world
	InstallDetour(ZoneRender_InjectionOffset,
		&RenderHooks::ZoneRender_Injection_Detour,
		&RenderHooks::ZoneRender_Injection_Trampoline,
		"ZoneRender_InjectinOffset");

	// Intercepts mouse events
	InstallDetour(__ProcessMouseEvent,
		ProcessMouseEvent_Detour,
		ProcessMouseEvent_Trampoline,
		"__ProcessMouseEvent");

	// Intercepts keyboard events.
	InstallDetour(__ProcessKeyboardEvent,
		ProcessKeyboardEvent_Detour,
		ProcessKeyboardEvent_Trampoline,
		"__ProcessKeyboardEvent");

	// Hook the window proc, so we can retrieve mouse/keyboard events. Maybe in the future
	// we use direct input for this too? Seems to work for now.
	InstallDetour(__WndProc, WndProc_Detour, WndProc_Trampoline, "__WndProc");

#if EQSWITCH_USESWTICH_LOGGING
	InstallDetour(EQSwitch__UseSwitch,
		&EQSwitch_Detour::UseSwitch_Detour,
		&EQSwitch_Detour::UseSwitch_Trampoline,
		"EQSwitch__UseSwitch");
#endif

	g_hooksInstalled = true;
	return !g_pDevice ? HookStatus::MissingDevice : HookStatus::Success;
}

static void RemoveDetours()
{
	for (HookInfo& hook : g_hooks)
	{
		if (hook.address != 0)
		{
			RemoveDetour(hook.address);
			hook.address = 0;
		}
	}
}

void ShutdownHooks()
{
	if (!g_hooksInstalled)
		return;

	RemoveDetours();

	g_hooksInstalled = false;
	g_hooks.clear();

	// Release our Direct3D device before freeing the dx9 library
	if (g_pDevice)
	{
		g_pDevice->Release();
		g_pDevice = nullptr;
	}

	if (g_d3d9Module)
	{
		FreeLibrary(g_d3d9Module);
		g_d3d9Module = 0;
	}

	FreeLibrary(g_dx9Module);

	if (g_inKeyboardEventHandler)
	{
		// A handle to our own module. We use this to avoid being released until we
		// have successfully left the keyboard hook.
		HMODULE selfModule = LoadLibraryA("MQ2Nav.dll");

		// start a thread and wait for g_inKeyboardEventHandler to be false.
		// then call FreeLibraryAndExitThread
		std::thread cleanup([selfModule]()
		{
			while (g_inKeyboardEventHandler)
			{
				Sleep(100);
			}

			// Wait a little bit AFTER the flag is cleared to allow enough time
			// to leave the hook function. 
			Sleep(5);

			// I Spent 3 days trying to figure out why this was needed. The
			// Reference count was always 1 after FreeLibraryAndExitThread, no
			// idea why...
			FreeLibrary(selfModule);

			// Free the library and exit this thread.
			FreeLibraryAndExitThread(selfModule, 0);
		});
		cleanup.detach();
	}
}

//----------------------------------------------------------------------------
