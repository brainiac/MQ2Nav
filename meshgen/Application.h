//
// Application.h
//

// Main GUI interface for EQNavigation

#pragma once

#include "meshgen/EQConfig.h"
#include "common/NavMesh.h"
#include "common/Utilities.h"
#include "imgui/ImGuiUtils.h"

#include <Recast.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>

class RecastContext;
class InputGeom;
class NavMeshTool;
class ZonePicker;
class ImportExportSettingsDialog;

struct ImGuiConsoleLog
{
	ImGuiTextBuffer Buf;
	ImGuiTextFilter Filter;
	ImVector<int>   Offsets;
	bool            ScrollToBottom;

	void Clear()
	{
		Buf.clear();
		Offsets.clear();
	}

	void AddLog(const char* string)
	{
		int old_size = Buf.size();
		Buf.appendf("%s", string);
		for (int new_size = Buf.size(); old_size < new_size; old_size++)
		{
			if (Buf[old_size] == '\n')
				Offsets.push_back(old_size);
		}
		ScrollToBottom = true;
	}

	void Draw(const char* title, bool* p_opened = nullptr)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		ImGui::Begin(title, p_opened);
		if (ImGui::Button("Clear")) Clear();
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		Filter.Draw("Filter", -100.0f);
		ImGui::Separator();
		ImGui::BeginChild("scrolling");
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
		if (copy) ImGui::LogToClipboard();
		ImGui::PushFont(mq::imgui::ConsoleFont);

		if (Filter.IsActive())
		{
			const char* buf_begin = Buf.begin();
			const char* line = buf_begin;
			for (int line_no = 0; line != nullptr; line_no++)
			{
				const char* line_end = (line_no < Offsets.Size) ? buf_begin + Offsets[line_no] : nullptr;
				if (Filter.PassFilter(line, line_end))
					ImGui::TextUnformatted(line, line_end);
				line = line_end && line_end[1] ? line_end + 1 : nullptr;
			}
		}
		else
		{
			ImGui::TextUnformatted(Buf.begin());
		}

		ImGui::PopFont();

		if (ScrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;

		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::End();
	}
};

class Application
{
public:
	Application();
	virtual ~Application();

	bool Initialize(int argc, const char* const* argv);
	int shutdown();

	bool update();

	RecastContext& GetContext() { return *m_rcContext; }

	//------------------------------------------------------------------------

	void ShowZonePickerDialog();
	void ShowSettingsDialog();

	void PushEvent(const std::function<void()>& cb);

	void AddLog(const std::string& message)
	{
		m_console.AddLog(message.c_str());
	}

private:
	bool InitSystem();
	void RenderInterface();


	// Load a zone's geometry given its shortname.
	void LoadGeometry(const std::string& zoneShortName, bool loadMesh);
	void Halt();

	// Reset the camera to the starting point
	void ResetCamera();

	// Get the filename of the mesh that would be used to save/open based on current zone
	std::string GetMeshFilename();

	// Menu Item Handling
	void OpenMesh();
	void SaveMesh();

	// input event handling
	bool HandleEvents();
	bool HandleCallbacks();


	void UpdateMovementState(bool keydown);
	void UpdateCamera();

	void DrawAreaTypesEditor();
	void ShowImportExportSettingsDialog(bool import);


private:
	EQConfig m_eqConfig;

	HWND           m_hWnd = nullptr;
	SDL_Window*    m_window = nullptr;


	// The build context. Everything passes this around. We own it.
	std::unique_ptr<RecastContext> m_rcContext;

	// short name of the currently loaded zone
	std::string m_zoneShortname;

	// long name of the currently loaded zone
	std::string m_zoneLongname;

	// rendering requires a hold on this lock. Used for updating the
	// render state from other threads
	std::mutex m_renderMutex;

	// The nav mesh object
	std::shared_ptr<NavMesh> m_navMesh;

	// The mesh tool that we use to build/manipulate the mesh
	std::unique_ptr<NavMeshTool> m_meshTool;

	// The input geometry (??)
	std::unique_ptr<InputGeom> m_geom;

	// rendering properties
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;
	bool m_resetCamera = true;

	float m_progress = 0.0f;
	std::string m_activityMessage;

	bool m_showLog = false;
	bool m_showFailedToOpenDialog = false;

	glm::vec3 m_cam;
	float m_camr = 10;

	glm::mat4 m_proj;
	glm::mat4 m_model;
	glm::ivec4 m_view;
	glm::vec3 m_rays; // ray start
	glm::vec3 m_raye; // ray end
	glm::vec2 m_origr;
	glm::ivec2 m_orig;
	glm::vec2 m_r;

	float m_moveW = 0, m_moveS = 0;
	float m_moveA = 0, m_moveD = 0;
	float m_moveUp = 0, m_moveDown = 0;
	float m_moveSpeed = 0.0f;

	uint32_t m_lastTime = 0;
	float m_time = 0.0f;
	float m_timeDelta = 0;

	// mesh hittest
	bool m_mposSet = false;
	glm::vec3 m_mpos;

	// events
	glm::vec2 m_m;
	bool m_rotate = false;
	bool m_done = false;

	// log window
	float m_lastScrollPosition = 1.0, m_lastScrollPositionMax = 1.0;

	// maps display
	std::map<std::string, bool> m_expansionExpanded;
	std::string m_zoneDisplayName;
	bool m_zoneLoaded = false;
	bool m_showZonePickerDialog = false;
	bool m_showSettingsDialog = false;
	bool m_showDemo = false;
	bool m_showTools = true;
	bool m_showDebug = false;
	bool m_showProperties = true;
	bool m_showMapAreas = false;
	bool m_showOverlay = true;
	
	// Keyboard speed adjustment
	float m_camMoveSpeed = 250.0f;

	// zone to load on next pass
	std::string m_nextZoneToLoad;
	bool m_loadMeshOnZone = false;

	std::string m_iniFile;
	std::string m_logFile;

	bool m_showFailedToLoadZone = false;
	std::string m_failedZoneMsg;

	// current navmesh build worker thread
	std::thread m_buildThread;

	std::unique_ptr<ZonePicker> m_zonePicker;
	std::unique_ptr<ImportExportSettingsDialog> m_importExportSettings;

	std::vector<std::function<void()>> m_callbackQueue;
	std::mutex m_callbackMutex;

	ImGuiConsoleLog m_console;
};

template <typename Mutex>
class ConsoleLogSink : public spdlog::sinks::base_sink<Mutex>
{
public:
	ConsoleLogSink(Application* application)
		: m_application(application)
	{}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		this->formatter_->format(msg, formatted);

		m_application->AddLog(fmt::to_string(formatted).c_str());
	}

	void flush_() override {}

private:
	Application* m_application;
};

//----------------------------------------------------------------------------

class RecastContext : public rcContext
{
public:
	RecastContext();
	virtual ~RecastContext() = default;

protected:
	virtual void doResetLog() override {}
	virtual void doLog(const rcLogCategory category, const char* msg, const int len) override;
	virtual void doResetTimers() override;
	virtual void doStartTimer(const rcTimerLabel label) override;
	virtual void doStopTimer(const rcTimerLabel label) override;
	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override;

private:
	std::shared_ptr<spdlog::logger> m_logger;
	std::array<std::chrono::steady_clock::time_point, RC_MAX_TIMERS> m_startTime;
	std::array<std::chrono::nanoseconds, RC_MAX_TIMERS> m_accTime;
};

//----------------------------------------------------------------------------

class ImportExportSettingsDialog
{
public:
	ImportExportSettingsDialog(const std::shared_ptr<NavMesh>& navMesh, bool import);

	void Show(bool* open = nullptr);

private:
	bool m_import = false;
	bool m_failed = false;
	bool m_fileMissing = false;
	bool m_firstShow = true;
	std::weak_ptr<NavMesh> m_navMesh;
	std::unique_ptr<char[]> m_defaultFilename;

	// by default load all fields except for tiles
	PersistedDataFields m_fields = PersistedDataFields::All
		& ~PersistedDataFields::MeshTiles;
};
