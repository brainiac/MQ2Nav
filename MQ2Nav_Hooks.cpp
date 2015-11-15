//
// MQ2Nav_Hooks.cpp
//

#include "MQ2Nav_Hooks.h"
#include "MQ2Navigation.h"
#include "ImGuiRenderer.h"
#include "RenderHandler.h"
#include "FindPattern.h"

#include <imgui.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <atomic>
#include <thread>
#include <boost/algorithm/string.hpp>
#include <winternl.h>

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

const char* ZoneRender_InjectionMask = "xxxxxxxxxxxx????xxx?xxxxxxx????xxx?xxxxxxxxxx????xxx?xxxxxxx????xxx?xxxxx";
const unsigned char* ZoneRender_InjectionPattern = (const unsigned char*)"\x56\x8b\xf1\x57\x8d\x46\x14\x50\x83\xcf\xff\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x4e\x5c\x51\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x96\x80\x00\x00\x00\x52\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x8d\x46\x38\x50\x8b\xce\xe8\x00\x00\x00\x00\x85\xc0\x78\x00\x5f\x33\xc0\x5e\xc3";


//----------------------------------------------------------------------------
// eqgame.exe offsets

#define __GetMouseDataRel_x                                     0x62Fe85
INITIALIZE_EQGAME_OFFSET(__GetMouseDataRel);

const char* GetMouseDataRel_Mask = "xx????x????xxxxxxxxxxxxxxxxxxxxxxxx????xx????xx????xxxxxxxxxxxxx?xxxxxx????x";
const unsigned char* GetMouseDataRel_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x53\x33\xdb\x53\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\xc7\x44\x24\x10\x80\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x8b\x08\x6a\x14\x50\x8b\x41\x28\xff\xd0\x85\xc0\x79\x00\x8d\x43\x07\x5b\x81\xc4\x00\x00\x00\x00\xc3";

#define __ProcessMouseEvent_x                                   0x56EDC0
INITIALIZE_EQGAME_OFFSET(__ProcessMouseEvent);

const char* ProcessMouseEvent_Mask = "xxxx????xxxxxxxxxxx????xxxxxxx?xxxx?xxxxx????xx????xxxx????xx????xx????xx????x?x????xxxxxxxx";
const unsigned char* ProcessMouseEvent_Pattern = (const unsigned char*)"\x83\xec\x14\xa1\x00\x00\x00\x00\x8b\x80\xc8\x05\x00\x00\x53\x56\x33\xdb\xbe\x00\x00\x00\x00\x88\x5c\x24\x0b\x3b\xc6\x74\x00\x83\xf8\x01\x74\x00\x83\xf8\x05\x0f\x85\x00\x00\x00\x00\xff\x15\x00\x00\x00\x00\x3b\xc3\x0f\x84\x00\x00\x00\x00\x3b\x05\x00\x00\x00\x00\x0f\x85\x00\x00\x00\x00\x39\x1d\x00\x00\x00\x00\x74\x00\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x1c\x50\xff\xd2";

#define __ProcessKeyboardEvent_x                                0x632AE0
INITIALIZE_EQGAME_OFFSET(__ProcessKeyboardEvent);

const char* ProcessKeyboardEvent_Mask = "xx????xxxxxxxxxx????xxx?xx????x?x????xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxx?x????";
const unsigned char* ProcessKeyboardEvent_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xc7\x44\x24\x04\x20\x00\x00\x00\xff\x15\x00\x00\x00\x00\x85\xc0\x74\x00\x3b\x05\x00\x00\x00\x00\x75\x00\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x1c\x50\xff\xd2\xa1\x00\x00\x00\x00\x8b\x08\x6a\x00\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\x6a\x14\x50\x8b\x41\x28\xff\xd0\x85\xc0\x74\x00\xe8\x00\x00\x00\x00";

#define __FlushDxKeyboard_x                                     0x62FAD0
INITIALIZE_EQGAME_OFFSET(__FlushDxKeyboard);

const char* FlushDxKeyboard_Mask = "xx????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx????xx????xx????xxxx?x????xxxxx????x";
const unsigned char* FlushDxKeyboard_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x53\x33\xdb\x53\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\xc7\x44\x24\x10\x20\x00\x00\x00\x8b\x08\x6a\x14\x50\x8b\x41\x28\xff\xd0\x8b\x0d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x88\x1d\x00\x00\x00\x00\x3b\xcb\x5b\x74\x00\xe8\x00\x00\x00\x00\x8b\x04\x24\x81\xc4\x00\x00\x00\x00\xc3";

FUNCTION_AT_ADDRESS(int FlushDxKeyboard(), __FlushDxKeyboard);

// Don't need a signature for this, can get it programmatically
DWORD __WndProc = 0;

class CRender
{
public:
	/*0x000*/ BYTE Unknown0x0[0xf08];
	/*0xf08*/ IDirect3DDevice9 *pDevice; // device pointer
};

//----------------------------------------------------------------------------

bool GetOffsets()
{
	if ((ZoneRender_InjectionOffset = FindPattern(FixEQGraphicsOffset(0x10000000), 0x100000,
		ZoneRender_InjectionPattern, ZoneRender_InjectionMask)) == 0)
		return false;

	if ((__ProcessMouseEvent = FindPattern(FixOffset(0x500000), 0x100000,
		ProcessMouseEvent_Pattern, ProcessMouseEvent_Mask)) == 0)
		return false;

	if ((__ProcessKeyboardEvent = FindPattern(FixOffset(0x600000), 0x100000,
		ProcessKeyboardEvent_Pattern, ProcessKeyboardEvent_Mask)) == 0)
		return false;

	if ((__FlushDxKeyboard = FindPattern(FixOffset(0x600000), 0x100000,
		FlushDxKeyboard_Pattern, FlushDxKeyboard_Mask)) == 0)
		return false;

	if ((__WndProc = GetWindowLongPtr(*(HWND*)EQADDR_HWND, GWLP_WNDPROC)) == 0)
		return false;

	return true;
}

IDirect3DDevice9* GetDeviceFromEverquest()
{
	if (CRender* pRender = (CRender*)g_pDrawHandler)
		return  pRender->pDevice;

	return nullptr;
}

//----------------------------------------------------------------------------

// the global direct3d device that we are "borrowing"
IDirect3DDevice9* g_pDevice = nullptr;

// represents whether the device has been acquired and is good to use.
bool g_deviceAcquired = false;

// check if we are in the keyboard event handler during shutdown
std::atomic_bool g_inKeyboardEventHandler = false;

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
		if (g_pDevice != GetDevice())
			return Reset_Trampoline(pPresentationParameters);

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
		return BeginScene_Trampoline();
	}

	HRESULT WINAPI TestCooperativeLevel_Trampoline();
	HRESULT WINAPI TestCooperativeLevel_Detour()
	{
		return TestCooperativeLevel_Trampoline();
	}

	HRESULT WINAPI EndScene_Trampoline();
	HRESULT WINAPI EndScene_Detour()
	{
		if (GetDevice() != g_pDevice)
			return EndScene_Trampoline();

		// When TestCooperativeLevel returns all good, then we can reinitialize.
		// This will let the renderer control our flow instead of having to
		// poll for the state ourselves.
		if (!g_deviceAcquired)
		{
			HRESULT result = GetDevice()->TestCooperativeLevel();

			if (result == D3D_OK)
			{
				g_deviceAcquired = true;
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
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::TestCooperativeLevel_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::Reset_Trampoline(D3DPRESENT_PARAMETERS* pPresentationParameters));
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::BeginScene_Trampoline());
DETOUR_TRAMPOLINE_EMPTY(HRESULT RenderHooks::EndScene_Trampoline());

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
	HWND foregroundWindow = GetForegroundWindow();
	HWND eqHWnd = *(HWND*)EQADDR_HWND;
	if (foregroundWindow != eqHWnd)
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

	MouseBlocked = false;

	ImGuiIO& io = ImGui::GetIO();
	io.MousePos.x = MouseLocation.x;
	io.MousePos.y = MouseLocation.y;

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
	// This event occurs after the mouse event. Could probably put this in process events
	if (g_deviceAcquired)
	{
		if (g_imguiRenderer)
		{
			g_imguiRenderer->BeginNewFrame();
		}
	}

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

//----------------------------------------------------------------------------

// have detours been installed already?
bool g_hooksInstalled = false;

// the handle to the graphics dll
HMODULE g_dx9Module = 0;

// list of installed hooks at their address, also whether they should be removed
// immediately or deferred. true = defer until later.
std::map<DWORD, bool> g_installedHooks;

template <typename T>
void InstallDetour(DWORD address, const T& detour, const T& trampoline, bool deferred = false)
{
	AddDetourf(address, detour, trampoline);
	g_installedHooks[address] = deferred;
}

bool InitializeHooks()
{
	if (g_hooksInstalled)
		return false;

	if (!GetOffsets())
	{
		WriteChatf(PLUGIN_MSG "\arRendering support failed! We won't be able to draw to the 3D world.");
		return false;
	}

	g_dx9Module = LoadLibraryA("EQGraphicsDX9.dll");

	// Grab the Direct3D9 device and add a reference to it
	g_pDevice = GetDeviceFromEverquest();
	g_pDevice->AddRef();

	// Detour that enables rendering into the world
	InstallDetour(ZoneRender_InjectionOffset,
		&RenderHooks::ZoneRender_Injection_Detour,
		&RenderHooks::ZoneRender_Injection_Trampoline);

	// Intercepts mouse events
	InstallDetour(__ProcessMouseEvent,
		ProcessMouseEvent_Detour,
		ProcessMouseEvent_Trampoline);

	// Intercepts keyboard events. Unload must be deferred
	InstallDetour(__ProcessKeyboardEvent,
		ProcessKeyboardEvent_Detour,
		ProcessKeyboardEvent_Trampoline);

	// Hook the window proc, so we can retrieve mouse/keyboard events. Maybe in the future
	// we use direct input for this too? Seems to work for now.
	InstallDetour(__WndProc, WndProc_Detour, WndProc_Trampoline);

	// IDirect3DDevice9 virtual function hooks
	DWORD* d3dDevice_vftable = *(DWORD**)g_pDevice;
	InstallDetour(d3dDevice_vftable[0x3],
		&RenderHooks::TestCooperativeLevel_Detour,
		&RenderHooks::TestCooperativeLevel_Trampoline);
	InstallDetour(d3dDevice_vftable[0x10],
		&RenderHooks::Reset_Detour,
		&RenderHooks::Reset_Trampoline);
	InstallDetour(d3dDevice_vftable[0x29],
		&RenderHooks::BeginScene_Detour,
		&RenderHooks::BeginScene_Trampoline);
	InstallDetour(d3dDevice_vftable[0x2a],
		&RenderHooks::EndScene_Detour,
		&RenderHooks::EndScene_Trampoline);

	g_hooksInstalled = true;
}

static void RemoveDetours()
{
	for (auto iter = g_installedHooks.begin(); iter != g_installedHooks.end(); ++iter)
	{
		RemoveDetour(iter->first);
	}
	g_installedHooks.clear();
}

void ShutdownHooks()
{
	if (!g_hooksInstalled)
		return;

	RemoveDetours();
	g_hooksInstalled = false;

	// Release our Direct3D device before freeing the dx9 library
	if (g_pDevice)
	{
		g_pDevice->Release();
		g_pDevice = nullptr;
	}

	FreeLibrary(g_dx9Module);

	if (g_inKeyboardEventHandler)
	{
		// A handle to our own module. We use this to avoid being released until we
		// have successfully left the keyboard hook.
		HMODULE selfModule = LoadLibraryA("MQ2Navigation.dll");

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
