//
// Editor.cpp
//

#include "pch.h"
#include "Editor.h"

#include "common/MathUtil.h"
#include "meshgen/Application.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/ConsolePanel.h"
#include "meshgen/DebugPanel.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/PanelManager.h"
#include "meshgen/PropertiesPanel.h"
#include "meshgen/RenderManager.h"
#include "meshgen/Scene.h"
#include "meshgen/SelectionManager.h"
#include "meshgen/SettingsDialog.h"
#include "meshgen/ToolsPanel.h"
#include "meshgen/ZoneCollisionMesh.h"
#include "meshgen/ZonePicker.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneRenderManager.h"

#include "imgui/fonts/IconsLucide.h"
#include "imgui/fonts/IconsMaterialDesign.h"
#include "imgui/ImGuiUtils.h"
#include "imgui/scoped_helpers.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_stacklayout.h"

#include <glm/gtc/type_ptr.hpp>

#include "ZoneResourceManager.h"


Editor::Editor()
{
}

Editor::~Editor()
{
}

void Editor::OnInit()
{
	m_selectionMgr = std::make_unique<SelectionManager>();

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


	m_meshTool = std::make_unique<NavMeshTool>();
	m_zonePicker = std::make_unique<ZonePicker>();

	// Parse arguments and see if we have something to load right away
	const auto& arguments = Application::Get().GetArgs();
	if (arguments.size() > 1)
	{
		std::string startingZone = arguments[1];

		if (!startingZone.empty())
		{
			// Open new project with this starting zone
			OpenProject(startingZone, true);
		}
	}
	else
	{
		CreateEmptyProject();
	}
}

void Editor::OnShutdown()
{
	CloseProject();

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

	m_project->OnUpdate(timeStep);

	m_meshTool->handleRender(m_camera.GetViewProjMtx(), g_render->GetViewport());
}

void Editor::OnImGuiRender()
{
	m_panelManager->PrepareDockSpace();

	UI_UpdateViewport();

	UI_DrawMainMenuBar();
	UI_DrawToolbar();
	UI_DrawStatusBar();
	
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
		if (m_project && m_project->IsBusy())
		{
			m_project->CancelTasks();
		}

		if (m_zonePicker->IsShowing())
		{
			m_zonePicker->Close();
		}
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
			m_project->LoadNavMesh();
			break;
		case SDLK_s:
			m_project->SaveNavMesh();
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
		// Hit test zone geo mesh.
		if (m_project->IsZoneLoaded())
		{
			auto [rayStart, rayEnd] = CastRay(static_cast<float>(m_viewportMouse.x), static_cast<float>(m_viewportMouse.y));

			// Hit test mesh.
			if (float t; m_project->RaycastMesh(rayStart, rayEnd, t))
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
	m_mousePos.x = event.x;
	m_mousePos.y = event.y;

	// Update mouse position inside viewport
	{
		int mouseX = m_mousePos.x;
		int mouseY = m_mousePos.y - m_viewport.y;

		mouseY = (m_viewport.w - 1 - mouseY) + m_viewport.y;

		if (mouseY < 0)
			mouseY = 0;

		m_viewportMouse = { mouseX, mouseY };
	}

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
}

void Editor::UI_UpdateViewport()
{
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	ImVec2 viewportPos = ImGui::GetWindowContentRegionMin();
	ImVec2 viewportSize = ImGui::GetWindowContentRegionMax();

	if (ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(ctx, m_panelManager->GetMainDockSpaceID()))
	{
		ImGuiDockNode* centralNode = dockNode->CentralNode;

		viewportPos = centralNode->Pos;
		viewportSize = centralNode->Size;
	}

	m_viewport = { viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y };

	m_camera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
	g_render->SetViewport({ viewportPos.x, viewportPos.y }, { viewportSize.x, viewportSize.y });
}

void Editor::UI_DrawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open Zone", "Ctrl+O", nullptr,
				!m_project->IsBusy()))
			{
				ShowZonePicker();
			}
			if (ImGui::MenuItem("Close Zone", nullptr, nullptr,
				!m_project->IsBusy() && m_project->IsZoneLoaded()))
			{
				CreateEmptyProject();
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
			bool canLoad = !m_project->IsBusy();
			if (ImGui::MenuItem("Load Mesh", "Ctrl+L", nullptr, canLoad))
			{
				m_project->LoadNavMesh();
			}

			bool canUseMesh = !m_project->IsBusy() && m_project->IsNavMeshReady();
			if (ImGui::MenuItem("Save Mesh", "Ctrl+S", nullptr, canUseMesh))
			{
				m_project->SaveNavMesh();
			}


			if (ImGui::MenuItem("Reset Mesh", "", nullptr, canUseMesh))
			{
				m_project->ResetNavMesh();
				m_meshTool->Reset();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Area Types...", nullptr, m_project->IsNavMeshReady()))
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

	mq::imgui::ScopedStyleStack styleStack(
		ImGuiStyleVar_FramePadding, ImVec2(2, 2)
	);

	ImGui::BeginDisabled(disabled);
	float cursorY = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(3);

	bool pressed;

	{
		mq::imgui::ScopedColorStack colorStack2(
			ImGuiCol_Text, (selected && !disabled) ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255)
		);

		ImGui::PushFont(font ? font : LCIconFontLarge);
		pressed = ImGui::Button(text, buttonSize);
		ImGui::PopFont();
	}

	if (tooltip)
		ImGui::SetItemTooltip("%s", tooltip);
	ImGui::EndDisabled();

	ImGui::SetCursorPosY(cursorY);

	return pressed;
}

static void ToolbarSeparator(int adj = 0)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	// Vertical separator, for menu bars (use current line height).
	float y1 = window->Rect().Min.y + adj;
	float y2 = window->Rect().Max.y - 1 + adj;
	const ImRect bb(ImVec2(window->DC.CursorPos.x + 4, y1), ImVec2(window->DC.CursorPos.x + 5.0f, y2));
	ImGui::ItemSize(ImVec2(9.0, 1.0f));

	// Draw
	window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Separator));
}

void Editor::UI_DrawToolbar()
{
	// Set up the area we will use for the toolbar
	{
		mq::imgui::ScopedStyleStack window_style_stack(
			ImGuiStyleVar_WindowBorderSize, 1.0f,
			ImGuiStyleVar_WindowPadding, ImVec2(0, 0),
			ImGuiStyleVar_FrameBorderSize, 1.0f
		);
		mq::imgui::ScopedColorStack window_color_stack(
			ImGuiCol_WindowBg, ImGui::GetColorU32(ImGuiCol_MenuBarBg)
		);

		ImGuiWindowFlags window_flags = 0
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoSavedSettings
			;

		float height = 32;
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		if (!ImGui::BeginViewportSideBar("##Toolbar", viewport, ImGuiDir_Up, height, window_flags))
		{
			ImGui::End();
			return;
		}

		ImGui::BeginHorizontal("##ToolbarHorizontal", ImVec2(ImGui::GetContentRegionAvail().x, height), 0.5f);

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const float window_border_size = window->WindowBorderSize;

		ImRect menu_bar_rect = window->Rect();
		menu_bar_rect.Max.y -= 1;
		//window->DrawList->AddRectFilled(menu_bar_rect.Min + ImVec2(window_border_size, 0), menu_bar_rect.Max - ImVec2(window_border_size, 0),
		//	ImGui::GetColorU32(ImGuiCol_MenuBarBg), 0.0f, ImDrawFlags_None);
		
		window->DrawList->AddLine(menu_bar_rect.GetBL(), menu_bar_rect.GetBR(), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
	}

	mq::imgui::ScopedStyleStack styleStack2(
		ImGuiStyleVar_FramePadding, ImVec2(4, 4),
		ImGuiStyleVar_FrameRounding, 2.0f,
		ImGuiStyleVar_FrameBorderSize, 1.0f
	);

	bool isBlocked = m_project->IsBusy();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
	ImGui::Spring(0);

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
		if (ToolbarButton(ICON_LC_SETTINGS "##Settings", "Settings"))
		{
			ShowSettingsDialog();
		}

		ToolbarSeparator();

		//
		// Mesh Actions
		//

		// Load Mesh
		if (ToolbarButton(ICON_LC_FOLDER_OPEN "##OpenMesh", "Open Mesh", isBlocked || !m_project->IsZoneLoaded()))
		{
			m_project->LoadNavMesh();
		}

		// Save
		if (ToolbarButton(ICON_LC_SAVE "##SaveMesh", "Save Mesh", isBlocked || !m_project->IsZoneLoaded() || !m_project->IsNavMeshReady()))
		{
			m_project->SaveNavMesh();
		}

		ToolbarSeparator();

		//
		// Tools
		//

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 0));

		if (ToolbarButton(ICON_LC_MOUSE_POINTER_2 "##SelectTool", "Select", false, nullptr, m_interactMode == InteractMode::Select))
			m_interactMode = InteractMode::Select;
		if (ToolbarButton(ICON_LC_MOVE "##MoveTool", "Translate", false, nullptr, m_interactMode == InteractMode::Move))
			m_interactMode = InteractMode::Move;
		if (ToolbarButton(ICON_LC_ROTATE_3D "##RotateTool", "Rotate", false, nullptr, m_interactMode == InteractMode::Rotate))
			m_interactMode = InteractMode::Rotate;
		if (ToolbarButton(ICON_LC_SCALE_3D "##ScaleTool", "Scale", false, nullptr, m_interactMode == InteractMode::Translate))
			m_interactMode = InteractMode::Translate;

		ImGui::PopStyleVar();

		ToolbarSeparator();


		//
		// Camera Controls
		//

		ImGui::Spring(0.25f);

		// Current Position
		ToolbarIconText(ICON_MD_EXPLORE, "Camera position", mq::imgui::LargeTextFont);
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));

			ImGui::SuspendLayout();

			glm::vec3 cameraPos = to_eq_coord(m_camera.GetPosition());
			ImGui::SetNextItemWidth(240.0f);
			if (ImGui::DragFloat3("##Position", glm::value_ptr(cameraPos), 0.01f))
			{
				m_camera.SetPosition(from_eq_coord(cameraPos));
			}

			ImGui::ResumeLayout();

			ImGui::PopStyleVar();
		}

		ImGui::Spring(0.75f);
	}

	ImGui::Spring(0);
	ImGui::PopStyleVar(); // ItemSpacing

	ImGui::EndHorizontal();
	ImGui::End();
}

void Editor::UI_DrawStatusBar()
{
	// Set up the area we will use for the statusbar

	mq::imgui::ScopedStyleStack window_style_stack(
		ImGuiStyleVar_WindowBorderSize, 0.0f,
		ImGuiStyleVar_WindowPadding, ImVec2(4, 4),
		ImGuiStyleVar_FrameBorderSize, 1.0f,
		ImGuiStyleVar_FramePadding, ImVec2(4, 4),
		ImGuiStyleVar_ItemSpacing, ImVec2(4, 0)
	);

	constexpr uint32_t green = IM_COL32(14, 42, 17, 228);
	constexpr uint32_t blue = IM_COL32(20, 26, 54, 240);

	mq::imgui::ScopedColorStack window_color_stack(
		ImGuiCol_WindowBg, m_project->IsZoneLoaded() ? green : blue
	);

	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		;

	float height = 24;
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	if (!ImGui::BeginViewportSideBar("##Statusbar", viewport, ImGuiDir_Down, height, window_flags))
	{
		ImGui::End();
		return;
	}

	ImGui::BeginHorizontal("##StatusbarHorizontal", ImGui::GetContentRegionAvail(), 0.5f);
	ImGui::Spring(0);

	// Draw border line above
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	ImRect menu_bar_rect = window->Rect();
	window->DrawList->AddLine(menu_bar_rect.GetTL(), menu_bar_rect.GetTR(), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);

	{
		// show zone name / stats
		{

			if (m_project->IsZoneLoading())
				ImGui::Text("Loading %s...", m_project->GetShortName().c_str());
			else if (!m_project->IsZoneLoaded())
				ImGui::TextColored(ImColor(255, 255, 255), "No zone loaded (Ctrl+O to select zone)");
			else
				ImGui::TextColored(ImColor(0, 255, 0), "%s (%s)", m_project->GetLongName().c_str(), m_project->GetShortName().c_str());

			if (m_project->IsZoneLoaded())
			{
				ToolbarSeparator(1);

				auto* resourceManager = m_project->GetResourceManager();
				if (resourceManager->HasDynamicObjects())
					ImGui::Text("%d zone objects loaded", resourceManager->GetDynamicObjectsCount());
				else
				{
					ImGui::TextColored(ImColor(255, 255, 0), "No zone objects loaded");

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Dynamic objects can be loaded after entering\n"
							"a zone in EQ with MQ2Nav loaded. Re-open the zone\n"
							"to refresh the dynamic objects.");
						ImGui::EndTooltip();
					}
				}

				ToolbarSeparator(1);

				auto collisionMesh = m_project->GetCollisionMesh();
				ImGui::TextColored(ImColor(127, 127, 127), "Collision Mesh Verts: %.1fk Tris: %.1fk",
					collisionMesh->getVertCount() / 1000.0f, collisionMesh->getTriCount() / 1000.0f);
			}
		}

		ImGui::Spring(1.0f);
		ToolbarSeparator(1);

		ImGui::Dummy(ImVec2(80, 0));
		ImVec2 pos = ImGui::GetCursorPos();

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetCursorPos(pos - ImVec2(80, 0));

		ImGui::Text("%.1f FPS", io.Framerate);
	}

	ImGui::Spring(0);

	ImGui::EndHorizontal();
	ImGui::End();
}

void Editor::UI_DrawZonePicker()
{
	if (m_zonePicker->Draw())
	{
		OpenProject(m_zonePicker->GetSelectedZone(), m_zonePicker->ShouldLoadNavMesh());
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
	if (m_importExportSettings && !m_project->IsNavMeshReady())
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
	if (m_showMapAreas && m_project->IsNavMeshReady())
	{
		ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_Once);

		if (ImGui::Begin("Area Types", &m_showMapAreas))
		{
			auto navMesh = m_project->GetNavMesh();

			ImGui::Columns(5, "##AreaTypes", false);

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
	if (auto navMesh = m_project->GetNavMesh())
	{
		m_importExportSettings = std::make_unique<ImportExportSettingsDialog>(navMesh, import);
	}
}

void Editor::SetProject(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_project = zoneProject;

	// Update the window title
	std::string windowTitle = fmt::format("MacroQuest NavMesh Generator - {}", m_project->GetDisplayName());
	Application::Get().SetWindowTitle(windowTitle);
	m_selectionMgr->Clear();

	m_panelManager->OnProjectChanged(m_project);
	m_meshTool->OnProjectChanged(m_project);

	SetNavMeshProject(m_project->GetNavMeshProject());
}

void Editor::SetNavMeshProject(const std::shared_ptr<NavMeshProject>& navMeshProject)
{
	m_panelManager->OnNavMeshProjectChanged(navMeshProject);
	m_meshTool->OnNavMeshProjectChanged(navMeshProject);
}

void Editor::CreateEmptyProject()
{
	if (m_project)
	{
		CloseProject();
	}

	SetProject(std::make_shared<ZoneProject>(this, "No Zone"));
	m_camera = Camera();
}

void Editor::OpenProject(const std::string& name, bool loadMesh)
{
	CloseProject();

	m_project = std::make_shared<ZoneProject>(this, name);
	m_project->LoadZone(loadMesh);
}

void Editor::CloseProject()
{
	// Save?

	SetNavMeshProject(nullptr);

	m_selectionMgr->Clear();

	m_panelManager->OnProjectChanged(nullptr);
	m_meshTool->OnProjectChanged(nullptr);

	if (m_project)
	{
		m_project->OnShutdown();
	}

	m_project.reset();
}

void Editor::OnProjectLoaded(const std::shared_ptr<ZoneProject>& project)
{
	SetProject(project);

	// Reset the camera
	const glm::vec3& bmin = m_project->GetMeshBoundsMin();
	const glm::vec3& bmax = m_project->GetMeshBoundsMax();

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

std::tuple<glm::vec3, glm::vec3> Editor::CastRay(float mx, float my)
{
	auto& viewMtx = m_camera.GetViewMtx();
	auto& projMtx = m_camera.GetProjMtx();

	glm::vec3 rayStart = glm::unProject(glm::vec3(mx, my, 0.0f), viewMtx, projMtx, m_viewport);
	glm::vec3 rayEnd = glm::unProject(glm::vec3(mx, my, 1.0f), viewMtx, projMtx, m_viewport);

	return { rayStart, rayEnd };
}
