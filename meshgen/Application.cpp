//
// Application.cpp
//

#include "meshgen/Application.h"
#include "meshgen/InputGeom.h"
#include "meshgen/Logging.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/RenderManager.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZonePicker.h"
#include "common/Utilities.h"

#include "imgui/ImGuiUtils.h"
#include "imgui/imgui_impl.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui_internal.h"
#include <imgui.h>

#include <bx/math.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <SDL2/SDL.h>

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

//============================================================================

Application::Application()
{
	Logging::Initialize();
	m_config.Initialize();
	Logging::InitSecondStage(this);

	SPDLOG_INFO("EverQuest Path: {}", m_config.GetEverquestPath());
	SPDLOG_INFO("MacroQuest Path: {} IsValid={}", m_config.GetMacroQuestPath(), m_config.IsMacroQuestPathValid());
	SPDLOG_INFO("Config Path: {}", m_config.GetConfigPath());
	SPDLOG_INFO("Resources Path: {}", m_config.GetResourcesPath());
	SPDLOG_INFO("Logging Path: {}", m_config.GetLogsPath());
	SPDLOG_INFO("Output Path: {}", m_config.GetOutputPath());

	g_render = new RenderManager();
	g_resourceMgr = new ResourceManager();
}

Application::~Application()
{
	delete g_resourceMgr;
	delete g_render;

	Logging::Shutdown();
}

bool Application::Initialize(int32_t argc, const char* const* argv)
{
	if (!InitSystem())
		return false;

	std::string startingZone;
	if (argc > 1)
		startingZone = argv[1];

	if (!startingZone.empty())
	{
		m_loadMeshOnZone = true;
		m_nextZoneToLoad = startingZone;
	}

	m_navMesh = std::make_shared<NavMesh>(m_config.GetOutputPath(), startingZone);
	m_meshTool = std::make_unique<NavMeshTool>(m_navMesh);

	m_rcContext = std::make_unique<RecastContext>();

	m_meshTool->setContext(m_rcContext.get());
	m_meshTool->setOutputPath(m_config.GetOutputPath().c_str());

	return true;
}

bool Application::InitSystem()
{
	SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER);

	if (!m_config.GetSavedWindowDimensions(m_windowRect))
	{
		m_windowRect = {
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1600,
			900
		};
	}
	else
	{
		SPDLOG_INFO("Loaded persisted window rect: {},{},{},{}",
			m_windowRect.x, m_windowRect.y, m_windowRect.z, m_windowRect.w);
	}

	// Setup window
	SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	m_window = SDL_CreateWindow("MacroQuest NavMesh Generator", m_windowRect.x, m_windowRect.y, m_windowRect.z, m_windowRect.w, window_flags);
	if (m_window == nullptr)
	{
		char szMessage[256];
		sprintf_s(szMessage, "Error: SDL_CreateWindow(): %s", SDL_GetError());

		::MessageBoxA(nullptr, szMessage, "Error Starting Engine", MB_OK | MB_ICONERROR);
		return false;
	}

	//SDL_GetWindowPosition(m_window, &x, &y);

	if (!g_render->Initialize(m_windowRect.z, m_windowRect.w, m_window))
		return false;
	if (!g_resourceMgr->Initialize())
		return false;

	InitImGui();

	m_lastTime = SDL_GetTicks64();
	m_time = 0.0f;

	return true;
}

void Application::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGuiContext* context = ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	// this requires permanent storage
	m_iniFile = fmt::format("{}/config/MeshGenerator_UI.ini", m_config.GetConfigPath());
	io.IniFilename = m_iniFile.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_Impl_Init(m_window, context);

	mq::imgui::ConfigureFonts(io.Fonts);
	mq::imgui::ConfigureStyle();
}

int Application::Shutdown()
{
	Halt();

	if (m_meshTool)
		m_meshTool->GetRenderManager()->DestroyObjects();

	m_zonePicker.reset();
	m_geom.reset();

	g_resourceMgr->Shutdown();
	g_render->Shutdown();
	return 0;
}

bool Application::update()
{
	if (m_done)
		return false;

	// fractional time since last update
	//std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();
	//auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(time - m_lastTime).count();
	uint64_t time = SDL_GetTicks64();
	m_timeDelta = (time / m_lastTime) / 1000.0f;
	m_lastTime = time;
	m_time += m_timeDelta;

	if (!HandleCallbacks())
		return false;

	if (!HandleEvents())
		return false;

	// Run simulation frames that have elapsed since the last iteration
	constexpr float SIM_RATE = 20;
	constexpr float DELTA_TIME = 1.0f / SIM_RATE;
	m_timeAccumulator = rcClamp(m_timeAccumulator + m_timeDelta, -1.0f, 1.0f);
	int simIter = 0;
	while (m_timeAccumulator > DELTA_TIME)
	{
		m_timeAccumulator -= DELTA_TIME;
		if (simIter < 5 && m_meshTool)
			m_meshTool->handleUpdate(DELTA_TIME);
		simIter++;
	}

	UpdateCamera();

	g_render->BeginFrame(m_timeDelta);

	UpdateImGui();

	UpdateViewport();

	// TODO: Can push these down into the mesh tool
	Camera* camera = g_render->GetCamera();
	const glm::mat4& viewProjMtx = camera->GetViewProjMtx();

	m_meshTool->handleRender(viewProjMtx, g_render->GetViewport());

	// Load a zone if a zone load was requested
	if (!m_nextZoneToLoad.empty())
	{
		PushEvent([zone = m_nextZoneToLoad, loadMesh = m_loadMeshOnZone, this]()
			{
				LoadGeometry(zone, loadMesh);
			});

		m_loadMeshOnZone = false;
		m_nextZoneToLoad.clear();
	}

	g_render->EndFrame();
	return true;
}

bool Application::HandleEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

		auto& io = ImGui::GetIO();

		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				m_windowRect.z = event.window.data1;
				m_windowRect.w = event.window.data2;

				Uint32 flags = SDL_GetWindowFlags(m_window);
				bool isMaximized = flags & SDL_WINDOW_MAXIMIZED;
				if (!isMaximized)
					m_config.SetSavedWindowDimensions(m_windowRect);

				UpdateViewport();
			}
			else if (event.window.event == SDL_WINDOWEVENT_MOVED)
			{
				m_windowRect.x = event.window.data1;
				m_windowRect.y = event.window.data2;

				Uint32 flags = SDL_GetWindowFlags(m_window);
				bool isMaximized = flags & SDL_WINDOW_MAXIMIZED;
				if (!isMaximized)
					m_config.SetSavedWindowDimensions(m_windowRect);
			}
			else if (event.window.event == SDL_WINDOWEVENT_SHOWN)
			{
				int x, y, z, w;
				SDL_GetWindowPosition(m_window, &x, &y);
				SDL_GetWindowSize(m_window, &z, &w);

				m_windowRect = { x, y, z, w };

				Uint32 flags = SDL_GetWindowFlags(m_window);
				bool isMaximized = flags & SDL_WINDOW_MAXIMIZED;
				if (!isMaximized)
					m_config.SetSavedWindowDimensions(m_windowRect);
			}
			break;

		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				if (m_meshTool->isBuildingTiles())
					Halt();
				else if (m_showZonePickerDialog)
					m_showZonePickerDialog = false;
			}

			if (ImGui::GetIO().WantTextInput)
				break;

			// Handle any key presses here.
			if (event.key.keysym.mod & KMOD_CTRL)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_o:
					ShowZonePickerDialog();
					break;
				case SDLK_l:
					OpenMesh();
					break;
				case SDLK_s:
					SaveMesh();
					break;

				case SDLK_p:
					m_showProperties = !m_showProperties;
					break;

				case SDLK_t:
					m_showTools = !m_showTools;
					break;

				case SDLK_COMMA:
					m_showSettingsDialog = true;
					break;
				default:
					//printf("key: %d", event.key.keysym.sym);
					break;
				}
			}
			else if (event.key.keysym.sym == SDLK_F4
				&& (event.key.keysym.mod & KMOD_ALT))
			{
				m_done = true;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (io.WantCaptureMouse)
				break;

			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				// Clear keyboard focus
				ImGui::SetWindowFocus(nullptr);

				// Rotate view
				m_rotate = true;
				m_lastMouseLook.x = m_mousePos.x;
				m_lastMouseLook.y = m_mousePos.y;
			}
			else if (event.button.button == SDL_BUTTON_LEFT)
			{
				// Hit test mesh.
				if (m_geom)
				{
					const glm::vec3& rayStart = g_render->GetCursorRayStart();
					const glm::vec3& rayEnd = g_render->GetCursorRayEnd();

					// Hit test mesh.
					if (float t; m_geom->raycastMesh(rayStart, rayEnd, t))
					{
						glm::vec3 pos = rayStart + (rayEnd - rayStart) * t;

						if (SDL_GetModState() & KMOD_CTRL)
						{
							m_mposSet = true;
							m_mpos = pos;
						}
						else
						{
							bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

							m_meshTool->handleClick(pos, shift);
						}
					}
					else
					{
						if (SDL_GetModState() & KMOD_CTRL)
						{
							m_mposSet = false;
						}
					}
				}
			}
			break;

		case SDL_MOUSEBUTTONUP:
			if (io.WantCaptureMouse)
				break;

			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				//SDL_SetRelativeMouseMode(SDL_FALSE);
				m_rotate = false;
			}
			break;

		case SDL_MOUSEMOTION:
			g_render->SetMousePosition(glm::ivec2{ event.motion.x, event.motion.y });
			m_mousePos.x = event.motion.x;
			m_mousePos.y = event.motion.y;

			if (io.WantCaptureMouse)
				break;

			if (m_rotate)
			{
				int dx = m_mousePos.x - m_lastMouseLook.x;
				int dy = m_mousePos.y - m_lastMouseLook.y;
				m_lastMouseLook = m_mousePos;
				m_lastMouseLook.y = m_mousePos.y;

				Camera* camera = g_render->GetCamera();
				camera->UpdateMouseLook(dx, -dy);
			}
			break;

		case SDL_QUIT:
			Halt();
			m_done = true;
			break;

		default:
			break;
		}
	}

	return !m_done;
}

void Application::UpdateCamera()
{
	Camera* camera = g_render->GetCamera();

	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		camera->ClearKeyState();
	}
	else
	{
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		camera->SetKeyState(CameraKey_Forward, keyState[SDL_SCANCODE_W] != 0);
		camera->SetKeyState(CameraKey_Backward, keyState[SDL_SCANCODE_S] != 0);
		camera->SetKeyState(CameraKey_Left, keyState[SDL_SCANCODE_A] != 0);
		camera->SetKeyState(CameraKey_Right, keyState[SDL_SCANCODE_D] != 0);
		camera->SetKeyState(CameraKey_Up, keyState[SDL_SCANCODE_SPACE] != 0);
		camera->SetKeyState(CameraKey_Down, keyState[SDL_SCANCODE_C] != 0);
		camera->SetKeyState(CameraKey_SpeedUp, (SDL_GetModState() & KMOD_SHIFT) != 0);
	}

	camera->Update(m_timeDelta);
}

void Application::UpdateImGui()
{
	if (!m_activityMessage.empty())
	{
		mq::imgui::RenderTextCentered(
			ImVec2(static_cast<float>(m_windowRect.z / 2), static_cast<float>(m_windowRect.w / 2)),
			ImColor(255, 255, 255, 128), m_activityMessage.c_str(), m_progress);
	}

	m_showFailedToOpenDialog = false;

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	m_dockspaceID = ImGui::DockSpaceOverViewport(mainViewport, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open Zone", "Ctrl+O", nullptr,
				!m_meshTool->isBuildingTiles()))
			{
				ShowZonePickerDialog();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Settings", "Ctrl+,", nullptr))
			{
				m_showSettingsDialog = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit", "Alt+F4"))
				m_done = true;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Zone"))
		{
			if (ImGui::MenuItem("Load Mesh", "Ctrl+L", nullptr,
				!m_meshTool->isBuildingTiles()))
			{
				OpenMesh();
			}
			if (ImGui::MenuItem("Save Mesh", "Ctrl+S", nullptr,
				!m_meshTool->isBuildingTiles() && m_navMesh->IsNavMeshLoaded()))
			{
				SaveMesh();
			}
			if (ImGui::MenuItem("Reset Mesh", "", nullptr,
				!m_meshTool->isBuildingTiles() && m_navMesh->IsNavMeshLoaded()))
			{
				m_navMesh->ResetNavMesh();
				m_meshTool->handleGeometryChanged(m_geom.get());
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Area Types..."))
				m_showMapAreas = !m_showMapAreas;

			ImGui::Separator();

			if (ImGui::MenuItem("Export Settings", "", nullptr,
				!m_meshTool->isBuildingTiles() && m_navMesh->IsNavMeshLoaded()))
			{
				ShowImportExportSettingsDialog(false);
			}

			if (ImGui::MenuItem("Import Settings", "", nullptr,
				!m_meshTool->isBuildingTiles() && m_navMesh->IsNavMeshLoaded()))
			{
				ShowImportExportSettingsDialog(true);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Show Properties", "Ctrl+P", m_showProperties))
				m_showProperties = !m_showProperties;
			if (ImGui::MenuItem("Show Tools", "Ctrl+T", m_showTools))
				m_showTools = !m_showTools;
			if (ImGui::MenuItem("Show Debug", nullptr, m_showDebug))
				m_showDebug = !m_showDebug;
			if (ImGui::MenuItem("Show Shortcuts Overlay", nullptr, m_showOverlay))
				m_showOverlay = !m_showOverlay;

			ImGui::Separator();

			if (ImGui::MenuItem("Show Log", 0, m_showLog))
				m_showLog = !m_showLog;

			if (ImGui::MenuItem("Show ImGui Demo", 0, m_showDemo))
				m_showDemo = !m_showDemo;

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawZonePickerDialog();

	if (m_showOverlay)
	{
		ImGuiContext* ctx = ImGui::GetCurrentContext();
		ImVec2 Corner;

		if (ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(ctx, m_dockspaceID))
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

	if (m_showFailedToOpenDialog)
		ImGui::OpenPopup("Failed To Open");
	if (ImGui::BeginPopupModal("Failed To Open"))
	{
		m_showFailedToOpenDialog = false;
		ImGui::Text("Failed to open mesh.");
		if (ImGui::Button("Close"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (m_showFailedToLoadZone)
		ImGui::OpenPopup("Failed To Open Zone");
	if (ImGui::BeginPopupModal("Failed To Open Zone"))
	{
		m_showFailedToLoadZone = false;

		ImGui::Text("%s", m_failedZoneMsg.c_str());
		if (ImGui::Button("Close"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ShowSettingsDialog();

	if (m_importExportSettings)
	{
		bool show = true;
		m_importExportSettings->Show(&show);
		if (!show)
		{
			m_importExportSettings.reset();
		}
	}

	if (m_showDemo)
	{
		ImGui::ShowDemoWindow(&m_showDemo);
	}

	if (m_showProperties)
	{
		ImGui::SetNextWindowPos(ImVec2(20, 21 + 20), ImGuiCond_FirstUseEver);
		//ImGui::SetNextWindowSizeConstraints(ImVec2(360, -1), ImVec2(360, -1));

		if (ImGui::Begin("Info", &m_showProperties))
		{
			// show zone name
			if (!m_zoneLoaded)
				ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
			else
				ImGui::TextColored(ImColor(0, 255, 0), m_zoneDisplayName.c_str());

			ImGui::Separator();

			Camera* camera = g_render->GetCamera();

			glm::vec3 cameraPos = to_eq_coord(camera->GetPosition());
			if (ImGui::DragFloat3("Position", glm::value_ptr(cameraPos), 0.01f))
			{
				camera->SetPosition(from_eq_coord(cameraPos));
			}

			float angle[2] = { camera->GetHorizontalAngle(), camera->GetVerticalAngle() };
			if (ImGui::DragFloat2("Camera Angle", angle, 0.1f))
			{
				camera->SetHorizontalAngle(angle[0]);
				camera->SetVerticalAngle(angle[1]);
			}

			float fov = camera->GetFieldOfView();
			if (ImGui::DragFloat("FOV", &fov))
			{
				camera->SetFieldOfView(fov);
			}

			float ratio = camera->GetAspectRatio();
			if (ImGui::DragFloat("Aspect Ratio", &ratio, 0.1f))
			{
				camera->SetAspectRatio(ratio);
			}

			if (m_geom)
			{
				auto* loader = m_geom->getMeshLoader();

				if (loader->HasDynamicObjects())
					ImGui::TextColored(ImColor(0, 127, 127), "%d zone objects loaded", loader->GetDynamicObjectsCount());
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

				ImGui::Text("Verts: %.1fk Tris: %.1fk", loader->getVertCount() / 1000.0f, loader->getTriCount() / 1000.0f);

				if (m_navMesh->IsNavMeshLoaded())
				{
					ImGui::Separator();
					if (ImGui::Button((const char*)ICON_FA_FLOPPY_O " Save"))
						SaveMesh();
				}
			}

			ImGuiContext* ctx = ImGui::GetCurrentContext();

			ImGui::DragInt4("Window Rect", &m_windowRect.x);

			glm::ivec4 rect = g_render->GetViewport();
			ImGui::DragInt4("Viewport", &rect.x);

			ImGui::DragInt2("Mouse Pos", &m_mousePos.x);

			glm::ivec2 viewportMouse = g_render->GetMousePos();
			ImGui::DragInt2("Viewport Mouse", &viewportMouse.x);


			//if (ImGui::BeginTabItem("Scene"))
			//{
			//	float planes[2] = { m_nearPlane, m_farPlane };
			//	if (ImGui::DragFloat2("Near/Far Plane", planes))
			//	{
			//		m_nearPlane = planes[0];
			//		m_farPlane = planes[1];
			//	}

			//	ImGui::InputInt4("Viewport", glm::value_ptr(m_viewport));
			//	ImGui::InputFloat3("Ray Start", glm::value_ptr(m_rayStart));
			//	ImGui::InputFloat3("Ray End", glm::value_ptr(m_rayEnd));

			//	if (ImGui::CollapsingHeader("ViewModel Matrix"))
			//	{
			//		ImGui::InputFloat4("[0]", glm::value_ptr(m_viewModelMtx[0]));
			//		ImGui::InputFloat4("[1]", glm::value_ptr(m_viewModelMtx[1]));
			//		ImGui::InputFloat4("[2]", glm::value_ptr(m_viewModelMtx[2]));
			//		ImGui::InputFloat4("[3]", glm::value_ptr(m_viewModelMtx[3]));
			//	}

			//	if (ImGui::CollapsingHeader("Projection Matrix"))
			//	{
			//		ImGui::InputFloat4("[0]", glm::value_ptr(m_projMtx[0]));
			//		ImGui::InputFloat4("[1]", glm::value_ptr(m_projMtx[1]));
			//		ImGui::InputFloat4("[2]", glm::value_ptr(m_projMtx[2]));
			//		ImGui::InputFloat4("[3]", glm::value_ptr(m_projMtx[3]));
			//	}

			//	ImGui::DragFloat("FOV", &m_fov, 0.1f, 30.0f, 150.0f);

			//	ImGui::InputInt2("Mouse", glm::value_ptr(m_mousePos));

			//	ImGui::Separator();
			//	ImGui::SliderFloat("Cam Speed", &m_camMoveSpeed, 10, 350);

			//	ImGui::EndTabItem();
			//}

			//if (ImGui::BeginTabItem("Im3d"))
			//{
			//	Im3d::AppData& appData = Im3d::GetAppData();
			//	static float size = 2.0f;

			//	//for (int i = 0; i < Im3d::Key_Count; ++i)
			//	//{
			//	//	ImGui::Text("Key %d: %d", i, appData.m_keyDown[i]);
			//	//}
			//	ImGui::DragFloat("Size", &size);
			//	ImGui::InputFloat3("Cursor Ray Origin", &appData.m_cursorRayOrigin.x);
			//	ImGui::InputFloat3("Cursor Ray Direction", &appData.m_cursorRayDirection.x);
			//	Im3d::SetSize(size);

			//	static Im3d::Mat4 transform(1.0f);
			//	//transform.setTranslation(Im3d::Vec3(200, -300, 200));

			//	Im3d::Text(Im3d::Vec3(10, 0, 0), 1.0, Im3d::Color(1.0, 0.0, 0.0), 0, "Hello");

			//	if (Im3d::Gizmo("GizmoUnified", transform))
			//	{
			//		// if Gizmo() returns true, the transform was modified
			//		switch (Im3d::GetContext().m_gizmoMode)
			//		{
			//		case Im3d::GizmoMode_Translation:
			//		{
			//			Im3d::Vec3 pos = transform.getTranslation();
			//			ImGui::Text("Position: %.3f, %.3f, %.3f", pos.x, pos.y, pos.z);
			//			break;
			//		}
			//		case Im3d::GizmoMode_Rotation:
			//		{
			//			Im3d::Vec3 euler = Im3d::ToEulerXYZ(transform.getRotation());
			//			ImGui::Text("Rotation: %.3f, %.3f, %.3f", Im3d::Degrees(euler.x), Im3d::Degrees(euler.y), Im3d::Degrees(euler.z));
			//			break;
			//		}
			//		case Im3d::GizmoMode_Scale:
			//		{
			//			Im3d::Vec3 scale = transform.getScale();
			//			ImGui::Text("Scale: %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
			//			break;
			//		}
			//		default: break;
			//		};

			//	}

			//	ImGui::EndTabItem();
			//}
		}

		ImGui::End();
	}

	if (m_showTools)
	{
		// start the tools dialog
		//ImGui::SetNextWindowPos(ImVec2(m_width - 315, 21));
		//ImGui::SetNextWindowSize(ImVec2(315, m_height - 21));

		//const float oldWindowRounding = ImGui::GetStyle().WindowRounding;
		//ImGui::GetStyle().WindowRounding = 0;

		ImGui::Begin("Tools##Tools", &m_showTools, /*ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoTitleBar*/ 0);

		//ImGui::GetStyle().WindowRounding = oldWindowRounding;

		if (m_geom)
		{
			if (m_meshTool->isBuildingTiles())
			{
				int tw, th, tm;
				m_meshTool->getTileStatistics(tw, th, tm);
				int tt = tw * th;

				float percent = (float)m_meshTool->getTilesBuilt() / (float)tt;

				char szProgress[256];
				sprintf_s(szProgress, "%d of %d (%.2f%%)", m_meshTool->getTilesBuilt(), tt, percent * 100);

				ImGui::ProgressBar(percent, ImVec2(-1, 0), szProgress);

				if (ImGui::Button((const char*)ICON_MD_CANCEL " Stop"))
					m_meshTool->CancelBuildAllTiles();
			}
			else
			{
				m_meshTool->handleTools();
			}
		}

		ImGui::End();
	}

	if (m_showDebug)
	{
		if (ImGui::Begin("Debug", &m_showDebug))
			m_meshTool->handleDebug();

		ImGui::End();
	}

	if (m_showLog)
		m_console.Draw("Log", &m_showLog);

	if (m_showMapAreas)
	{
		ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_Once);

		if (ImGui::Begin("Area Types", &m_showMapAreas))
		{
			DrawAreaTypesEditor();
		}

		ImGui::End();
	}
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Application::OpenMesh()
{
	if (m_meshTool->isBuildingTiles())
		return;

	if (m_navMesh->LoadNavMeshFile() != NavMesh::LoadResult::Success)
	{
		m_showFailedToOpenDialog = true;
	}
}

void Application::SaveMesh()
{
	if (m_meshTool->isBuildingTiles() || !m_navMesh->IsNavMeshLoaded())
		return;

	m_meshTool->SaveNavMesh();
}

void Application::ShowSettingsDialog()
{
	if (m_showSettingsDialog)
		ImGui::OpenPopup("Settings");
	m_showSettingsDialog = false;

	if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("EQ Path");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##EQPath", (char*)m_config.GetEverquestPath().c_str(),
			m_config.GetEverquestPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##EverquestPath", ImVec2(120, 0)))
			m_config.SelectEverquestPath();
		ImGui::PopItemWidth();

		ImGui::Text("Navmesh Path (Path to MacroQuest root directory)");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##NavmeshPath", (char*)m_config.GetMacroQuestPath().c_str(), m_config.GetMacroQuestPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##OutputPath", ImVec2(120, 0)))
		{
			m_config.SelectMacroQuestPath();
			m_navMesh->SetNavMeshDirectory(m_config.GetOutputPath());
		}
		ImGui::PopItemWidth();

		bool useExtents = m_config.GetUseMaxExtents();
		if (ImGui::Checkbox("Apply max extents to zones with far away geometry", &useExtents))
			m_config.SetUseMaxExtents(useExtents);
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

void Application::DrawZonePickerDialog()
{
	if (m_meshTool->isBuildingTiles())
	{
		m_showZonePickerDialog = false;
	}

	if (m_showZonePickerDialog)
	{
		if (m_zonePicker->Show())
		{
			m_loadMeshOnZone = m_zonePicker->ShouldLoadNavMesh();
			m_nextZoneToLoad = m_zonePicker->GetSelectedZone();
			m_showZonePickerDialog = false;
		}
	}

	if (!m_showZonePickerDialog && m_zonePicker)
	{
		m_zonePicker.reset();
	}
}

void Application::ShowZonePickerDialog()
{
	m_showZonePickerDialog = true;

	if (!m_zonePicker)
	{
		m_zonePicker = std::make_unique<ZonePicker>(m_config);
	}
}

void Application::UpdateViewport()
{
	ImGuiContext* ctx = ImGui::GetCurrentContext();

	g_render->SetWindowSize(m_windowRect.z, m_windowRect.w);

	if (ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(ctx, m_dockspaceID))
	{
		ImGuiDockNode* centralNode = dockNode->CentralNode;

		g_render->SetViewport({ centralNode->Pos.x, centralNode->Pos.y }, { centralNode->Size.x, centralNode->Size.y });
	}
	else
	{
		g_render->SetViewport({ 0, 0 }, m_windowRect.zw());
	}
}

void Application::ShowImportExportSettingsDialog(bool import)
{
	m_importExportSettings.reset(new ImportExportSettingsDialog(m_navMesh, import));
}

static bool RenderAreaType(NavMesh* navMesh, const PolyAreaType& area, int userIndex = -1)
{
	uint8_t areaId = area.id;

	bool builtIn = !IsUserDefinedPolyArea(areaId);
	ImColor col(area.color);
	int32_t flags32 = area.flags;
	float cost = area.cost;
	bool useNewName = false;
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

void Application::DrawAreaTypesEditor()
{
	ImGui::Columns(5, 0, false);

	RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Unwalkable));
	RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Ground));
	//RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Jump));
	RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Water));
	RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Prefer));
	RenderAreaType(m_navMesh.get(), m_navMesh->GetPolyArea((uint8_t)PolyArea::Avoid));

	ImGui::Separator();

	for (uint8_t index = (uint8_t)PolyArea::UserDefinedFirst;
		index <= (uint8_t)PolyArea::UserDefinedLast; ++index)
	{
		const auto& area = m_navMesh->GetPolyArea(index);
		if (area.valid)
		{
			if (RenderAreaType(m_navMesh.get(), area, area.id - (uint8_t)PolyArea::UserDefinedFirst))
				m_navMesh->RemoveUserDefinedArea(area.id);
		}
	}

	uint8_t nextAreaId = m_navMesh->GetFirstUnusedUserDefinedArea();
	if (nextAreaId != 0)
	{
		if (ImGui::Button(" + "))
		{
			auto area = m_navMesh->GetPolyArea(nextAreaId);
			m_navMesh->UpdateArea(area);
		}
	}

	ImGui::Columns(1);
}

void Application::ResetCamera()
{
	if (m_geom)
	{
		const glm::vec3& bmin = m_geom->getMeshBoundsMin();
		const glm::vec3& bmax = m_geom->getMeshBoundsMax();

		// Reset camera and fog to match the mesh bounds.
		float camr = glm::distance(bmax, bmin) / 2;

		Camera* camera = g_render->GetCamera();
		camera->SetFarClipPlane(camr * 3);

		glm::vec3 cam = {
			(bmax[0] + bmin[0]) / 2 + camr,
			(bmax[1] + bmin[1]) / 2 + camr,
			(bmax[2] + bmin[2]) / 2 + camr
		};

		camera->SetPosition(cam);
		camera->SetHorizontalAngle(-45);
		camera->SetVerticalAngle(45);
	}
}

std::string Application::GetMeshFilename()
{
	std::stringstream ss;
	ss << m_config.GetOutputPath() << "\\" << m_zoneShortname << ".navmesh";

	return ss.str();
}

void Application::Halt()
{
	m_meshTool->CancelBuildAllTiles();
}

void Application::LoadGeometry(const std::string& zoneShortName, bool loadMesh)
{
	std::unique_lock<std::mutex> lock(m_renderMutex);

	Halt();

	std::string eqPath = m_config.GetEverquestPath();
	std::string outputPath = m_config.GetOutputPath();

	auto ptr = std::make_unique<InputGeom>(zoneShortName, eqPath);

	auto geomLoader = std::make_unique<MapGeometryLoader>(
		zoneShortName, eqPath, outputPath);

	if (m_config.GetUseMaxExtents())
	{
		auto iter = MaxZoneExtents.find(zoneShortName);
		if (iter != MaxZoneExtents.end())
		{
			geomLoader->SetMaxExtents(iter->second);
		}
	}

	if (!ptr->loadGeometry(std::move(geomLoader), m_rcContext.get()))
	{
		m_showFailedToLoadZone = true;

		std::stringstream ss;
		ss << "Failed to load zone: " << m_config.GetLongNameForShortName(zoneShortName);

		m_failedZoneMsg = ss.str();
		m_geom.reset();

		m_navMesh->ResetNavMesh();
		m_meshTool->handleGeometryChanged(nullptr);
		m_zoneLoaded = false;
		return;
	}

	m_geom = std::move(ptr);

	m_zoneShortname = zoneShortName;
	m_zoneLongname = m_config.GetLongNameForShortName(m_zoneShortname);

	m_zoneDisplayName = fmt::format("{} ({})", m_zoneLongname, m_zoneShortname);
	m_expansionExpanded.clear();

	// Update the window title
	std::string windowTitle = fmt::format("MacroQuest NavMesh Generator - {}", m_zoneDisplayName);
	SDL_SetWindowTitle(m_window, windowTitle.c_str());

	m_meshTool->handleGeometryChanged(m_geom.get());
	m_navMesh->SetZoneName(m_zoneShortname);
	ResetCamera();
	m_zoneLoaded = true;

	if (loadMesh)
	{
		m_navMesh->LoadNavMeshFile();
	}
}

void Application::PushEvent(const std::function<void()>& cb)
{
	std::unique_lock<std::mutex> lock(m_callbackMutex);
	m_callbackQueue.push_back(cb);
}

bool Application::HandleCallbacks()
{
	std::vector<std::function<void()>> callbacks;

	{
		std::unique_lock<std::mutex> lock(m_callbackMutex);
		std::swap(callbacks, m_callbackQueue);
	}

	for (const auto& cb : callbacks)
	{
		cb();
	}

	return !m_done;
}

//----------------------------------------------------------------------------

ImportExportSettingsDialog::ImportExportSettingsDialog(
	const std::shared_ptr<NavMesh>& navMesh, bool import)
	: m_import(import)
	, m_navMesh(navMesh)
{
	fs::path settingsPath = navMesh->GetNavMeshDirectory();
	settingsPath /= "Settings";
	settingsPath /= navMesh->GetZoneName() + ".json";

	m_defaultFilename = std::make_unique<char[]>(256);
	strcpy_s(m_defaultFilename.get(), 256, settingsPath.string().c_str());
}

void ImportExportSettingsDialog::Show(bool* open /* = nullptr */)
{
	auto navMesh = m_navMesh.lock();

	if (!navMesh)
	{
		*open = false;
		return;
	}

	const char* failedDialog = m_import ? "Failed to import" : "Failed to export";

	if (m_failed)
	{
		if (ImGui::BeginPopupModal(failedDialog, open))
		{
			if (m_import)
				ImGui::Text("Failed to import settings. Settings file might not exist or it may be invalid.");
			else
				ImGui::Text("Failed to export settings. Output directory or file may not be writable.");

			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	}
	else
	{
		auto& io = ImGui::GetIO();

		ImGui::SetNextWindowPos(io.DisplaySize, ImGuiCond_Appearing, ImVec2(0.5, 0.5));
		ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

		if (ImGui::Begin(m_import ? "Import Settings" : "Export Settings", open))
		{
			ImGui::Text("Settings File");

			ImGui::PushItemWidth(-1);
			bool changed = ImGui::InputText("##Edit", m_defaultFilename.get(), 256);

			if (m_import && (m_firstShow || changed))
			{
				std::error_code ec;
				fs::path p(m_defaultFilename.get());
				m_fileMissing = !fs::is_regular_file(p, ec);
			}
			if (m_fileMissing)
			{
				ImGui::TextColored(ImColor(255, 0, 0), (const char*) ICON_FA_EXCLAMATION_TRIANGLE " File is missing");
			}

			ImGui::PopItemWidth();

			ImGui::CheckboxFlags("Mesh Settings", (uint32_t*)&m_fields, +PersistedDataFields::BuildSettings);
			ImGui::CheckboxFlags("Area Types", (uint32_t*)&m_fields, +PersistedDataFields::AreaTypes);
			ImGui::CheckboxFlags("Convex Volumes", (uint32_t*)&m_fields, +PersistedDataFields::ConvexVolumes);

			if (ImGui::Button(m_import ? "Import" : "Export"))
			{
				auto func = m_import ? &NavMesh::ImportJson : &NavMesh::ExportJson;

				bool result = ((*navMesh).*func)(m_defaultFilename.get(), m_fields);
				if (!result)
				{
					m_failed = true;
				}
				else
				{
					*open = false;
				}
			}
		}

		ImGui::End();

		if (m_failed)
		{
			ImGui::OpenPopup(failedDialog);
		}
	}

	m_firstShow = false;
}
