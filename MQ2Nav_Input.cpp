//
// MQ2Nav_Input.cpp
//

#include "MQ2Nav_Input.h"
#include "MQ2Navigation.h"
#include "FindPattern.h"

#include <imgui.h>

//------------------------------------------------------------------------
// Hooks

// Copied from MQ2Globals.cpp
#define INITIALIZE_EQGAME_OFFSET(var) DWORD var = (((DWORD)var##_x - 0x400000) + baseAddress)

#define __GetMouseDataRel_x                                   0x62Fe85
INITIALIZE_EQGAME_OFFSET(__GetMouseDataRel);

#define ProcessMouseEvent_x                                 0x56EDC0
INITIALIZE_EQGAME_OFFSET(ProcessMouseEvent);


const char* GetMouseDataRel_Mask = "xx????x????xxxxxxxxxxxxxxxxxxxxxxxx????xx????xx????xxxxxxxxxxxxx?xxxxxx????x";
const unsigned char* GetMouseDataRel_Pattern = (const unsigned char*)"\x81\xec\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x53\x33\xdb\x53\x8d\x54\x24\x08\x52\x8d\x54\x24\x10\x52\xc7\x44\x24\x10\x80\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x8b\x08\x6a\x14\x50\x8b\x41\x28\xff\xd0\x85\xc0\x79\x00\x8d\x43\x07\x5b\x81\xc4\x00\x00\x00\x00\xc3";

const char* ProcessMouseEvent_Mask = "xxxx????xxxxxxxxxxx????xxxxxxx?xxxx?xxxxx????xx????xxxx????xx????xx????xx????x?x????xxxxxxxxx????xxx?xx????xxxxxxxx?xxxxxxxxx????";
const unsigned char* ProcessMouseEvent_Pattern = (const unsigned char*)"\x83\xec\x14\xa1\x00\x00\x00\x00\x8b\x80\xc8\x05\x00\x00\x53\x56\x33\xdb\xbe\x00\x00\x00\x00\x88\x5c\x24\x0b\x3b\xc6\x74\x00\x83\xf8\x01\x74\x00\x83\xf8\x05\x0f\x85\x00\x00\x00\x00\xff\x15\x00\x00\x00\x00\x3b\xc3\x0f\x84\x00\x00\x00\x00\x3b\x05\x00\x00\x00\x00\x0f\x85\x00\x00\x00\x00\x39\x1d\x00\x00\x00\x00\x74\x00\xa1\x00\x00\x00\x00\x8b\x08\x8b\x51\x1c\x50\xff\xd2\xa1\x00\x00\x00\x00\x3b\xc3\x74\x00\x8b\x0d\x00\x00\x00\x00\x83\xb9\xc8\x05\x00\x00\x05\x75\x00\x80\xb8\xae\x05\x00\x00\x66\x0f\x84\x00\x00\x00\x00";

static bool GetOffsets()
{
	if ((__GetMouseDataRel = FindPattern(FixOffset(0x600000), 0x100000,
		GetMouseDataRel_Pattern, GetMouseDataRel_Mask)) == 0)
		return false;

	if ((ProcessMouseEvent = FindPattern(FixOffset(0x500000), 0x100000,
		ProcessMouseEvent_Pattern, ProcessMouseEvent_Mask)) == 0)
		return false;

	return true;
}


//------------------------------------------------------------------------
// mouse handling

DIMOUSESTATE m_dims;
POINT MouseLocation;
bool MouseBlocked = false; // blocked if EQ is dragging mouse

// GetMouseDataRel populates
FUNCTION_AT_ADDRESS(void GetMouseDataRel(), __GetMouseDataRel);

struct MouseStateData {
	DWORD x;
	DWORD y;
	DWORD Scroll;
	DWORD relX;
	DWORD relY;
	DWORD Extra;
};
MouseStateData* MouseState = (MouseStateData*)EQADDR_DIMOUSECOPY;
DIMOUSESTATE* DIMouseState = (DIMOUSESTATE*)EQADDR_DIMOUSECHECK;

inline HWND GetWindowHandle()
{
	return *(HWND*)EQADDR_HWND;
}

struct MOUSEINFO2 {
	DWORD X;
	DWORD Y;
	DWORD SpeedX;
	DWORD SpeedY;
	DWORD Scroll;
};
MOUSEINFO2* MouseInfo = (MOUSEINFO2*)EQADDR_MOUSE;
ImGuiIO tempIO;

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

void ClampMousePosition(POINT& point)
{
	// This might mess up if we're in fullscreen mode.
	HWND eqHWnd = *(HWND*)EQADDR_HWND;

	RECT r = { 0, 0, 0, 0 };
	GetClientRect(eqHWnd, &r);

	if (point.x < r.left)
		point.x = r.left;
	else if (point.x >= r.right)
		point.x = r.right - 1;
	if (point.y < r.top)
		point.y = r.top;
	else if (point.y >= r.bottom)
		point.y = r.bottom - 1;
}

#define g_pMouse ((IDirectInputDevice8A*)(*EQADDR_DIMOUSE))

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

	return false;
}

DETOUR_TRAMPOLINE_EMPTY(void ProcessMouseEvent_Trampoline())

void ProcessMouseEvent_Detour()
{
	// We need to collect enough information from directInput to determine
	// if we will skip the ProcessMouseEvent call. This means we need to know:
	// * the position of the mouse
	// * the current state of the mouse buttons

	// This is a partial implementation of ProcessMouseEvent to recreate enough state
	// to determine the answers to these questions, feed it into ImGui, and then
	// determine if ImGui wants the mouse.

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

	// TEMP: This might be really wierd, but lets try processing the data
	// from the last frame.
	//g_pMouse->Acquire();

	// __Mouse ->
	// DI8__MOUSE__COPY

	ImGuiIO& io = ImGui::GetIO();
	io.MousePos.x = MouseLocation.x;
	io.MousePos.y = MouseLocation.y;

	if (!io.WantCaptureMouse)
	{
		ProcessMouseEvent_Trampoline();
	}
	else
	{
		// need to reset relative movement variables
		MouseState->relX = 0;
		MouseState->relY = 0;
		//MouseState->Scroll = 0;
		MouseState->Extra = 0;

		DIMouseState->lX = 0;
		DIMouseState->lY = 0;
		//DIMouseState->lZ = 0;

		//MouseInfo->Scroll = 0;

		io.MouseDown[0] = tempIO.MouseDown[0];
		io.MouseDown[1] = tempIO.MouseDown[1];
		io.MouseWheel = tempIO.MouseWheel;
		memcpy(io.KeysDown, tempIO.KeysDown, sizeof(tempIO.KeysDown));
		memcpy(io.InputCharacters, tempIO.InputCharacters, sizeof(tempIO.InputCharacters));
	}
}

DWORD WndProcAddress = 0;

DETOUR_TRAMPOLINE_EMPTY(LRESULT __stdcall WndProc_Trampoline(HWND, UINT, WPARAM, LPARAM));
LRESULT __stdcall WndProc_Detour(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		tempIO.MouseDown[0] = true;
		break;
	case WM_LBUTTONUP:
		tempIO.MouseDown[0] = false;
		break;
	case WM_RBUTTONDOWN:
		tempIO.MouseDown[1] = true;
		break;
	case WM_RBUTTONUP:
		tempIO.MouseDown[1] = false;
		break;
	case WM_MOUSEWHEEL:
		tempIO.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		break;
	case WM_MOUSEMOVE:
		tempIO.MousePos.x = (signed short)(lParam);
		tempIO.MousePos.y = (signed short)(lParam >> 16);
		break;
	case WM_KEYDOWN:
		if (wParam < 256)
			tempIO.KeysDown[wParam] = 1;
		break;
	case WM_KEYUP:
		if (wParam < 256)
			tempIO.KeysDown[wParam] = 0;
		break;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			tempIO.AddInputCharacter((unsigned short)wParam);
		break;
	}

	return WndProc_Trampoline(hWnd, msg, wParam, lParam);
}

void InstallInputHooks()
{
	EzDetour(ProcessMouseEvent, ProcessMouseEvent_Detour, ProcessMouseEvent_Trampoline);

	// Hook the wndproc
	HWND eqHWnd = *(HWND*)EQADDR_HWND;
	WndProcAddress = GetWindowLongPtr(eqHWnd, GWLP_WNDPROC);

	EzDetour(WndProcAddress, WndProc_Detour, WndProc_Trampoline);
}

void RemoveInputHooks()
{
	RemoveDetour(WndProcAddress);
	RemoveDetour(ProcessMouseEvent);
}

void RenderInputUI()
{
	ImGui::BeginChild("##Input");

	if (ImGui::CollapsingHeader("Keyboard", "##keyboard", true, true))
	{
		ImGui::Text("Keyboard");
		ImGui::SameLine();

		if (ImGui::GetIO().WantCaptureKeyboard)
			ImGui::TextColored(ImColor(0, 255, 0), "Captured");
		else
			ImGui::TextColored(ImColor(255, 0, 0), "Not Captured");

		ImGui::Text("Text Input");
		ImGui::SameLine();

		if (ImGui::GetIO().WantTextInput)
			ImGui::TextColored(ImColor(0, 255, 0), "Captured");
		else
			ImGui::TextColored(ImColor(255, 0, 0), "Not Captured");
	}

	if (ImGui::CollapsingHeader("Mouse", "##mouse", true, true))
	{
		ImGui::Text("Position: (%d, %d)", MouseLocation, MouseLocation.y);

		ImGui::Text("Mouse");
		ImGui::SameLine();

		if (ImGui::GetIO().WantCaptureMouse)
			ImGui::TextColored(ImColor(0, 255, 0), "Captured");
		else
			ImGui::TextColored(ImColor(255, 0, 0), "Not Captured");

		ImGui::Text("Mouse");
		ImGui::SameLine();

		if (!MouseBlocked)
			ImGui::TextColored(ImColor(0, 255, 0), "Not Blocked");
		else
			ImGui::TextColored(ImColor(255, 0, 0), "Blocked");

		// Clicks? Clicks!
		PMOUSECLICK clicks = EQADDR_MOUSECLICK;

		ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("##clicks", "EQ Mouse Clicks"))
		{
			ImGui::Columns(3);

			ImGui::Text("Button"); ImGui::NextColumn();
			ImGui::Text("Click"); ImGui::NextColumn();
			ImGui::Text("Confirm"); ImGui::NextColumn();

			for (int i = 0; i < 5; i++)
			{
				ImGui::Text("%d", i); ImGui::NextColumn();
				ImGui::Text("%d", clicks->Click[i]); ImGui::NextColumn();
				ImGui::Text("%d", clicks->Confirm[i]); ImGui::NextColumn();
			}

			ImGui::Columns(1);
			ImGui::TreePop();
		}

		ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("##position", "EQ Mouse State"))
		{
			ImGui::LabelText("Position", "(%d, %d)", MouseState->x, MouseState->y);
			ImGui::LabelText("Scroll", "%d", MouseState->Scroll);
			ImGui::LabelText("Relative", "%d, %d", MouseState->relX, MouseState->relY);
			ImGui::LabelText("Extra", "%d", MouseState->Extra);

			ImGui::TreePop();
		}

		ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("##dimouse", "DI Mouse State"))
		{
			int pos[3] = { DIMouseState->lX, DIMouseState->lY, DIMouseState->lZ };
			ImGui::InputInt3("Pos", pos);

			int buttons[4] = { DIMouseState->rgbButtons[0], DIMouseState->rgbButtons[1], DIMouseState->rgbButtons[2], DIMouseState->rgbButtons[3] };
			ImGui::InputInt4("Buttons", buttons);

			ImGui::TreePop();
		}

		ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("##mouseinfo", "EQ Mouse Info"))
		{
			int pos[2] = { EQADDR_MOUSE->X, EQADDR_MOUSE->Y };
			ImGui::InputInt2("Pos", pos);

			int speed[2] = { EQADDR_MOUSE->SpeedX, EQADDR_MOUSE->SpeedY };
			ImGui::InputInt2("Speed", speed);

			int scroll = MouseInfo->Scroll;
			ImGui::InputInt("Scroll", &scroll);

			ImGui::TreePop();
		}
	}

	ImGui::EndChild();
}

//
//
//void MQ2NavigationRender::DoMouse()
//{
//	IDirectInputDevice8A* device = *EQADDR_DIMOUSE;
//	HRESULT err;
//
//	if ((err = device->Acquire()) != DI_OK)
//		return;
//
//	if ((err = device->GetDeviceState(sizeof(DIMOUSESTATE), &m_dims) != DI_OK))
//		return;
//
//}
