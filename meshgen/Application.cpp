//
// Application.cpp
//

#include "meshgen/Application.h"
#include "meshgen/DebugDrawImpl.h"
#include "meshgen/InputGeom.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZonePicker.h"
#include "meshgen/resource.h"
#include "common/Utilities.h"

#include <RecastDebugDraw.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <zone-utilities/log/log_base.h>
#include <zone-utilities/log/log_macros.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "imgui/imgui_impl.h"
#include "imgui/ImGuiUtils.h"
#include "imgui/imgui_impl_sdl2.h"
#include <imgui.h>

#include <filesystem>
#include <sstream>

#include "mq/base/Color.h"
#include "engine/bgfx_utils.h"
#include "engine/debugdraw/debugdraw.h"
#include <bgfx/bgfx.h>

#pragma warning(push)
#pragma warning(disable: 4244)

namespace fs = std::filesystem;

//============================================================================

static const int32_t MAX_LOG_MESSAGES = 1000;

static bool IsKeyboardBlocked()
{
	return ImGui::GetIO().WantTextInput;
}

static HWND sdlNativeWindowHandle(SDL_Window* window)
{
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (!SDL_GetWindowWMInfo(window, &wmi))
	{
		return nullptr;
	}

	return wmi.info.win.window;
}

class EQEmuLogSink : public EQEmu::Log::LogBase
{
public:
	EQEmuLogSink()
	{
		m_logger = spdlog::get("EQEmu");
	}

	virtual void OnRegister(int enabled_logs) override {}
	virtual void OnUnregister() override {}

	virtual void OnMessage(EQEmu::Log::LogType log_type, const std::string& message) override
	{
		switch (log_type)
		{
		case EQEmu::Log::LogTrace:
			m_logger->trace(message);
			break;
		case EQEmu::Log::LogDebug:
			m_logger->debug(message);
			break;
		case EQEmu::Log::LogInfo:
			m_logger->info(message);
			break;
		case EQEmu::Log::LogWarn:
			m_logger->warn(message);
			break;
		case EQEmu::Log::LogError:
			m_logger->error(message);
			break;
		case EQEmu::Log::LogFatal:
			m_logger->critical(message);
			break;
		}
	}

private:
	std::shared_ptr<spdlog::logger> m_logger;
};

//============================================================================

Application::Application()
{
	// Construct the path to the ini file
	char fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);

	// TODO: Support config directory
	m_iniFile = fmt::format("{}/config/MeshGenerator_UI.ini", fullPath);
	m_logFile = fmt::format("{}/logs/MeshGenerator.log", fullPath);

	// set up default logger
	auto logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("MeshGen", m_logFile, true);
	logger->sinks().push_back(std::make_shared<ConsoleLogSink<std::mutex>>(this));
#if defined(_DEBUG)
	logger->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("%L %Y-%m-%d %T.%f [%n] %v");
	spdlog::flush_every(std::chrono::seconds{ 5 });
	spdlog::register_logger(logger->clone("Recast"));
	spdlog::register_logger(logger->clone("EQEmu"));

	eqLogInit(-1);
	eqLogRegister(std::make_shared<EQEmuLogSink>());
}

Application::~Application()
{
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

	m_navMesh = std::make_shared<NavMesh>(m_eqConfig.GetOutputPath(), startingZone);
	m_meshTool = std::make_unique<NavMeshTool>(m_navMesh);
	m_camera = new Camera();

	m_rcContext = std::make_unique<RecastContext>();

	m_meshTool->setContext(m_rcContext.get());
	m_meshTool->setOutputPath(m_eqConfig.GetOutputPath().c_str());

	m_lastTime = GetTickCount64();

	return true;
}

bool Application::InitSystem()
{
	SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER);

	m_width = 1600;
	m_height = 900;

	// scale default window size by monitor dpi
	float dpi = 0;
	if (SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr) == 0)
	{
		if (dpi > 0)
		{
			dpi = dpi / 96;
			m_width *= dpi;
			m_height *= dpi;
		}
	}

	m_debug = BGFX_DEBUG_TEXT;
	m_reset = BGFX_RESET_MSAA_X16;

	// Setup window
	SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	m_window = SDL_CreateWindow("MacroQuest NavMesh Generator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width, m_height, window_flags);
	if (m_window == nullptr)
	{
		char szMessage[256];
		sprintf_s(szMessage, "Error: SDL_CreateWindow(): %s", SDL_GetError());

		::MessageBoxA(nullptr, szMessage, "Error Starting Engine", MB_OK | MB_ICONERROR);
		return false;
	}

	bgfx::Init init;
	init.type = bgfx::RendererType::Direct3D11;
	init.vendorId = BGFX_PCI_ID_NONE;
	init.platformData.nwh = sdlNativeWindowHandle(m_window);
	init.platformData.ndt = nullptr;
	init.platformData.type = bgfx::NativeWindowHandleType::Default;
	init.resolution.width = m_width;
	init.resolution.height = m_height;
	init.resolution.reset = m_reset;
	if (!bgfx::init(init))
	{
		char szMessage[256];
		sprintf_s(szMessage, "Error: bgfx::init() failed");

		::MessageBoxA(nullptr, szMessage, "Error Starting Engine", MB_OK | MB_ICONERROR);
		return false;
	}

	bgfx::setViewMode(0, bgfx::ViewMode::Sequential);

	utilsInit();
	ddInit();
	
	DebugDrawImpl::init();
	ZoneRenderManager::init();

	// Enable debug text.
	bgfx::setDebug(m_debug);

	// Set view 0 clear state.
	ImVec4 clear_color = ImVec4(0.3f, 0.3f, 0.32f, 1.00f);

	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, mq::MQColor(clear_color).ToRGBA(), 1.0f, 0);
	m_caps = bgfx::getCaps();

	//
	// Initialize ImGui
	//

	IMGUI_CHECKVERSION();
	ImGuiContext* context = ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = m_iniFile.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports | ImGuiConfigFlags_DpiEnableScaleFonts;

	ImGui::StyleColorsDark();

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

	//
	//

	m_lastTime = SDL_GetTicks();
	m_time = 0.0f;

	bgfx::frame();
	return true;
}

int Application::shutdown()
{
	Halt();

	if (m_meshTool)
		m_meshTool->GetRenderManager()->DestroyObjects();

	m_zonePicker.reset();
	m_geom.reset();

	ZoneRenderManager::shutdown();

	// shutdown ImGui
	ImGui_Impl_Shutdown();

	DebugDrawImpl::shutdown();
	ddShutdown();

	// Shutdown bgfx.
	bgfx::shutdown();

	utilsShutdown();

	delete m_camera;
	m_camera = nullptr;

	return 0;
}

bool Application::update()
{
	if (m_done)
		return false;

	// fractional time since last update
	Uint32 time = GetTickCount64();
	m_timeDelta = (time - m_lastTime) / 1000.0f;
	m_lastTime = time;
	m_time += m_timeDelta;

	static float timeAcc = 0.0f;

	if (!HandleCallbacks())
		return false;

	if (!HandleEvents())
		return false;

	const float SIM_RATE = 20;
	const float DELTA_TIME = 1.0f / SIM_RATE;
	timeAcc = rcClamp(timeAcc + m_timeDelta, -1.0f, 1.0f);
	int simIter = 0;
	while (timeAcc > DELTA_TIME)
	{
		timeAcc -= DELTA_TIME;
		if (simIter < 5 && m_meshTool)
			m_meshTool->handleUpdate(DELTA_TIME);
		simIter++;
	}

	ImGui_Impl_NewFrame();

	RenderInterface();

	UpdateCamera();

	float view[16];
	m_camera->GetViewMatrix(view);

	float proj[16];
	bx::mtxProj(proj, 50.0f, float(m_width) / float(m_height), m_nearPlane, m_farPlane, m_caps->homogeneousDepth,
		bx::Handedness::Right);

	// Set view 0 default viewport.
	bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));
	bgfx::setViewTransform(0, view, proj);

	// Use debug font to print information about this example.
	bgfx::dbgTextClear();

	// This dummy draw call is here to make sure that view 0 is cleared
	// if no other draw calls are submitted to view 0.
	bgfx::touch(0);

	{
		//std::lock_guard<std::mutex> lock(m_renderMutex);


		m_meshTool->handleRender();
	}

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

	ImGui_Impl_Render();

	// Advance to next frame. Rendering thread will be kicked to process submitted rendering primitives.
	bgfx::frame();

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
				m_width = event.window.data1;
				m_height = event.window.data2;
				bgfx::reset(m_width, m_height, m_reset);
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

			if (IsKeyboardBlocked())
				break;

			// Handle any key presses here.
			if (event.key.keysym.mod & KMOD_CTRL)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_o:
					m_showZonePickerDialog = true;
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
			else
			{
				switch (event.key.keysym.scancode)
				{
				case SDL_SCANCODE_W:
				case SDL_SCANCODE_S:
				case SDL_SCANCODE_A:
				case SDL_SCANCODE_D:
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_C:
					UpdateMovementState(true);
					break;

				default: break;
				}
			}
			break;

		case SDL_KEYUP:
			UpdateMovementState(false);
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
					// Hit test mesh.
					float t;
					if (m_geom->raycastMesh(glm::value_ptr(m_rays), glm::value_ptr(m_raye), t))
					{
						if (SDL_GetModState() & KMOD_CTRL)
						{
							m_mposSet = true;
							m_mpos[0] = m_rays[0] + (m_raye[0] - m_rays[0])*t;
							m_mpos[1] = m_rays[1] + (m_raye[1] - m_rays[1])*t;
							m_mpos[2] = m_rays[2] + (m_raye[2] - m_rays[2])*t;
						}
						else
						{
							glm::vec3 pos;
							pos[0] = m_rays[0] + (m_raye[0] - m_rays[0])*t;
							pos[1] = m_rays[1] + (m_raye[1] - m_rays[1])*t;
							pos[2] = m_rays[2] + (m_raye[2] - m_rays[2])*t;
							bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

							m_meshTool->handleClick(m_rays, pos, shift);
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
			if (io.WantCaptureMouse)
				break;

			m_mousePos.x = event.motion.x;
			m_mousePos.y = m_height - 1 - event.motion.y;
			if (m_rotate)
			{
				int dx = m_mousePos.x - m_lastMouseLook.x;
				int dy = m_mousePos.y - m_lastMouseLook.y;
				m_lastMouseLook = m_mousePos;
				m_lastMouseLook.y = m_mousePos.y;

				m_camera->UpdateMouseLook(dx, dy);
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

void Application::UpdateMovementState(bool keydown)
{

}

void Application::UpdateCamera()
{
	// Perform the camera reset if requested
	if (m_resetCamera)
	{
		ResetCamera();
		m_resetCamera = false;
	}

	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		m_camera->ClearKeyState();
	}
	else
	{
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		m_camera->SetKeyState(CameraKey_Forward, keyState[SDL_SCANCODE_W] != 0);
		m_camera->SetKeyState(CameraKey_Backward, keyState[SDL_SCANCODE_S] != 0);
		m_camera->SetKeyState(CameraKey_Left, keyState[SDL_SCANCODE_A] != 0);
		m_camera->SetKeyState(CameraKey_Right, keyState[SDL_SCANCODE_D] != 0);
		m_camera->SetKeyState(CameraKey_Up, keyState[SDL_SCANCODE_SPACE] != 0);
		m_camera->SetKeyState(CameraKey_Down, keyState[SDL_SCANCODE_C] != 0);
		m_camera->SetKeyState(CameraKey_SpeedUp, (SDL_GetModState() & KMOD_SHIFT) != 0);
	}

	m_camera->Update(m_timeDelta);
}

void Application::RenderInterface()
{
	char buffer[240] = { 0, };

	if (!m_activityMessage.empty())
	{
		mq::imgui::RenderTextCentered(ImVec2(m_width / 2, m_height / 2),
			ImColor(255, 255, 255, 128), m_activityMessage.c_str(), m_progress);
	}

	m_showFailedToOpenDialog = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open Zone", "Ctrl+O", nullptr,
				!m_meshTool->isBuildingTiles()))
			{
				m_showZonePickerDialog = true;
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

	ShowZonePickerDialog();

	if (m_showZonePickerDialog)
	{
		return;
	}

	ImGui::SetNextWindowPos(
		ImVec2(ImGui::GetIO().DisplaySize.x - 10.0 - 315, ImGui::GetIO().DisplaySize.y - 10.0),
		ImGuiCond_Always,
		ImVec2(1.0, 1.0));
	ImGui::SetNextWindowBgAlpha(0.3f);

	if (ImGui::Begin("##Overlay", &m_showOverlay, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
	{
		m_meshTool->handleRenderOverlay(m_proj, m_model, m_view);

		// Help text.
		static const char msg[] = "Hotkeys:\n* W/S/A/D: Move\n"
			"* Space/C: Move up/down\n"
			"* Shift+Move: Increase movement speed\n"
			"* RMB: Look around";

		ImGui::TextColored(ImColor(255, 255, 255, 128), msg);
	}
	ImGui::End();

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

		ImGui::Begin("Properties", &m_showProperties, ImGuiWindowFlags_AlwaysAutoResize);

		// show zone name
		if (!m_zoneLoaded)
			ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
		else
			ImGui::TextColored(ImColor(0, 255, 0), m_zoneDisplayName.c_str());

		ImGui::Separator();

		glm::vec3 cameraPos = m_camera->GetPosition().zxy();
		if (ImGui::DragFloat3("Position", glm::value_ptr(cameraPos), 0.01f))
		{
			m_camera->SetPosition(cameraPos.yzx());
		}

		float angle[2] = { m_camera->GetHorizontalAngle(), m_camera->GetVerticalAngle() };
		if (ImGui::DragFloat2("Camera Angle", angle, 0.1f))
		{
			m_camera->SetHorizontalAngle(angle[0]);
			m_camera->SetVerticalAngle(angle[1]);
		}

		float planes[2] = { m_nearPlane, m_farPlane };
		if (ImGui::DragFloat2("Near/Far Plane", planes))
		{
			m_nearPlane = planes[0];
			m_farPlane = planes[1];
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
		ImGui::Separator();
		ImGui::SliderFloat("Cam Movement Speed", &m_camMoveSpeed, 10, 350);

		ImGui::End();
	}

	if (m_showTools)
	{
		// start the tools dialog
		ImGui::SetNextWindowPos(ImVec2(m_width - 315, 21));
		ImGui::SetNextWindowSize(ImVec2(315, m_height - 21));

		const float oldWindowRounding = ImGui::GetStyle().WindowRounding;
		ImGui::GetStyle().WindowRounding = 0;

		ImGui::Begin("##Tools", &m_showTools, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoTitleBar);

		ImGui::GetStyle().WindowRounding = oldWindowRounding;

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
		ImGui::InputText("##EQPath", (char*)m_eqConfig.GetEverquestPath().c_str(),
			m_eqConfig.GetEverquestPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##EverquestPath", ImVec2(120, 0)))
			m_eqConfig.SelectEverquestPath();
		ImGui::PopItemWidth();

		ImGui::Text("Navmesh Path (Path to MQ2 root directory)");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##NavmeshPath", (char*)m_eqConfig.GetMQ2Path().c_str(),
			m_eqConfig.GetMQ2Path().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##OutputPath", ImVec2(120, 0)))
		{
			m_eqConfig.SelectOutputPath();
			m_navMesh->SetNavMeshDirectory(m_eqConfig.GetOutputPath());
		}
		ImGui::PopItemWidth();

		bool useExtents = m_eqConfig.GetUseMaxExtents();
		if (ImGui::Checkbox("Apply max extents to zones with far away geometry", &useExtents))
			m_eqConfig.SetUseMaxExtents(useExtents);
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

void Application::ShowZonePickerDialog()
{
	if (m_meshTool->isBuildingTiles()) {
		m_showZonePickerDialog = false;
	}

	if (m_showZonePickerDialog) {
		bool focus = false;
		if (!m_zonePicker)
		{
			m_zonePicker = std::make_unique<ZonePicker>(m_eqConfig);
			focus = true;
			ImGui::SetNextWindowFocus();
		}

		if (m_zonePicker->Show(focus))
		{
			m_loadMeshOnZone = m_zonePicker->ShouldLoadNavMesh();
			m_nextZoneToLoad = m_zonePicker->GetSelectedZone();
			m_showZonePickerDialog = false;
		}
	}

	if (!m_showZonePickerDialog && m_zonePicker) {
		m_zonePicker.reset();
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
		float camr = sqrtf(rcSqr(bmax[0] - bmin[0]) + rcSqr(bmax[1] - bmin[1]) + rcSqr(bmax[2] - bmin[2])) / 2;

		glm::vec3 cam = {
			(bmax[0] + bmin[0]) / 2 + camr,
			(bmax[1] + bmin[1]) / 2 + camr,
			(bmax[2] + bmin[2]) / 2 + camr
		};

		m_camera->SetPosition(cam);
		m_camera->SetHorizontalAngle(-45);
		m_camera->SetVerticalAngle(45);
	}
}

std::string Application::GetMeshFilename()
{
	std::stringstream ss;
	ss << m_eqConfig.GetOutputPath() << "\\" << m_zoneShortname << ".navmesh";

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

	std::string eqPath = m_eqConfig.GetEverquestPath();
	std::string outputPath = m_eqConfig.GetOutputPath();

	auto ptr = std::make_unique<InputGeom>(zoneShortName, eqPath);

	auto geomLoader = std::make_unique<MapGeometryLoader>(
		zoneShortName, eqPath, outputPath);

	if (m_eqConfig.GetUseMaxExtents())
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
		ss << "Failed to load zone: " << m_eqConfig.GetLongNameForShortName(zoneShortName);

		m_failedZoneMsg = ss.str();
		m_geom.reset();

		m_navMesh->ResetNavMesh();
		m_meshTool->handleGeometryChanged(nullptr);
		m_zoneLoaded = false;
		return;
	}

	m_geom = std::move(ptr);

	m_zoneShortname = zoneShortName;
	m_zoneLongname = m_eqConfig.GetLongNameForShortName(m_zoneShortname);

	std::stringstream ss;
	ss << m_zoneLongname << " (" << m_zoneShortname << ")";
	m_zoneDisplayName = ss.str();
	m_expansionExpanded.clear();

	// Update the window title
	std::stringstream ss2;
	ss2 << "MQ2Nav Mesh Generator - " << m_zoneDisplayName;
	std::string windowTitle = ss2.str();
	//SDL_SetWindowTitle(m_window, windowTitle.c_str());

	m_meshTool->handleGeometryChanged(m_geom.get());
	m_navMesh->SetZoneName(m_zoneShortname);
	m_resetCamera = true;
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

//============================================================================

RecastContext::RecastContext()
{
	m_logger = spdlog::get("Recast");
}

void RecastContext::doLog(const rcLogCategory category,
	const char* message, const int length)
{
	switch (category)
	{
	case RC_LOG_PROGRESS:
		m_logger->trace(std::string_view(message, length));
		break;

	case RC_LOG_WARNING:
		m_logger->warn(std::string_view(message, length));
		break;

	case RC_LOG_ERROR:
		m_logger->error(std::string_view(message, length));
		break;
	}
}

void RecastContext::doResetTimers()
{
	for (int i = 0; i < RC_MAX_TIMERS; ++i)
		m_accTime[i] = std::chrono::nanoseconds();
}

void RecastContext::doStartTimer(const rcTimerLabel label)
{
	m_startTime[label] = std::chrono::steady_clock::now();
}

void RecastContext::doStopTimer(const rcTimerLabel label)
{
	auto deltaTime = std::chrono::steady_clock::now() - m_startTime[label];

	m_accTime[label] += deltaTime;
}

int RecastContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		m_accTime[label]).count();
}

#pragma warning(pop)

//----------------------------------------------------------------------------

ImportExportSettingsDialog::ImportExportSettingsDialog(
	const std::shared_ptr<NavMesh>& navMesh, bool import)
	: m_navMesh(navMesh)
	, m_import(import)
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
