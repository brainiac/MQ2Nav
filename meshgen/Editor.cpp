//
// Editor.cpp
//

#include "pch.h"
#include "Editor.h"

#include "meshgen/Application.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/ConsolePanel.h"
#include "meshgen/DebugPanel.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/PanelManager.h"
#include "meshgen/PropertiesPanel.h"
#include "meshgen/RenderManager.h"
#include "meshgen/SettingsDialog.h"
#include "meshgen/ToolsPanel.h"
#include "meshgen/ZoneContext.h"
#include "meshgen/ZonePicker.h"

#include "imgui/fonts/IconsLucide.h"
#include "imgui/fonts/IconsMaterialDesign.h"
#include "imgui/ImGuiUtils.h"
#include "imgui/scoped_helpers.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>


Editor::Editor()
{
}

Editor::~Editor()
{
}

void Editor::OnInit()
{
	m_panelManager = std::make_unique<PanelManager>();
	m_panelManager->AddPanel<PropertiesPanel>(this);
	m_panelManager->AddPanel<ConsolePanel>();
	m_panelManager->AddPanel<DebugPanel>(this);
	m_panelManager->AddPanel<ToolsPanel>(this);

	m_panelManager->AddDockingLayout({
		.splits = {
			{.initialDock = "MainDockSpace", .newDock = "LeftPane", .direction = ImGuiDir_Left, .ratio = .16f },
			{.initialDock = "MainDockSpace", .newDock = "RightPane", .direction = ImGuiDir_Right, .ratio = .16f },
			{.initialDock = "MainDockSpace", .newDock = "BottomPane", .direction = ImGuiDir_Down, .ratio = .25f }
		},
		.assignments = {
			{.panelName = "Properties", .dockName = "LeftPane", .open = true },
			{.panelName = "Console Log", .dockName = "BottomPane", .open = true },
			{.panelName = "Debug", .dockName = "BottomPane", .open = false },
			{.panelName = "Tools", .dockName = "RightPane", .open = true },
		},
		.layoutName = "Default",
		.isDefault = true,
	});

	m_panelManager->AddDockingLayout({
		.splits = {
			{.initialDock = "MainDockSpace", .newDock = "LeftPane", .direction = ImGuiDir_Left, .ratio = .16f },
			{.initialDock = "MainDockSpace", .newDock = "RightPane", .direction = ImGuiDir_Right, .ratio = .16f },
			{.initialDock = "MainDockSpace", .newDock = "BottomPane", .direction = ImGuiDir_Down, .ratio = .25f }
		},
		.assignments = {
			{.panelName = "Properties", .dockName = "LeftPane", .open = true },
			{.panelName = "Console Log", .dockName = "LeftPane", .open = true },
			{.panelName = "Debug", .dockName = "LeftPane", .open = false },
			{.panelName = "Tools", .dockName = "LeftPane", .open = true },
		},
		.layoutName = "Test Layout",
	});

	//auto& arguments = 
	//std::string startingZone;
	//if (argc > 1)
	//	startingZone = argv[1];

	//if (!startingZone.empty())
	//{
	//	// Defer loading this zone until we start processing events.
	//	m_backgroundTaskManager->PostToMainThread([this, startingZone]()
	//		{
	//			// Load the zone
	//			BeginLoadZone(startingZone, true);
	//		});
	//}

	m_meshTool = std::make_unique<NavMeshTool>();
	m_zonePicker = std::make_unique<ZonePicker>();
}

void Editor::OnShutdown()
{
	Halt();

	if (m_meshTool)
	{
		m_meshTool->GetRenderManager()->DestroyObjects();
	}

	m_panelManager->Shutdown();
	m_zonePicker.reset();
}

void Editor::OnUpdate(float timeStep)
{
	// Run simulation frames that have elapsed since the last iteration
	constexpr float SIM_RATE = 20;
	constexpr float DELTA_TIME = 1.0f / SIM_RATE;
	m_timeAccumulator = rcClamp(m_timeAccumulator + timeStep, -1.0f, 1.0f);
	int simIter = 0;
	while (m_timeAccumulator > DELTA_TIME)
	{
		m_timeAccumulator -= DELTA_TIME;
		if (simIter < 5 && m_meshTool)
			m_meshTool->handleUpdate(DELTA_TIME);
		simIter++;
	}

	UpdateCamera(timeStep);

	m_meshTool->handleRender(m_camera.GetViewProjMtx(), g_render->GetViewport());
}

void Editor::OnImGuiRender()
{
	m_panelManager->PrepareDockSpace();

	UI_UpdateViewport();

	UI_DrawMainMenuBar();
	UI_DrawToolbar();
	
	UI_DrawOverlayText();

	UI_DrawZonePicker();

	UI_DrawSettingsDialog();
	UI_DrawImportExportSettingsDialog();

	if (m_showDemo)
	{
		ImGui::ShowDemoWindow(&m_showDemo);
	}

	if (m_showMetricsWindow)
	{
		ImGui::ShowMetricsWindow(&m_showMetricsWindow);
	}

	m_panelManager->OnImGuiRender();
}


void Editor::OnKeyDown(const SDL_KeyboardEvent& event)
{
	if (event.keysym.sym == SDLK_ESCAPE)
	{
		if (m_meshTool->IsBuildingTiles())
			Halt();
		else if (m_zonePicker->IsShowing())
			m_zonePicker->Close();
	}

	if (ImGui::GetIO().WantTextInput)
		return;

	// Handle any key presses here.
	if (event.keysym.mod & KMOD_CTRL)
	{
		switch (event.keysym.sym)
		{
		case SDLK_o:
			m_zonePicker->Show();
			break;
		case SDLK_l:
			OpenMesh();
			break;
		case SDLK_s:
			SaveMesh();
			break;

		case SDLK_COMMA:
			m_showSettingsDialog = true;
			break;
		default:
			//printf("key: %d", event.key.keysym.sym);
			break;
		}
	}
}

void Editor::OnMouseDown(const SDL_MouseButtonEvent& event)
{
	auto& io = ImGui::GetIO();

	if (io.WantCaptureMouse)
		return;

	// Handle mouse clicks here.
	if (event.button == SDL_BUTTON_RIGHT)
	{
		// Clear keyboard focus
		ImGui::SetWindowFocus(nullptr);

		// Rotate view
		m_rotate = true;
		m_lastMouseLook.x = m_mousePos.x;
		m_lastMouseLook.y = m_mousePos.y;
	}
	else if (event.button == SDL_BUTTON_LEFT)
	{
		// Hit test mesh.
		if (m_zoneContext)
		{
			const glm::vec3& rayStart = g_render->GetCursorRayStart();
			const glm::vec3& rayEnd = g_render->GetCursorRayEnd();

			// Hit test mesh.
			if (float t; m_zoneContext->RaycastMesh(rayStart, rayEnd, t))
			{
				bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

				glm::vec3 pos = rayStart + (rayEnd - rayStart) * t;
				m_meshTool->handleClick(pos, shift);
			}
		}
	}
}

void Editor::OnMouseUp(const SDL_MouseButtonEvent& event)
{
	auto& io = ImGui::GetIO();

	if (io.WantCaptureMouse)
		return;

	// Handle mouse clicks here.
	if (event.button == SDL_BUTTON_RIGHT)
	{
		//SDL_SetRelativeMouseMode(SDL_FALSE);
		m_rotate = false;
	}
}

void Editor::OnMouseMotion(const SDL_MouseMotionEvent& event)
{
	g_render->SetMousePosition(glm::ivec2{ event.x, event.y });

	m_mousePos.x = event.x;
	m_mousePos.y = event.y;

	auto& io = ImGui::GetIO();

	if (io.WantCaptureMouse)
		return;

	if (m_rotate)
	{
		int dx = m_mousePos.x - m_lastMouseLook.x;
		int dy = m_mousePos.y - m_lastMouseLook.y;
		m_lastMouseLook = m_mousePos;
		m_lastMouseLook.y = m_mousePos.y;

		m_camera.UpdateMouseLook(dx, -dy);
	}
}

void Editor::OnWindowSizeChanged(const SDL_WindowEvent& event)
{
	m_windowSize = { event.data1, event.data2 };
}

void Editor::UI_UpdateViewport()
{
	ImGuiContext* ctx = ImGui::GetCurrentContext();

	if (ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(ctx, m_panelManager->GetMainDockSpaceID()))
	{
		ImGuiDockNode* centralNode = dockNode->CentralNode;

		m_camera.SetViewportSize(static_cast<uint32_t>(centralNode->Size.x), static_cast<uint32_t>(centralNode->Size.y));
		g_render->SetViewport({ centralNode->Pos.x, centralNode->Pos.y }, { centralNode->Size.x, centralNode->Size.y });
	}
	else
	{
		if (m_windowSize.x == 0 && m_windowSize.y == 0)
		{
			auto viewportSize = ImGui::GetContentRegionAvail();

			g_render->SetViewport({ 0, 0 }, { viewportSize.x, viewportSize.y });
		}
		else
		{
			g_render->SetViewport({ 0, 0 }, m_windowSize);
		}
	}
}

void Editor::UI_DrawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open Zone", "Ctrl+O", nullptr,
				!IsBlockingOperationActive()))
			{
				ShowZonePicker();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Settings", "Ctrl+,", nullptr))
			{
				m_showSettingsDialog = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit", "Alt+F4"))
				Application::Get().Quit();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Zone"))
		{
			bool canLoad = m_zoneContext && !m_zoneContext->IsBuildingNavMesh();
			if (ImGui::MenuItem("Load Mesh", "Ctrl+L", nullptr, canLoad))
			{
				OpenMesh();
			}

			bool canUseMesh = m_zoneContext && m_zoneContext->IsNavMeshLoaded() && !m_zoneContext->IsBuildingNavMesh();
			if (ImGui::MenuItem("Save Mesh", "Ctrl+S", nullptr, canUseMesh))
			{
				SaveMesh();
			}

			if (ImGui::MenuItem("Reset Mesh", "", nullptr, canUseMesh))
			{
				if (m_zoneContext)
					m_zoneContext->ResetNavMesh();

				m_meshTool->Reset();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Area Types...", nullptr, m_zoneContext != nullptr))
				m_showMapAreas = !m_showMapAreas;

			ImGui::Separator();

			if (ImGui::MenuItem("Export Settings", "", nullptr, canUseMesh))
			{
				ShowImportExportSettingsDialog(false);
			}

			if (ImGui::MenuItem("Import Settings", "", nullptr, canUseMesh))
			{
				ShowImportExportSettingsDialog(true);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			m_panelManager->DoWindowMenu();

			ImGui::Separator();

			if (ImGui::MenuItem("Show Shortcuts Overlay", nullptr, m_showOverlay))
				m_showOverlay = !m_showOverlay;

			ImGui::Separator();

			m_panelManager->DoLayoutsMenu();

			ImGui::Separator();

			ImGui::MenuItem("Show ImGui Demo", nullptr, &m_showDemo);
			ImGui::MenuItem("Show ImGui Metrics/Debugger", nullptr, &m_showMetricsWindow);

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

static void ToolbarIconText(const char* text, const char* tooltip = nullptr, ImFont* font = nullptr)
{
	float cursorY = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(3);

	ImGui::PushFont(font ? font : LCIconFontLarge);
	ImGui::Text("%s", text);
	ImGui::PopFont();

	if (tooltip)
		ImGui::SetItemTooltip("%s", tooltip);

	ImGui::SetCursorPosY(cursorY);
}

static bool ToolbarButton(const char* text, const char* tooltip = nullptr, bool disabled = false, ImFont* font = nullptr,
	bool selected = false)
{
	constexpr ImVec2 buttonSize = ImVec2(26, 26);

	mq::imgui::ScopedColorStack colorStack(
		ImGuiCol_Button, selected ? IM_COL32(66, 150, 250, 102) : IM_COL32(128, 128, 128, 64),
		ImGuiCol_ButtonHovered, selected ? IM_COL32(66, 150 , 250, 255) : IM_COL32(128, 128, 128, 224),
		ImGuiCol_ButtonActive, selected ? IM_COL32(66, 150, 250, 255) : IM_COL32(128, 128, 128, 128),
		ImGuiCol_Border, IM_COL32(192, 192, 192, 96),
		ImGuiCol_Separator, IM_COL32(192, 192, 192, 96)
	);

	ImGui::BeginDisabled(disabled);
	float cursorY = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(3);

	ImGui::PushFont(font ? font : LCIconFontLarge);
	bool pressed = ImGui::Button(text, buttonSize);
	ImGui::PopFont();

	if (tooltip)
		ImGui::SetItemTooltip("%s", tooltip);
	ImGui::EndDisabled();

	ImGui::SetCursorPosY(cursorY);

	return pressed;
}

static void ToolbarSeparator()
{
	float cursorY = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(3);

	ImGuiWindow* window = ImGui::GetCurrentWindow();

	// Vertical separator, for menu bars (use current line height).
	float y1 = 1;
	float y2 = window->Rect().Max.y - 1;
	const ImRect bb(ImVec2(window->DC.CursorPos.x, y1), ImVec2(window->DC.CursorPos.x + 1.0f, y2));
	ImGui::ItemSize(ImVec2(1.0, 0.0f));
	if (!ImGui::ItemAdd(bb, 0))
		return;

	// Draw
	window->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(192, 192, 192, 96));
	ImGui::SetCursorPosY(cursorY);
}

void Editor::UI_DrawToolbar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	float height = 32;

	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	
	bool draw = ImGui::BeginViewportSideBar("toolbar", viewport, ImGuiDir_Up, height, window_flags);
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (draw)
	{
		const float window_border_size = window->WindowBorderSize;
		window->DC.LayoutType = ImGuiLayoutType_Horizontal;
		window->DC.CursorPosPrevLine.y += 6;
		window->DC.CursorStartPos.y += 6;
		window->DC.CursorPos.y += 6;
		window->DC.IsSameLine = true;

		ImRect menu_bar_rect = window->Rect();
		menu_bar_rect.Max.y -= 1;
		window->DrawList->AddRectFilled(menu_bar_rect.Min + ImVec2(window_border_size, 0), menu_bar_rect.Max - ImVec2(window_border_size, 0),
			ImGui::GetColorU32(ImGuiCol_MenuBarBg), 0.0f, ImDrawFlags_None);
		
		window->DrawList->AddLine(menu_bar_rect.GetBL(), menu_bar_rect.GetBR(), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
	}

	ImGui::PopStyleVar(3);

	mq::imgui::ScopedStyleStack styleStack(
		ImGuiStyleVar_FramePadding, ImVec2(2, 2),
		ImGuiStyleVar_FrameRounding, 2.0f,
		ImGuiStyleVar_FrameBorderSize, 1.0f
	);

	bool isBlocked = IsBlockingOperationActive();

	if (draw)
	{
		ImGui::SetCursorPosX(8);
		ImGui::SetCursorPosY(6);

		{
			//
			// Basic Actions
			//

			// Open Zone
			if (ToolbarButton(ICON_MD_PUBLIC "##OpenZone", "Open Zone", isBlocked, mq::imgui::LargeTextFont))
			{
				ShowZonePicker();
			}

			// Settings
			if (ToolbarButton(ICON_LC_SETTINGS "##Settings2", "Settings"))
			{
				ShowSettingsDialog();
			}

			ToolbarSeparator();

			//
			// Mesh Actions
			//

			// Load Mesh
			if (ToolbarButton(ICON_LC_FOLDER_OPEN "##OpenMesh", "Open Mesh", isBlocked || !m_zoneContext))
			{
				OpenMesh();
			}

			// Save
			if (ToolbarButton(ICON_LC_SAVE "##SaveMesh", "Save Mesh", isBlocked || !m_zoneContext || !m_zoneContext->IsNavMeshLoaded()))
			{
				SaveMesh();
			}

			ToolbarSeparator();

			//
			// Tools
			//

			if (ToolbarButton(ICON_LC_MOUSE_POINTER_2 "##SelectTool", "Select", false, nullptr, m_interactMode == InteractMode::Select))
				m_interactMode = InteractMode::Select;
			ImGui::SameLine(0, 1);
			if (ToolbarButton(ICON_LC_MOVE "##MoveTool", "Translate", false, nullptr, m_interactMode == InteractMode::Move))
				m_interactMode = InteractMode::Move;
			ImGui::SameLine(0, 1);
			if (ToolbarButton(ICON_LC_ROTATE_3D "##RotateTool", "Rotate", false, nullptr, m_interactMode == InteractMode::Rotate))
				m_interactMode = InteractMode::Rotate;
			ImGui::SameLine(0, 1);
			if (ToolbarButton(ICON_LC_SCALE_3D "##ScaleTool", "Scale", false, nullptr, m_interactMode == InteractMode::Translate))
				m_interactMode = InteractMode::Translate;

			ToolbarSeparator();


			//
			// Camera Controls
			//

			// show zone name
			{
				if (!m_zoneContext || !m_zoneContext->IsZoneLoaded())
				{
					if (m_loadingZoneContext)
						ImGui::TextColored(ImColor(255, 255, 0), "Loading %s...", m_loadingZoneContext->GetShortName().c_str());
					else
						ImGui::TextColored(ImColor(255, 255, 255), "No zone loaded");
				}
				else
					ImGui::TextColored(ImColor(0, 255, 0), "%s", m_zoneContext->GetShortName().c_str());
			}

			// Current Position
			ToolbarIconText(ICON_MD_EXPLORE, "Camera position", mq::imgui::LargeTextFont);
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));

				glm::vec3 cameraPos = to_eq_coord(m_camera.GetPosition());
				ImGui::SetNextItemWidth(240.0f);
				if (ImGui::DragFloat3("##Position", glm::value_ptr(cameraPos), 0.01f))
				{
					m_camera.SetPosition(from_eq_coord(cameraPos));
				}

				ImGui::PopStyleVar();
			}
		}

		ImGui::End();
	}
}

void Editor::UI_DrawZonePicker()
{
	if (m_zonePicker->Draw())
	{
		OpenZone(m_zonePicker->GetSelectedZone(), m_zonePicker->ShouldLoadNavMesh());
	}
}

void Editor::UI_DrawSettingsDialog()
{
	if (m_showSettingsDialog)
	{
		ImGui::OpenPopup("Settings");
		m_showSettingsDialog = false;
	}

	if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("EQ Path");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##EQPath", (char*)g_config.GetEverquestPath().c_str(),
			g_config.GetEverquestPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##EverquestPath", ImVec2(120, 0)))
			g_config.SelectEverquestPath();
		ImGui::PopItemWidth();

		ImGui::Text("Navmesh Path (Path to MacroQuest root directory)");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##NavmeshPath", (char*)g_config.GetMacroQuestPath().c_str(), g_config.GetMacroQuestPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##OutputPath", ImVec2(120, 0)))
		{
			g_config.SelectMacroQuestPath();
		}
		ImGui::PopItemWidth();

		bool useExtents = g_config.GetUseMaxExtents();
		if (ImGui::Checkbox("Apply max extents to zones with far away geometry", &useExtents))
			g_config.SetUseMaxExtents(useExtents);
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Some zones have geometry that is really far away, either\n"
				"horizontally or vertically. Some zones can be configured to strip\n"
				"out this far away geometry in code. See NavMeshData.cpp to add your\n"
				"own. Disable this feature if it is causing issues with mesh generation.\n\n"
				"The zone will need to be reloaded to apply this change");
			ImGui::EndTooltip();
		}
		ImGui::Separator();

		if (ImGui::Button("Close", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void Editor::UI_DrawOverlayText()
{
	if (m_showOverlay)
	{
		ImGuiContext* ctx = ImGui::GetCurrentContext();
		ImVec2 Corner;

		if (ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(ctx, m_panelManager->GetMainDockSpaceID()))
		{
			ImGuiDockNode* centralNode = dockNode->CentralNode;

			Corner = ImVec2(centralNode->Pos.x + centralNode->Size.x,
				centralNode->Pos.y + centralNode->Size.y);
		}
		else
		{
			Corner = ImGui::GetIO().DisplaySize;
		}

		ImGui::SetNextWindowPos(
			ImVec2(Corner.x - 10.0f, Corner.y - 10.0f),
			ImGuiCond_Always,
			ImVec2(1.0, 1.0));
		ImGui::SetNextWindowBgAlpha(0.3f);

		if (ImGui::Begin("##Overlay", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			m_meshTool->handleRenderOverlay();

			// Help text.
			static const char msg[] = "Hotkeys:\n* W/S/A/D: Move\n"
				"* Space/C: Move up/down\n"
				"* Shift+Move: Increase movement speed\n"
				"* RMB: Look around";

			ImGui::TextColored(ImColor(255, 255, 255, 128), msg);
		}
		ImGui::End();
	}
}

void Editor::UI_DrawImportExportSettingsDialog()
{
	if (m_importExportSettings && !m_zoneContext)
	{
		m_importExportSettings.reset();
	}

	if (m_importExportSettings)
	{
		bool show = true;
		m_importExportSettings->Show(&show);
		if (!show)
			m_importExportSettings.reset();
	}
}

static bool RenderAreaType(NavMesh* navMesh, const PolyAreaType& area, int userIndex = -1)
{
	uint8_t areaId = area.id;

	bool builtIn = !IsUserDefinedPolyArea(areaId);
	ImColor col(area.color);
	int32_t flags32 = area.flags;
	float cost = area.cost;
	std::string newName;
	bool remove = false;

	ImGui::PushID(areaId);

	bool changed = false;

	if (areaId != 0) // no color for 0
	{
		changed |= ImGui::ColorEdit4("##color", &col.Value.x,
			ImGuiColorEditFlags_NoInputs
			| ImGuiColorEditFlags_NoTooltip
			| ImGuiColorEditFlags_NoOptions
			| ImGuiColorEditFlags_NoDragDrop);
	}

	ImGui::NextColumn();
	ImGui::SetColumnOffset(1, 35);

	if (builtIn)
		ImGui::Text("Built-in %d", areaId);
	else
		ImGui::Text("User %d", userIndex != -1 ? userIndex : areaId);

	ImGui::NextColumn();
	ImGui::SetColumnOffset(2, 120);

	char name[256];
	strcpy_s(name, area.name.c_str());

	ImGui::PushItemWidth(200);
	bool useNewName;

	if (builtIn)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));
		useNewName = ImGui::InputText("##name", name, 256, ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
	}
	else
	{
		useNewName = ImGui::InputText("##name", name, 256, 0);
	}

	ImGui::PopItemWidth();

	if (useNewName)
	{
		newName = name;
		changed = true;
	}

	ImGui::NextColumn();
	ImGui::SetColumnOffset(3, 325);

	ImGui::PushItemWidth(45);

	if (areaId == 0)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));
		changed |= ImGui::InputFloat("##cost", &cost, 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
	}
	else
	{
		changed |= ImGui::InputFloat("##cost", &cost, 0.0f, 0.0f, "%.1f");
	}

	ImGui::PopItemWidth();

	if (changed)
	{
		PolyAreaType areaType{
			areaId,
			useNewName ? newName : area.name,
			ImU32(col),
			uint16_t(flags32),
			cost,
			true,
			true,
		};
		navMesh->UpdateArea(areaType);
	}

	ImGui::NextColumn();
	ImGui::SetColumnOffset(4, 370);

	if (!builtIn)
	{
		if (ImGui::Button("  -  "))
		{
			remove = true;
		}
	}
	else
	{
		auto iter = std::find_if(DefaultPolyAreas.begin(), DefaultPolyAreas.end(),
			[areaId](const PolyAreaType& def) { return def.id == areaId; });
		if (iter != DefaultPolyAreas.end())
		{
			const auto& defArea = *iter;
			if (defArea.color != col || defArea.cost != cost)
			{
				if (ImGui::Button("Revert"))
				{
					navMesh->UpdateArea(defArea);
				}
			}
		}
	}

	ImGui::NextColumn();
	ImGui::PopID();

	return remove;
}

void Editor::UI_DrawAreaTypesEditor()
{
	if (m_showMapAreas && m_zoneContext)
	{
		ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_Once);

		if (ImGui::Begin("Area Types", &m_showMapAreas))
		{
			auto navMesh = m_zoneContext->GetNavMesh();

			ImGui::Columns(5, 0, false);

			RenderAreaType(navMesh.get(), navMesh->GetPolyArea((uint8_t)PolyArea::Unwalkable));
			RenderAreaType(navMesh.get(), navMesh->GetPolyArea((uint8_t)PolyArea::Ground));
			//RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Jump));
			RenderAreaType(navMesh.get(), navMesh->GetPolyArea((uint8_t)PolyArea::Water));
			RenderAreaType(navMesh.get(), navMesh->GetPolyArea((uint8_t)PolyArea::Prefer));
			RenderAreaType(navMesh.get(), navMesh->GetPolyArea((uint8_t)PolyArea::Avoid));

			ImGui::Separator();

			for (uint8_t index = (uint8_t)PolyArea::UserDefinedFirst;
				index <= (uint8_t)PolyArea::UserDefinedLast; ++index)
			{
				const auto& area = navMesh->GetPolyArea(index);
				if (area.valid)
				{
					if (RenderAreaType(navMesh.get(), area, area.id - (uint8_t)PolyArea::UserDefinedFirst))
						navMesh->RemoveUserDefinedArea(area.id);
				}
			}

			uint8_t nextAreaId = navMesh->GetFirstUnusedUserDefinedArea();
			if (nextAreaId != 0)
			{
				if (ImGui::Button(" + "))
				{
					auto area = navMesh->GetPolyArea(nextAreaId);
					navMesh->UpdateArea(area);
				}
			}

			ImGui::Columns(1);
		}

		ImGui::End();
	}
}


bool Editor::IsBlockingOperationActive()
{
	return m_meshTool && m_meshTool->IsBuildingTiles();
}

void Editor::ShowNotificationDialog(const std::string& title, const std::string& message, bool modal)
{
	m_panelManager->ShowNotificationDialog(title, message, modal);
}

void Editor::ShowZonePicker()
{
	m_zonePicker->Show();
}

void Editor::ShowSettingsDialog()
{
	m_showSettingsDialog = true;
}

void Editor::ShowImportExportSettingsDialog(bool import)
{
	if (m_zoneContext)
	{
		m_importExportSettings = std::make_unique<ImportExportSettingsDialog>(m_zoneContext->GetNavMesh(), import);
	}
}

void Editor::SetZoneContext(const std::shared_ptr<ZoneContext>& zoneContext)
{
	if (m_zoneContext == zoneContext)
		return;

	m_zoneContext = zoneContext;

	if (zoneContext == nullptr)
	{
		m_meshTool->SetZoneContext(nullptr);
		m_panelManager->SetZoneContext(zoneContext);
	}
	else
	{
		// Update the window title
		std::string windowTitle = fmt::format("MacroQuest NavMesh Generator - {}",
			m_zoneContext->GetDisplayName());
		Application::Get().SetWindowTitle(windowTitle);

		m_meshTool->SetZoneContext(m_zoneContext);
		m_panelManager->SetZoneContext(m_zoneContext);

		ResetCamera();
	}
}

void Editor::OpenZone(const std::string& shortZoneName, bool loadMesh)
{
	// FIXME
	Application::Get().GetBackgroundTaskManager().StopZoneTasks();

	Halt();

	std::shared_ptr<ZoneContext> zoneContext = std::make_shared<ZoneContext>(this, shortZoneName);
	m_loadingZoneContext = zoneContext;

	auto finishLoading = [this, zoneContext](bool success, bool clearLoading)
		{
			if (success)
			{
				SetZoneContext(zoneContext);
			}

			if (clearLoading && m_loadingZoneContext == zoneContext)
			{
				m_loadingZoneContext.reset();
			}
		};

	tf::Taskflow loadFlow = zoneContext->BuildInitialTaskflow(loadMesh,
		[this, zoneContext, finishLoading](LoadingPhase phase, bool result)
		{
			Application::Get().GetBackgroundTaskManager().PostToMainThread(
				[app = this, zoneContext, phase, result, finishLoading]()
				{
					if (phase == LoadingPhase::BuildTriangleMesh && result)
					{
						finishLoading(true, false);
					}
					else
					{
						if (result)
						{
							finishLoading(true, true);
						}
						else
						{
							ResultState rs = zoneContext->GetResultState();

							if (rs.phase == LoadingPhase::LoadZone || rs.phase == LoadingPhase::BuildTriangleMesh)
								app->ShowNotificationDialog("Failed to load zone", rs.message);
							else
								app->ShowNotificationDialog("Failed to build navigation mesh", rs.message);

							finishLoading(false, true);
						}
					}
				});
		});

	// Log the taskflow for debugging
	//std::stringstream ss;
	//taskflow.dump(ss);
	//bx::debugPrintf("Taskflow: %s", ss.str().c_str());

	Application::Get().GetBackgroundTaskManager().AddZoneTask(std::move(loadFlow));
}

void Editor::OpenMesh()
{
	if (m_zoneContext)
	{
		m_zoneContext->LoadNavMesh();
	}
}

void Editor::SaveMesh()
{
	if (m_zoneContext)
	{
		m_zoneContext->SaveNavMesh();
	}
}

void Editor::Halt()
{
	if (m_meshTool)
	{
		m_meshTool->CancelBuildAllTiles();
	}
}

void Editor::ResetCamera()
{
	if (m_zoneContext)
	{
		const glm::vec3& bmin = m_zoneContext->GetMeshBoundsMin();
		const glm::vec3& bmax = m_zoneContext->GetMeshBoundsMax();

		// Reset camera and fog to match the mesh bounds.
		float camr = glm::distance(bmax, bmin) / 2;

		m_camera.SetFarClipPlane(camr * 3);

		glm::vec3 cam = {
			(bmax[0] + bmin[0]) / 2 + camr,
			(bmax[1] + bmin[1]) / 2 + camr,
			(bmax[2] + bmin[2]) / 2 + camr
		};

		m_camera.SetPosition(cam);
		m_camera.SetYaw(-45);
		m_camera.SetPitch(45);
	}
}

void Editor::UpdateCamera(float timeStep)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		m_camera.ClearKeyState();
	}
	else
	{
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		m_camera.SetKeyState(CameraKey_Forward, keyState[SDL_SCANCODE_W] != 0);
		m_camera.SetKeyState(CameraKey_Backward, keyState[SDL_SCANCODE_S] != 0);
		m_camera.SetKeyState(CameraKey_Left, keyState[SDL_SCANCODE_A] != 0);
		m_camera.SetKeyState(CameraKey_Right, keyState[SDL_SCANCODE_D] != 0);
		m_camera.SetKeyState(CameraKey_Up, keyState[SDL_SCANCODE_SPACE] != 0);
		m_camera.SetKeyState(CameraKey_Down, keyState[SDL_SCANCODE_C] != 0);
		m_camera.SetKeyState(CameraKey_SpeedUp, (SDL_GetModState() & KMOD_SHIFT) != 0);
	}

	m_camera.Update(timeStep);
}