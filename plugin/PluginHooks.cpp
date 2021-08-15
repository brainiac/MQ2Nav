//
// PluginHooks.cpp
//

#include "pch.h"
#include "PluginHooks.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/ImGuiRenderer.h"
#include "plugin/RenderHandler.h"
#include "plugin/imgui/imgui_impl_win32.h"
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
#include <spdlog/spdlog.h>
#include <winternl.h>
#include <tchar.h>

#include <cfenv>

//============================================================================

// have detours been installed already?
bool g_hooksInstalled = false;

#if !defined(MQNEXT)

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

struct SigPatternInfo {
	const char* mask;
	const unsigned char* pattern;
	uintptr_t rangeBegin;
	uintptr_t rangeEnd;
};

//============================================================================
// EQGraphicsDX9.dll offsets

//----------------------------------------------------------------------------
// CParticleSystem__Render

// you can find this one near one of the D3DPERF_EndEvent Calls:
//
// .text:10018E38                 call    sub_10071560 <--
// .text:10018E3D                 mov     eax, dword_101B1E5C
// .text:10018E42                 mov     dword ptr [eax+0BCh], 0FFFFFFFFh
// .text:10018E4C                 mov     ecx, [edi+8]
// .text:10018E4F                 call    sub_10023A60
// .text:10018E54                 call    D3DPERF_EndEvent

// RenderFlames_sub_10072EB0 in EQGraphicsDX9.dll ~ 2015-08-20
DWORD CParticleSystem__Render = 0;

SigPatternInfo CParticleSystem__Render_PatternInfo[] = {
#if defined(ROF2EMU) || defined(UFEMU)
	// 0x10072110 rof2
	// Sig: 56 8B F1 57 8D 46 14 50 83 CF FF E8 ? ? ? ? 85 C0 78 35 8D 4E 5C 51 8B CE E8 ? ? ? ?
	// Sig: \x56\x8B\xF1\x57\x8D\x46\x14\x50\x83\xCF\xFF\xE8\x00\x00\x00\x00\x85\xC0\x78\x35\x8D\x4E\x5C\x51\x8B\xCE\xE8\x00\x00\x00\x00
	{
		"xxxxxxxxxxxx????xxxxxxxxxxx????",
		(const unsigned char*)"\x56\x8B\xF1\x57\x8D\x46\x14\x50\x83\xCF\xFF\xE8\x00\x00\x00\x00\x85\xC0\x78\x35\x8D\x4E\x5C\x51\x8B\xCE\xE8\x00\x00\x00\x00",
		0x10000000, 0x10500000,
	},
#else
	// 56 8B F1 8D 46 14 50 E8 ? ? ? ? 85 C0 78 38 8D 46 5C 8B CE 50 E8 ? ? ? ? 85 C0 78 29 8D 86 ? ? ? ? 8B CE 50 E8 ? ? ? ? 85 C0 78 17 8D 46 38 8B CE 50 E8 ? ? ? ? 33 C9 85 C0 5E 0F 99 C1 8D 41 FF C3
	{
		"xxxxxxxx????xxxxxxxxxxx????xxxxxx????xxxx????xxxxxxxxxxx????xxxxxxxxxxxx",
		(const unsigned char*)"\x56\x8B\xF1\x8D\x46\x14\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x38\x8D\x46\x5C\x8B\xCE\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x29\x8D\x86\x00\x00\x00\x00\x8B\xCE\x50\xE8\x00\x00\x00\x00\x85\xC0\x78\x17\x8D\x46\x38\x8B\xCE\x50\xE8\x00\x00\x00\x00\x33\xC9\x85\xC0\x5E\x0F\x99\xC1\x8D\x41\xFF\xC3",
		0x10000000, 0x10500000,
	},
#endif
};

//============================================================================
// eqgame.exe offsets

INITIALIZE_EQGAME_OFFSET(__ProcessMouseEvent);

// Calls __FlushDxKeyboard, GetAsyncKeyState, and GetForegroundWindow multiple times. Look for function that
// calls GetAsyncKeyState a bunch, with GetforegroundWindow and __FlushDxKeyboard calls in the first block.

//----------------------------------------------------------------------------
// ProcessKeyboardEvent

DWORD __ProcessKeyboardEvent = 0;

// 0x643e16 - 2018-02-18
//const char* ProcessKeyboardEvent_Mask = "xx????xxxxxxxxxx????xxx?xx????x?x????xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxx?x????";
//const unsigned char* ProcessKeyboardEvent_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xc7\x44\x24\x04\x20\x00\x00\x00\xff\x15\x00\x00\x00\x00\x85\xc0\x74\x00\x3b\x05\x00\x00\x00\x00\x75\x00\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x1c\x50\xff\xd2\xa1\x00\x00\x00\x00\x8b\x08\x6a\x00\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\x6a\x14\x50\x8b\x41\x28\xff\xd0\x85\xc0\x74\x00\xe8\x00\x00\x00\x00";

SigPatternInfo ProcessKeyboardEvent_PatternInfo[] = {
	// Sig: 81 EC ? ? ? ? C7 44 24 ? ? ? ? ?
	{
		"xx????xxx?????",
		(const unsigned char*)"\x81\xEC\x00\x00\x00\x00\xC7\x44\x24\x00\x00\x00\x00\x00",
		0x550000, 0xB50000,
	}
};

//----------------------------------------------------------------------------
// FlushDxKeyboard

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
//

// 0x73e9b0 - 2018-02-16
// 0x640f00 - 2018-02-18
// 0x6CA9B0 - 2020-05-21
// 0x6CB640 - 202-06-09 test

SigPatternInfo FlushDxKeyboard_PatternInfo[] = {
#if defined(ROF2EMU) || defined(UFEMU)
	// 0x81 0xEC ? ? ? ? 0xA1 ? ? ? ? 0x53 0x33 0xDB 0x53 0x8D 0x54 0x24 0x08 0x52 0x8D 0x54 0x24 0x10 0x52 0xC7 0x44 0x24 0x10 0x20 0x00 0x00 0x00 0x8B 0x08 0x6A 0x14 0x50 0x8B 0x41 0x28 0xFF 0xD0 0x8B 0x0D ? ? ? ? 0x89 0x1D ? ? ? ? 0x89 0x1D ? ? ? ? 0x88 0x1D ? ? ? ? 0x3B 0xCB 0x5B 0x74 ? 0xE8 ? ? ? ? 0x8B 0x04 0x24 0x81 0xC4 ? ? ? ? 0xC3
	{
		"xx????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx????xx????xx????xxxx?x????xxxxx????x",
		(const unsigned char*)"\x81\xec\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x53\x33\xdb\x53\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\xc7\x44\x24\x10\x20\x00\x00\x00\x8b\x08\x6a\x14\x50\x8b\x41\x28\xff\xd0\x8b\x0d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x88\x1d\x00\x00\x00\x00\x3b\xcb\x5b\x74\x00\xe8\x00\x00\x00\x00\x8b\x04\x24\x81\xc4\x00\x00\x00\x00\xc3",
		0x550000, 0xB50000,
	},
#else
	{
		"xx????x????xxxxxxxxxxxxx?????xxxxxxxxx",
		(const unsigned char*)"\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x8D\x14\x24\x6A\x00\x52\x8D\x54\x24\x0C\xC7\x44\x24\x00\x00\x00\x00\x00\x8B\x08\x52\x6A\x14\x50\xFF\x51\x28",
		0x550000, 0xB50000,
	},
	// 81 EC ? ? ? ? 8B 0D ? ? ? ? B8 ? ? ? ? 89 04 24 85 C9 74 16 8B 01 8D 14 24 6A 00 52 8D 54 24 0C 52
	{
		"xx????xx????x????",
		(const unsigned char*)"\x81\xEC\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00\xB8\x00\x00\x00\x00",
		0x550000, 0xB50000,
	},
#endif
};

FUNCTION_AT_ADDRESS(int FlushDxKeyboard(), __FlushDxKeyboard);

//----------------------------------------------------------------------------
// WndProc

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

// 0x6431f0 - 2018-02-18
//const char* WndProc_Mask = "xxx????xx????xxxx????xxxxx????xxxxxxxxxxxxxxxxxxxxxxxxx?x????xxx?xx????xxxx????xxxx????xx????";
//const unsigned char* WndProc_Pattern = (const unsigned char*)"\x6a\xff\x68\x00\x00\x00\x00\x64\xa1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xec\x2c\x8b\x0d\x00\x00\x00\x00\x53\x55\x8b\x6c\x24\x4c\x56\x8b\x74\x24\x54\x33\xdb\x57\x8b\x7c\x24\x50\x89\x5c\x24\x10\x3b\xcb\x74\x00\xe8\x00\x00\x00\x00\x84\xc0\x74\x00\x8b\x0d\x00\x00\x00\x00\x56\x55\x57\xe8\x00\x00\x00\x00\x85\xc0\x0f\x84\x00\x00\x00\x00\x39\x1d\x00\x00\x00\x00";

SigPatternInfo WndProc_PatternInfo[] = {
#if defined(ROF2EMU) || defined(UFEMU)
	// 0x6431f0 - 2018-02-18
	// Sig: 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 40 8B 0D ? ? ? ? 55 8B 6C 24 5C 56 8B 74 24 5C 57
	{
		"xxx????xx????xxxx????xxxxx????xxxxxxxxxxx",
		(const unsigned char*)"\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x40\x8B\x0D\x00\x00\x00\x00\x55\x8B\x6C\x24\x5C\x56\x8B\x74\x24\x5C\x57",
		0x550000, 0xB50000,
	}
#else
	// 55 8B EC 64 A1 ? ? ? ? 6A FF 68 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 1C 8B 0D ? ? ? ?
	{
		"xxxxx????xxx????xxxx????xxxxx????",
		(const unsigned char*)"\x55\x8B\xEC\x64\xA1\x00\x00\x00\x00\x6A\xFF\x68\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x1C\x8B\x0D\x00\x00\x00\x00",
		0x550000, 0xB50000,
	}
#endif
};

//----------------------------------------------------------------------------

bool GetOffsets()
{
	//----------------------------------------------------------------------------
	// EQGraphicsDX9.dll Offset

	// Find the EQGraphicsDX9.dll image size, and use it to calculate the upper bound of
	// each signature search range.

	{
		MODULEINFO EQGraphicsModInfo = { 0 };
		GetModuleInformation(GetCurrentProcess(), (HMODULE)EQGraphicsBaseAddress, &EQGraphicsModInfo, sizeof(MODULEINFO));
		int EQGraphicsUpperBound = EQGraphicsBaseAddress + EQGraphicsModInfo.SizeOfImage;

		for (const SigPatternInfo& info : CParticleSystem__Render_PatternInfo)
		{
			CParticleSystem__Render = FindPattern(FixEQGraphicsOffset(info.rangeBegin), info.rangeEnd - info.rangeBegin, EQGraphicsUpperBound, info.pattern, info.mask);
			if (CParticleSystem__Render)
				break;
		}
		if (!CParticleSystem__Render)
			return false;
	}

	//----------------------------------------------------------------------------
	// EQGame.exe Offsets

	{
		MODULEINFO EQGameModInfo = { 0 };
		GetModuleInformation(GetCurrentProcess(), (HMODULE)baseAddress, &EQGameModInfo, sizeof(MODULEINFO));
		int EQGameUpperBound = baseAddress + EQGameModInfo.SizeOfImage;

		// __ProcessKeyboardEvent:
		for (const SigPatternInfo& info : ProcessKeyboardEvent_PatternInfo)
		{
			__ProcessKeyboardEvent = FindPattern(FixOffset(info.rangeBegin), info.rangeEnd - info.rangeBegin, EQGameUpperBound, info.pattern, info.mask);
			if (__ProcessKeyboardEvent)
				break;
		}
		if (!__ProcessKeyboardEvent)
			return false;

		// __FlushDxKeyboard:
		for (const SigPatternInfo& info : FlushDxKeyboard_PatternInfo)
		{
			__FlushDxKeyboard = FindPattern(FixOffset(info.rangeBegin), info.rangeEnd - info.rangeBegin, EQGameUpperBound, info.pattern, info.mask);
			if (__FlushDxKeyboard)
				break;
		}
		if (!__FlushDxKeyboard)
			return false;

		// __WndProc:
		for (const SigPatternInfo& info : WndProc_PatternInfo)
		{
			__WndProc = FindPattern(FixOffset(info.rangeBegin), info.rangeEnd - info.rangeBegin, EQGameUpperBound, info.pattern, info.mask);
			if (__WndProc)
				break;
		}
		if (!__WndProc)
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

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
IDirect3DDevice9* gpD3D9Device = nullptr;

// represents whether the device has been acquired and is good to use.
bool gbDeviceAcquired = false;

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
				SPDLOG_WARN("Detected a change in the rendering device. Attempting to recover.");
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
		if (gpD3D9Device != GetThisDevice())
		{
			return Reset_Trampoline(pPresentationParameters);
		}

		gbDeviceAcquired = false;

		if (g_renderHandler)
		{
			g_renderHandler->InvalidateDeviceObjects();
		}

		return Reset_Trampoline(pPresentationParameters);
	}

	HRESULT WINAPI BeginScene_Trampoline();
	HRESULT WINAPI BeginScene_Detour()
	{
		gpD3D9Device = GetThisDevice();

		return BeginScene_Trampoline();
	}

	HRESULT WINAPI EndScene_Trampoline();
	HRESULT WINAPI EndScene_Detour()
	{
		if (GetThisDevice() != gpD3D9Device)
		{
			return EndScene_Trampoline();
		}

		// When TestCooperativeLevel returns all good, then we can reinitialize.
		// This will let the renderer control our flow instead of having to
		// poll for the state ourselves.
		if (!gbDeviceAcquired)
		{
			HRESULT result = GetThisDevice()->TestCooperativeLevel();

			if (result == D3D_OK)
			{
				gbDeviceAcquired = true;

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
		if (gbDeviceAcquired)
		{
			IDirect3DStateBlock9* stateBlock = nullptr;
			gpD3D9Device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

			if (g_imguiRenderer)
			{
				g_imguiRenderer->ImGuiRender();
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
		if (gbDeviceAcquired)
		{
			IDirect3DStateBlock9* stateBlock = nullptr;
			gpD3D9Device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

			if (g_renderHandler)
			{
				g_renderHandler->PerformRender();
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

				// save the rounding state. We'll restore it after we're done here.
				// For some reason, CreateDeviceEx seems to tamper with it.
				int round = fegetround();

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
					SPDLOG_DEBUG("InstallD3D9Hooks: failed to CreateDeviceEx. Result={:#x}", hRes);
				}

				// restore floating point rounding state
				fesetround(round);

				d3d9ex->Release();
			}
			else
			{
				SPDLOG_DEBUG("InstallD3D9Hooks: failed Direct3DCreate9Ex failed. Result={:#x}", hRes);
			}
		}
		else
		{
			SPDLOG_DEBUG("InitD3D9CApture: failed to get address of Direct3DCreate9Ex");
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

// Mouse hook prevents mouse events from reaching EQ when imgui captures
// the mouse. Needed because ImGui uses win32 events but EQ uses directinput.
DETOUR_TRAMPOLINE_EMPTY(void ProcessMouseEvent_Trampoline())
void ProcessMouseEvent_Detour()
{
	// only process the mouse events if we are the foreground window.
	if (!gbInForeground)
	{
		ProcessMouseEvent_Trampoline();
		return;
	}

	if (nav::GetSettings().show_ui && ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
		{
			MouseState->InWindow = 0;
			return;
		}
	}

	ProcessMouseEvent_Trampoline();
}

// Keyboard hook prevents keyboard events from reaching EQ when imgui captures
// the keyboard. Needed because ImGui uses win32 events but EQ uses directinput.
DETOUR_TRAMPOLINE_EMPTY(unsigned int ProcessKeyboardEvent_Trampoline());
unsigned int ProcessKeyboardEvent_Detour()
{
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& io = ImGui::GetIO();

		if (io.WantCaptureKeyboard)
		{
			FlushDxKeyboard();
			return 0;
		}
	}

	// If you /unload or /plugin mq2nav unload via command, then the plugin will
	// try to unload while inside this hook. We need to fix that by unloading later.
	g_inKeyboardEventHandler = true;
	unsigned int result = ProcessKeyboardEvent_Trampoline();
	g_inKeyboardEventHandler = false;

	return result;
}

// Forwards events to ImGui. If ImGui consumes the event, we won't pass it to the game.
DETOUR_TRAMPOLINE_EMPTY(LRESULT __stdcall WndProc_Trampoline(HWND, UINT, WPARAM, LPARAM));
LRESULT WINAPI WndProc_Detour(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_imguiRenderer && g_imguiRenderer->IsReady())
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		if (msg == WM_KEYDOWN && ImGui::GetCurrentContext() != nullptr)
		{
			// We need to send an AddInputCharacter because WM_CHAR does not get sent by EQ.
			BYTE keyState[256];
			if (GetKeyboardState(keyState) == TRUE)
			{
				WORD character = 0;

				if (ToAscii(wParam, (lParam & 0x1ff0000) > 16, keyState, &character, 0) == 1)
				{
					ImGuiIO& io = ImGui::GetIO();

					io.AddInputCharacter(character);
				}
			}
		}
	}

	return WndProc_Trampoline(hWnd, msg, wParam, lParam);
}

//----------------------------------------------------------------------------

HookStatus InitializeHooks()
{
	if (g_hooksInstalled)
	{
		if (!gpD3D9Device)
		{
			return HookStatus::MissingDevice;
		}

		return HookStatus::Success;
	}

	if (!GetOffsets()
		|| !InstallD3D9Hooks())
	{
		SPDLOG_ERROR("Rendering support failed! We won't be able to draw to the 3d world.");
		return HookStatus::MissingOffset;
	}

	g_dx9Module = LoadLibraryA("EQGraphicsDX9.dll");

	// Detour that enables rendering into the world
	InstallDetour(CParticleSystem__Render,
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
	return !gpD3D9Device ? HookStatus::MissingDevice : HookStatus::Success;
}
#endif // !defined(MQNEXT)

#if defined(MQNEXT)

static void MQCreateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->CreateDeviceObjects();
	}
}

static void MQInvalidateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->InvalidateDeviceObjects();
	}
}

static void MQImGuiRender()
{
	if (g_imguiRenderer)
	{
		g_imguiRenderer->ImGuiRender();
	}
}

static void MQGraphicsSceneRender()
{
	if (g_renderHandler)
	{
		g_renderHandler->PerformRender();
	}
}

static int sMQCallbacksId = -1;

static void AddNavRenderCallbacks()
{
	if (sMQCallbacksId != -1)
		return;

	MQRenderCallbacks callbacks;
	callbacks.CreateDeviceObjects = MQCreateDeviceObjects;
	callbacks.InvalidateDeviceObjects = MQInvalidateDeviceObjects;
	callbacks.ImGuiRender = MQImGuiRender;
	callbacks.GraphicsSceneRender = MQGraphicsSceneRender;

	sMQCallbacksId = AddRenderCallbacks(callbacks);
}

HookStatus InitializeHooks()
{
	if (gpD3D9Device)
	{
		g_hooksInstalled = true;
		AddNavRenderCallbacks();
	}

	if (g_hooksInstalled)
	{
		if (!gpD3D9Device)
		{
			return HookStatus::MissingDevice;
		}

		return HookStatus::Success;
	}

#if EQSWITCH_USESWTICH_LOGGING
	EzDetour(EQSwitch__UseSwitch,
		&EQSwitch_Detour::UseSwitch_Detour,
		&EQSwitch_Detour::UseSwitch_Trampoline);
#endif

	return !gpD3D9Device ? HookStatus::MissingDevice : HookStatus::Success;
}
#endif // defined(MQNEXT)

void ShutdownHooks()
{
	if (!g_hooksInstalled)
		return;

	RemoveRenderCallbacks(sMQCallbacksId);

#if !defined(MQNEXT)
	RemoveDetours();

	g_hooksInstalled = false;
	g_hooks.clear();

	// Release our Direct3D device before freeing the dx9 library
	if (gpD3D9Device)
	{
		gpD3D9Device->Release();
		gpD3D9Device = nullptr;
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
#endif
}

//----------------------------------------------------------------------------
