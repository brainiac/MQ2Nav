//
// Application.cpp
//

// Implementation of main GUI interface for EQNavigation

#include "meshgen/Application.h"
#include "meshgen/InputGeom.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZonePicker.h"
#include "meshgen/resource.h"
#include "meshgen/imgui/imgui_impl_opengl2.h"
#include "meshgen/imgui/imgui_impl_sdl.h"
#include "common/Utilities.h"

#include <RecastDebugDraw.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/fonts/IconsFontAwesome.h>
#include <imgui/misc/fonts/IconsMaterialDesign.h>
#include <imgui/custom/imgui_utils.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gl/GLU.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <zone-utilities/log/log_base.h>
#include <zone-utilities/log/log_macros.h>
#include <boost/algorithm/string.hpp>

#include <filesystem>
#include <sstream>

#pragma warning(push)
#pragma warning(disable: 4244)

namespace fs = std::filesystem;

//============================================================================

static const int32_t MAX_LOG_MESSAGES = 1000;

static bool IsKeyboardBlocked() {
	return ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput;
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

Application::Application(const std::string& defaultZone)
	: m_navMesh(new NavMesh(m_eqConfig.GetOutputPath() + "\\MQ2Nav", defaultZone))
	, m_meshTool(new NavMeshTool(m_navMesh))
	, m_resetCamera(true)
	, m_width(1600), m_height(900)
	, m_progress(0.0)
	, m_showLog(false)
	, m_nextZoneToLoad(defaultZone)
	, m_loadMeshOnZone(!defaultZone.empty())
{
	// Construct the path to the ini file
	CHAR fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);

	m_iniFile = fmt::format("{}/MeshGenerator_UI.ini", fullPath);
	m_logFile = fmt::format("{}/MeshGenerator.log", fullPath);

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
	m_rcContext = std::make_unique<RecastContext>();

	m_meshTool->setContext(m_rcContext.get());
	m_meshTool->setOutputPath(m_eqConfig.GetOutputPath().c_str());

	InitializeWindow();
	ImGui::SetupImGuiStyle(true, 0.8f);
}

Application::~Application()
{
	DestroyWindow();
}

bool Application::InitializeWindow()
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialize SDL\n");
		return false;
	}

	// Init OpenGL
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// scale default window size by monitor dpi
	float dpi = 0;
	if (SDL_GetDisplayDPI(0, &dpi, 0, 0) == 0)
	{
		if (dpi > 0)
		{
			dpi = dpi / 96;
			m_width *= dpi;
			m_height *= dpi;
		}
	}

	int wx = 200, wy = 200;
	SDL_Rect displayBounds;
	if (SDL_GetDisplayBounds(0, &displayBounds) == 0)
	{
		wx = (displayBounds.w - m_width) / 2;
		wy = (displayBounds.h - m_height) / 2;
	}

	m_window = SDL_CreateWindow("MQ2Nav Mesh Generator", wx, wy, m_width, m_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	m_glContext = SDL_GL_CreateContext(m_window);

	// set window icon
	HINSTANCE handle = ::GetModuleHandle(nullptr);
	HICON icon = ::LoadIcon(handle, MAKEINTRESOURCE(IDI_ICON1));
	if (icon != nullptr) {
		SDL_SysWMinfo wminfo;
		SDL_VERSION(&wminfo.version);
		if (SDL_GetWindowWMInfo(m_window, &wminfo) == 1) {
			HWND hwnd = wminfo.info.win.window;

			::SetClassLongPtrA(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(icon));
		}
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = m_iniFile.c_str();

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext);
	ImGui_ImplOpenGL2_Init();

	ImGuiEx::ConfigureFonts();

	glEnable(GL_CULL_FACE);

	float fogCol[4] = { 0.32f, 0.25f, 0.25f, 1 };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0);
	glFogf(GL_FOG_END, 10);
	glFogfv(GL_FOG_COLOR, fogCol);

	glDepthFunc(GL_LEQUAL);

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);

	return true;
}

void Application::DestroyWindow()
{
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(m_glContext);
	SDL_DestroyWindow(m_window);
	SDL_Quit();
}

int Application::RunMainLoop()
{
	m_time = 0.0f;
	m_lastTime = SDL_GetTicks();

	float timeAcc = 0.0f;

	while (!m_done)
	{
		// fractional time since last update
		Uint32 time = SDL_GetTicks();
		m_timeDelta = (time - m_lastTime) / 1000.0f;
		m_lastTime = time;
		m_time += m_timeDelta;

		const float SIM_RATE = 20;
		const float DELTA_TIME = 1.0f / SIM_RATE;
		timeAcc = rcClamp(timeAcc + m_timeDelta, -1.0f, 1.0f);
		int simIter = 0;
		while (timeAcc > DELTA_TIME) {
			timeAcc -= DELTA_TIME;
			if (simIter < 5 && m_meshTool)
				m_meshTool->handleUpdate(DELTA_TIME);
			simIter++;
		}

		ImVec4 clear_color = ImVec4(0.3f, 0.3f, 0.32f, 1.00f);

		glViewport(0, 0, m_width, m_height);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);

		// Render 3d
		glEnable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(50.0f, (float)m_width / (float)m_height, 1.0f, m_camr);

		// set up modelview matrix
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(m_r.x, 1, 0, 0);
		glRotatef(m_r.y, 0, 1, 0);
		glTranslatef(-m_cam.x, -m_cam.y, -m_cam.z);
		glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(m_model));

		//glm::mat4 m2 = glm::mat4x4{ 1.0f };
		//m2 = glm::rotate(m2, glm::radians(m_r.x), glm::vec3{ 1.0f, 0.0f, 0.0f });
		//m2 = glm::rotate(m2, glm::radians(m_r.y), glm::vec3{ 0.0f, 1.0f, 0.0f });
		//m_model = glm::translate(m2, -m_cam);

		// Get hit ray position and direction.
		glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(m_proj));
		glGetIntegerv(GL_VIEWPORT, glm::value_ptr(m_view));

		m_rays = glm::unProject(glm::vec3{ m_m.x, m_m.y, 0.0f }, m_model, m_proj, m_view);
		m_raye = glm::unProject(glm::vec3{ m_m.x, m_m.y, 1.0f }, m_model, m_proj, m_view);

		DispatchCallbacks();

		// Handle input events.
		HandleEvents();

		// Start the ImGui Frame
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_window);
		ImGui::NewFrame();

		RenderInterface();

		{
			std::lock_guard<std::mutex> lock(m_renderMutex);

			UpdateCamera();

			glEnable(GL_FOG);
			m_meshTool->handleRender();
			glDisable(GL_FOG);

			glEnable(GL_DEPTH_TEST);
		}

		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(m_window);

		// Do additional work here after rendering

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
	}

	Halt();

	m_geom.reset();
	return 0;
}

void Application::HandleEvents()
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
				//SDL_SetRelativeMouseMode(SDL_TRUE);

				// Rotate view
				m_rotate = true;
				m_orig.x = m_m.x;
				m_orig.y = m_m.y;
				m_origr.x = m_r.x;
				m_origr.y = m_r.y;
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

			m_m.x = event.motion.x;
			m_m.y = m_height - 1 - event.motion.y;
			if (m_rotate)
			{
				int dx = m_m.x - m_orig.x;
				int dy = m_m.y - m_orig.y;
				m_r.x = m_origr.x - dy * 0.25f;
				m_r.y = m_origr.y + dx * 0.25f;
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
		return;

	const Uint8* keystate = SDL_GetKeyboardState(NULL);
	m_moveW = rcClamp(m_moveW + m_timeDelta * 4 * (keystate[SDL_SCANCODE_W] ? 1 : -1), 0.0f, 1.0f);
	m_moveS = rcClamp(m_moveS + m_timeDelta * 4 * (keystate[SDL_SCANCODE_S] ? 1 : -1), 0.0f, 1.0f);
	m_moveA = rcClamp(m_moveA + m_timeDelta * 4 * (keystate[SDL_SCANCODE_A] ? 1 : -1), 0.0f, 1.0f);
	m_moveD = rcClamp(m_moveD + m_timeDelta * 4 * (keystate[SDL_SCANCODE_D] ? 1 : -1), 0.0f, 1.0f);
	m_moveUp = rcClamp(m_moveUp + m_timeDelta * 4 * (keystate[SDL_SCANCODE_SPACE] ? 1 : -1), 0.0f, 1.0f);
	m_moveDown = rcClamp(m_moveDown + m_timeDelta * 4 * (keystate[SDL_SCANCODE_C] ? 1 : -1), 0.0f, 1.0f);
	
	float keybSpeed = m_camMoveSpeed;
	if (SDL_GetModState() & KMOD_SHIFT)
		keybSpeed *= 10.0f;

	glm::vec3 dp{ m_moveD - m_moveA, m_moveS - m_moveW, m_moveUp - m_moveDown };
	dp *= m_timeDelta * keybSpeed;

	//  0  1  2  3
	//  4  5  6  7
	//  8  9 10 11
	// 12 13 14 15

	m_cam.x += dp.x * m_model[0][0];
	m_cam.x += dp.y * m_model[0][2];

	m_cam.y += dp.x * m_model[1][0];
	m_cam.y += dp.y * m_model[1][2];
	m_cam.y += dp.z;

	m_cam.z += dp.x * m_model[2][0];
	m_cam.z += dp.y * m_model[2][2];
}

void Application::RenderInterface()
{
	char buffer[240] = { 0, };

	if (!m_activityMessage.empty())
	{
		ImGui::RenderTextCentered(ImVec2(m_width / 2, m_height / 2),
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
		ImGui::SetNextWindowPos(ImVec2(20, 21 + 20), ImGuiSetCond_FirstUseEver);

		ImGui::Begin("Properties", &m_showProperties, ImGuiWindowFlags_AlwaysAutoResize);

		// show zone name
		if (!m_zoneLoaded)
			ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
		else
			ImGui::TextColored(ImColor(0, 255, 0), m_zoneDisplayName.c_str());

		ImGui::Separator();

		float camPos[3] = { m_cam.z, m_cam.x, m_cam.y };
		if (ImGui::InputFloat3("Position", camPos, 2))
		{
			m_cam.x = camPos[1];
			m_cam.y = camPos[2];
			m_cam.z = camPos[0];
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
				if (ImGui::Button(ICON_FA_FLOPPY_O " Save"))
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

				if (ImGui::Button(ICON_MD_CANCEL " Stop"))
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
		ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiSetCond_Once);

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

		ImGui::Text("Navmesh Path");
		ImGui::PushItemWidth(400);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(244, 250, 125));
		ImGui::InputText("##NavmeshPath", (char*)m_eqConfig.GetOutputPath().c_str(),
			m_eqConfig.GetOutputPath().length(), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(125);
		if (ImGui::Button("Change##OutputPath", ImVec2(120, 0)))
			m_eqConfig.SelectOutputPath();
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
		if (!m_zonePicker) {
			m_zonePicker = std::make_unique<ZonePicker>(m_eqConfig);
			focus = true;
			ImGui::SetNextWindowFocus();
		}
		if (m_zonePicker->Show(focus, &m_nextZoneToLoad)) {
			m_loadMeshOnZone = m_zonePicker->ShouldLoadNavMesh();
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

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	float offset = window->DC.CurrentLineTextBaseOffset;
	window->DC.CurrentLineTextBaseOffset += 2.0f;

	if (builtIn)
		ImGui::Text("Built-in %d", areaId);
	else
		ImGui::Text("User %d", userIndex != -1 ? userIndex : areaId);

	window->DC.CurrentLineTextBaseOffset = offset;

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
		changed |= ImGui::InputFloat("##cost", &cost, 0.0f, 0.0f, 1, ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor(1);
	}
	else
	{
		changed |= ImGui::InputFloat("##cost", &cost, 0.0f, 0.0f, 1);
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
	// Camera Reset
	if (m_geom)
	{
		const glm::vec3& bmin = m_geom->getMeshBoundsMin();
		const glm::vec3& bmax = m_geom->getMeshBoundsMax();

		// Reset camera and fog to match the mesh bounds.
		m_camr = sqrtf(rcSqr(bmax[0] - bmin[0]) +
			rcSqr(bmax[1] - bmin[1]) +
			rcSqr(bmax[2] - bmin[2])) / 2;
		m_cam.x = (bmax[0] + bmin[0]) / 2 + m_camr;
		m_cam.y = (bmax[1] + bmin[1]) / 2 + m_camr;
		m_cam.z = (bmax[2] + bmin[2]) / 2 + m_camr;

		m_camr *= 3;
		m_r.x = 45;
		m_r.y = -45;

		glFogf(GL_FOG_START, m_camr * 0.2f);
		glFogf(GL_FOG_END, m_camr * 1.25f);
	}
}

std::string Application::GetMeshFilename()
{
	std::stringstream ss;
	ss << m_eqConfig.GetOutputPath() << "\\MQ2Nav\\" << m_zoneShortname << ".navmesh";

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

	auto ptr = std::make_unique<InputGeom>(
		zoneShortName, eqPath, outputPath);

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
	SDL_SetWindowTitle(m_window, windowTitle.c_str());

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

void Application::DispatchCallbacks()
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
	settingsPath /= "Setings";
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
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiSetCond_Appearing);

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
				ImGui::TextColored(ImColor(255, 0, 0), ICON_FA_EXCLAMATION_TRIANGLE " File is missing");
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
