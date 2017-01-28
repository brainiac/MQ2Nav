//
// ImGuiRenderer.cpp
//

#include "ImGuiRenderer.h"
#include "ImGuiDX9.h"
#include "MQ2Navigation.h"
#include "MQ2Nav_Hooks.h"

#include <imgui.h>
#include <imgui/imgui_custom/ImGuiUtils.h>
#include <imgui/fonts/font_roboto_regular_ttf.h>

//----------------------------------------------------------------------------

static void ConfigureFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	// font: Roboto Regular @ 16px
	io.Fonts->AddFontFromMemoryCompressedTTF(GetRobotoRegularCompressedData(),
		GetRobotoRegularCompressedSize(), 16.0);

	//// font: FontAwesome
	//ImFontConfig faConfig;
	//faConfig.MergeMode = true;
	//static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	//io.Fonts->AddFontFromMemoryCompressedTTF(GetFontAwesomeCompressedData(),
	//	GetFontAwesomeCompressedSize(), 14.0f, &faConfig, icon_ranges);

	//// font: Material Design Icons
	//ImFontConfig mdConfig;
	//mdConfig.MergeMode = true;
	//static const ImWchar md_icon_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
	//io.Fonts->AddFontFromMemoryCompressedTTF(GetMaterialIconsCompressedData(),
	//	GetMaterialIconsCompressedSize(), 13.0f, &mdConfig, md_icon_ranges);
}

ImGuiRenderer::ImGuiRenderer(HWND eqhwnd, IDirect3DDevice9* device)
	: m_pDevice(device)
{
	// Iniialize the ImGui overlay
	ImGui_ImplDX9_Init(eqhwnd, device);

	ConfigureFonts();

	ImGuiIO& io = ImGui::GetIO();

	m_iniFileName = std::string(gszINIPath) + "\\MQ2NavUI.ini";
	io.IniFilename = m_iniFileName.c_str();

	ImGui::SetupImGuiStyle(true, 0.8f);

	m_pDevice->AddRef();

	m_prevHistoryPoint = std::chrono::system_clock::now();
	m_renderFrameRateHistory.clear();
}

ImGuiRenderer::~ImGuiRenderer()
{
	InvalidateDeviceObjects();

	// Cleanup the ImGui overlay
	ImGui_ImplDX9_Shutdown();

	m_pDevice->Release();
	m_pDevice = nullptr;
}

void ImGuiRenderer::InvalidateDeviceObjects()
{
	m_imguiReady = false;

	ImGui_ImplDX9_InvalidateDeviceObjects();
}

bool ImGuiRenderer::CreateDeviceObjects()
{
	return ImGui_ImplDX9_CreateDeviceObjects();
}

void ImGuiRenderer::Render(RenderPhase phase)
{
	if (phase != Render_UI)
		return;
	if (!m_visible)
		return;

	if (m_imguiReady)
	{
		// don't draw ui if we're not in game, but also
		// do call render so we keep the imgui input state clear.
		if (gGameState == GAMESTATE_INGAME && m_imguiRender)
		{

			DrawUI();

			m_imguiRender = false;
		}

		ImGui::Render();
	}
}

void ImGuiRenderer::BeginNewFrame()
{
	m_imguiReady = ImGui_ImplDX9_NewFrame();
	m_imguiRender = true;
}

void ImGuiRenderer::SetVisible(bool visible)
{
	m_visible = visible;
}

//----------------------------------------------------------------------------

void RenderInputUI();

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

				char label[32] = { 0 };
				sprintf_s(label, "%.2f", f);
				ImGui::Text(label); ImGui::NextColumn();
			}
		}
		ImGui::Columns(1);
	}
}

void ImGuiRenderer::DrawUI()
{
#if 0

	auto now = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_prevHistoryPoint).count() >= 100)
	{
		if (m_renderFrameRateHistory.size() > 200) {
			m_renderFrameRateHistory.erase(m_renderFrameRateHistory.begin(), m_renderFrameRateHistory.begin() + 20);
		}
		m_renderFrameRateHistory.push_back(ImGui::GetIO().Framerate);
		m_prevHistoryPoint = now;
	}

	// 1. Show a simple window
	// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(325, 700), ImGuiSetCond_Once);
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
#endif
	OnUpdateUI();
}


void RenderInputUI()
{
	return;

	if (ImGui::CollapsingHeader("Keyboard", "##keyboard", true, true))
	{
		ImGui::Text("Keyboard");
		ImGui::SameLine();

		if (ImGui::GetIO().WantCaptureKeyboard)
			ImGui::TextColored(ImColor(0, 255, 0), "Captured");
		else
			ImGui::TextColored(ImColor(255, 0, 0), "Not Captured");

		ImGui::LabelText("InputCharacters", "%S", ImGui::GetIO().InputCharacters);

		static char testEdit[256];
		ImGui::InputText("Test Edit", testEdit, 256);
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

		ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
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

		ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
		if (ImGui::TreeNode("##position", "EQ Mouse State"))
		{
			ImGui::LabelText("Position", "(%d, %d)", MouseState->x, MouseState->y);
			ImGui::LabelText("Scroll", "%d", MouseState->Scroll);
			ImGui::LabelText("Relative", "%d, %d", MouseState->relX, MouseState->relY);
			ImGui::LabelText("Extra", "%d", MouseState->InWindow);

			ImGui::TreePop();
		}

		ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
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
}
