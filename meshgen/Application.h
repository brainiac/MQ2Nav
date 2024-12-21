//
// Application.h
//

// Main GUI interface for EQNavigation

#pragma once

#include "common/NavMesh.h"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <string>

class BackgroundTaskManager;
class Editor;

extern ImFont* LCIconFont;
extern ImFont* LCIconFontLarge;

class Application
{
public:
	Application();
	virtual ~Application();

	static Application& Get() { return *instance; }
	static int Run(int argc, const char* const* argv);

	void Quit() { m_done = true; }

	const std::vector<std::string>& GetArgs() const { return m_args; }

	BackgroundTaskManager& GetBackgroundTaskManager() const { return *m_backgroundTaskManager.get(); }

	static bool IsMainThread();

	void SetWindowTitle(const std::string& title);

private:
	bool Initialize(int argc, const char* const* argv);
	int Shutdown();
	bool Update();

	bool InitSystem();
	void InitImGui();
	void RenderImGui();

	// input event handling
	bool HandleEvents();

private:
	static Application* instance;

	// Application args at startup
	std::vector<std::string> m_args;

	// rendering requires a hold on this lock. Used for updating the
	// render state from other threads
	std::mutex m_renderMutex;

	// Background Task Manager manages asynchronous tasks
	std::unique_ptr<BackgroundTaskManager> m_backgroundTaskManager;

	// ImGui Ini file.
	std::string m_iniFile;

	HWND              m_hWnd = nullptr;
	SDL_Window*       m_window = nullptr;
	glm::ivec4        m_windowRect = { 0, 0, 0, 0 };
	uint64_t          m_lastTime = 0;
	float             m_time = 0.0f;
	float             m_timeStep = 0;
	bool              m_done = false;

	std::unique_ptr<Editor> m_editor;
};
