// Main GUI interface for EQNavigation

#pragma once

#include "EQConfig.h"
#include "Recast.h"

#include <SDL.h>
#include <glm/glm.hpp>

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>

class BuildContext;
class Sample_TileMesh;
struct SDL_Surface;
class InputGeom;

class Interface
{
public:
	Interface(const std::string& defaultZone = std::string());
	~Interface();

	BuildContext& GetContext() { return *m_context; }

	// run the main event loop. This doesn't return until the program is ready to exit.
	// Return code is the result to exit with
	int RunMainLoop();

	//------------------------------------------------------------------------

	void ShowZonePickerDialog();
	void ShowSettingsDialog();

private:
	bool InitializeWindow();
	void DestroyWindow();

	void RenderInterface();


	// Load a zone's geometry given its shortname.
	void LoadGeometry(const std::string& zoneShortName);
	void Halt();

	// Reset the camera to the starting point
	void ResetCamera();

	// Get the filename of the mesh that would be used to save/open based on current zone
	std::string GetMeshFilename();

	enum Theme { DefaultTheme, LightTheme, DarkTheme1, DarkTheme2, DarkTheme3 };
	void SetTheme(Theme theme, bool force = false);

	// Menu Item Handling
	void OpenMesh();
	void SaveMesh();

	// input event handling
	void HandleEvents();

	void UpdateMovementState(bool keydown);
	void UpdateCamera();

private:
	EQConfig m_eqConfig;

	// short name of the currently loaded zone
	std::string m_zoneShortname;

	// long name of the currently loaded zone
	std::string m_zoneLongname;

	// rendering requires a hold on this lock. Used for updating the
	// render state from other threads
	std::mutex m_renderMutex;

	// The build context. Everything passes this around. We own it.
	std::unique_ptr<BuildContext> m_context;

	// The mesh that we use to generate the tiled navmesh
	std::unique_ptr<Sample_TileMesh> m_mesh;

	// The input geometry (??)
	std::unique_ptr<InputGeom> m_geom;

	// The current theme
	Theme m_currentTheme = DefaultTheme;

	// rendering properties
	bool m_resetCamera;
	int m_width, m_height;
	float m_progress;
	std::string m_activityMessage;
	bool m_showLog;
	bool m_showSample;
	bool m_showTools = false;
	bool m_showFailedToOpenDialog = false;

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

	glm::vec3 m_cam;
	float m_camr = 10;

	uint32_t m_lastTime = 0;
	float m_time = 0;
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

	// zone to load on next pass
	std::string m_nextZoneToLoad;

	// The main window surface
	SDL_Window* m_window = nullptr;
	SDL_GLContext m_glContext = 0;

	std::string m_iniFile;

	bool m_showFailedToLoadZone = false;
	std::string m_failedZoneMsg;

	// current navmesh build worker thread
	std::thread m_buildThread;
};


class BuildContext : public rcContext
{
public:
	BuildContext();
	virtual ~BuildContext();

	// Dumps the log to stdout
	void dumpLog(const char* format, ...);

	// Returns the number of log messages
	int getLogCount() const;

	// Returns the log message text
	const char* getLogText(int32_t index) const;

protected:
	virtual void doResetLog() override;
	virtual void doLog(const rcLogCategory category, const char* msg, const int len) override;
	virtual void doResetTimers() override;
	virtual void doStartTimer(const rcTimerLabel label) override;
	virtual void doStopTimer(const rcTimerLabel label) override;
	virtual int doGetAccumulatedTime(const rcTimerLabel label) const override;

private:
	std::chrono::steady_clock::time_point m_startTime[RC_MAX_TIMERS];
	std::chrono::nanoseconds m_accTime[RC_MAX_TIMERS];

	std::deque<std::string> m_logs;
	mutable std::mutex m_mtx;
};
