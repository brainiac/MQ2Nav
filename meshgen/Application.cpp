//
// Application.cpp
//

#include "meshgen/Application.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/Editor.h"
#include "meshgen/Logging.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/RenderManager.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZonePicker.h"

#include "imgui/fonts/IconsLucide.h"
#include "imgui/fonts/IconsLucide.h_lucide.ttf.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiUtils.h"

#include <bx/math.h>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <sstream>


namespace fs = std::filesystem;

//============================================================================

Application* Application::instance = nullptr;

static std::thread::id main_thread_id;

Application::Application()
{
	Logging::Initialize();
	g_config.Initialize();
	Logging::InitSecondStage(this);

	instance = this;
	main_thread_id = std::this_thread::get_id();

	//_putenv("TF_ENABLE_PROFILER=taskflow.json");

	SPDLOG_INFO("EverQuest Path: {}", g_config.GetEverquestPath());
	SPDLOG_INFO("MacroQuest Path: {} IsValid={}", g_config.GetMacroQuestPath(), g_config.IsMacroQuestPathValid());
	SPDLOG_INFO("Config Path: {}", g_config.GetConfigPath());
	SPDLOG_INFO("Resources Path: {}", g_config.GetResourcesPath());
	SPDLOG_INFO("Logging Path: {}", g_config.GetLogsPath());
	SPDLOG_INFO("Output Path: {}", g_config.GetOutputPath());

	m_backgroundTaskManager = std::make_unique<BackgroundTaskManager>();

	g_render = new RenderManager();
	g_resourceMgr = new ResourceManager();

	m_editor = std::make_unique<Editor>();
}

Application::~Application()
{
	m_editor.reset();

	delete g_resourceMgr;
	delete g_render;

	Logging::Shutdown();

	instance = nullptr;
}

/* static */
bool Application::IsMainThread()
{
	return std::this_thread::get_id() == main_thread_id;
}

int Application::Run(int argc, const char* const* argv)
{
	Application app;

	if (!app.Initialize(argc, argv))
	{
		return 1;
	}

	while (true)
	{
		if (!app.Update())
			break;
	}

	return app.Shutdown();
}

bool Application::Initialize(int32_t argc, const char* const* argv)
{
	if (!InitSystem())
		return false;

	for (int i = 0; i < argc; ++i)
	{
		m_args.push_back(argv[i]);
	}

	return true;
}

bool Application::InitSystem()
{
	SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER);

	if (!g_config.GetSavedWindowDimensions(m_windowRect))
	{
		m_windowRect = {
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1600,
			900
		};
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

	if (!g_render->Initialize(m_windowRect.z, m_windowRect.w, m_window))
		return false;
	if (!g_resourceMgr->Initialize())
		return false;

	InitImGui();

	m_lastTime = SDL_GetTicks64();
	m_time = 0.0f;

	m_editor->OnInit();

	return true;
}

ImFont* LCIconFont = nullptr;
ImFont* LCIconFontLarge = nullptr;

void Application::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGuiContext* context = ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	fs::path configPath = g_config.GetConfigPath();

	// this requires permanent storage
	m_iniFile = (configPath / "MeshGenerator_UI.ini").string();
	io.IniFilename = m_iniFile.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	mq::imgui::ConfigureFonts(io.Fonts);
	mq::imgui::ConfigureStyle();

	{
		// Lucide Font @ 14px
		ImFontConfig fontConfig;
		fontConfig.GlyphMinAdvanceX = 14.0f;
		strcpy_s(fontConfig.Name, "Lucide");
		constexpr ImWchar icon_ranges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
		LCIconFont = io.Fonts->AddFontFromMemoryCompressedTTF(GetLucideIconsCompressedData(), GetLucideIconsCompressedSize(), 14.0f, &fontConfig, icon_ranges);
	}

	{
		// Lucide Font @ 22px
		ImFontConfig fontConfig;
		strcpy_s(fontConfig.Name, "Lucide (Large)");
		constexpr ImWchar icon_ranges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
		LCIconFontLarge = io.Fonts->AddFontFromMemoryCompressedTTF(GetLucideIconsCompressedData(), GetLucideIconsCompressedSize(), 20.0f, &fontConfig, icon_ranges);
	}

	io.Fonts->Build();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	style.FrameRounding = 4;
	style.FrameBorderSize = 1.0f;
	style.ScrollbarRounding = 8;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.54f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
	colors[ImGuiCol_Separator] = ImVec4(0.75f, 0.75f, 0.75f, 0.38f);

	ImGui_Impl_Init(m_window, context);
}

int Application::Shutdown()
{
	m_backgroundTaskManager->Stop();
	m_editor->OnShutdown();

	g_resourceMgr->Shutdown();
	g_render->Shutdown();
	return 0;
}

bool Application::Update()
{
	if (m_done)
		return false;

	// Update our timestep
	uint64_t time = SDL_GetTicks64();
	m_timeStep = (time - m_lastTime) / 1000.0f;
	m_lastTime = time;
	m_time += m_timeStep;

	m_backgroundTaskManager->Process();
	if (m_done)
		return false;

	if (!HandleEvents())
		return false;

	g_render->BeginFrame(m_timeStep, m_editor->GetCamera());

	m_editor->OnUpdate(m_timeStep);

	RenderImGui();

	g_render->EndFrame();
	return true;
}

bool Application::HandleEvents()
{
	auto& io = ImGui::GetIO();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

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
					g_config.SetSavedWindowDimensions(m_windowRect);

				g_render->SetWindowSize(m_windowRect.z, m_windowRect.w);

				m_editor->OnWindowSizeChanged(event.window);
			}
			else if (event.window.event == SDL_WINDOWEVENT_MOVED)
			{
				m_windowRect.x = event.window.data1;
				m_windowRect.y = event.window.data2;

				Uint32 flags = SDL_GetWindowFlags(m_window);
				bool isMaximized = flags & SDL_WINDOW_MAXIMIZED;
				if (!isMaximized)
					g_config.SetSavedWindowDimensions(m_windowRect);
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
					g_config.SetSavedWindowDimensions(m_windowRect);
			}
			else if (event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
			{
				SPDLOG_INFO("Window Maximized");
			}
			else if (event.window.event == SDL_WINDOWEVENT_RESTORED)
			{
				SPDLOG_INFO("Window Restored");
			}
			else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
			{
				SPDLOG_INFO("Window Minimized");
			}
			break;

		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_F4
				&& (event.key.keysym.mod & KMOD_ALT))
			{
				m_done = true;
			}
			else
			{
				m_editor->OnKeyDown(event.key);
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			m_editor->OnMouseDown(event.button);
			break;

		case SDL_MOUSEBUTTONUP:
			m_editor->OnMouseUp(event.button);
			break;

		case SDL_MOUSEMOTION:
			m_editor->OnMouseMotion(event.motion);
			break;

		case SDL_QUIT:
			m_done = true;
			break;

		default:
			break;
		}
	}

	return !m_done;
}

void Application::RenderImGui()
{
	g_render->BeginImGui(m_timeStep, m_editor->GetCamera());

	m_editor->OnImGuiRender();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Application::SetWindowTitle(const std::string& title)
{
	SDL_SetWindowTitle(m_window, title.c_str());
}

int main(int argc, char* argv[])
{
	return Application::Run(argc, argv);
}
