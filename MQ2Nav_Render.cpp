//
// MQ2Nav_Render.cpp
//

#include "MQ2Nav_Render.h"
#include "MQ2Nav_Hooks.h"
#include "MQ2Navigation.h"

#include "FindPattern.h"
#include "imgui_impl_dx9.h"

#include <cassert>

//============================================================================

void RenderInputUI();

//----------------------------------------------------------------------------

std::shared_ptr<MQ2NavigationRender> g_render;


MQ2NavigationRender::MQ2NavigationRender()
{
	Initialize();
}

MQ2NavigationRender::~MQ2NavigationRender()
{
	Cleanup();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void MQ2NavigationRender::Initialize()
{
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

	m_deviceAcquired = true;
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
}

void MQ2NavigationRender::RenderGeometry()
{
	g_pDevice->GetTransform(D3DTS_WORLD, &m_worldMatrix);
	g_pDevice->GetTransform(D3DTS_PROJECTION, &m_projMatrix);
	g_pDevice->GetTransform(D3DTS_VIEW, &m_viewMatrix);

	UpdateNavigationPath();

	if (m_pLines)
	{
		g_pDevice->SetMaterial(&m_navPathMaterial);
		g_pDevice->SetStreamSource(0, m_pLines, 0, sizeof(LineVertex));
		g_pDevice->SetFVF(LineVertex::FVF_Flags);

		g_pDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, m_navpathLen - 1);
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

void MQ2NavigationRender::RenderOverlay()
{
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
		result = g_pDevice->CreateVertexBuffer((m_navpathLen + 1) * sizeof(LineVertex),
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


void RenderInputUI()
{
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
			ImGui::LabelText("Extra", "%d", MouseState->InWindow);

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
}
