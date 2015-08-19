// Main GUI interface for EQNavigation

#pragma once

#include "Recast.h"
#include "PerfTimer.h"

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <deque>

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
	
private:
	bool InitializeWindow();
	void DestroyWindow();

	void RenderInterface();

	void HandleEvents();

	// Load the list of maps.
	void LoadZoneList();


	void StartBuild();
	static DWORD WINAPI BuildThread(LPVOID lpParam);
	void BuildThreadImpl();

	void LoadGeometry();

	void Halt();

private:
	// currently chosen path to everquest installation
	std::string m_everquestPath;

	// currently chosen path to the output folder. We will put navmeshes here
	std::string m_outputPath;

	// loaded maps, keyed by their expansion group. Data is loaded from Zones.ini
	typedef std::pair<std::string /*shortName*/, std::string /*longName*/> ZoneNamePair;
	std::multimap<std::string, ZoneNamePair> m_loadedMaps;

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

	// The main window surface
	SDL_Surface* m_screen;

	// default zone to load, if any
	std::string m_defaultZone;

	// rendering properties
	bool m_resetCamera;
	int m_width, m_height;
	float m_progress;
	std::string m_activityMessage;
	bool m_mouseOverMenu;
	bool m_showDebugMode;
	bool m_showPreview;
	bool m_showTools;
	bool m_showLog;
	bool m_showLevels;
	bool m_showSample;
	bool m_showTestCases;

	GLdouble m_proj[16];
	GLdouble m_model[16];
	GLint m_view[4];

	float m_rays[3], m_raye[3];
	float m_origrx = 0, m_origry = 0;
	int m_origx = 0, m_origy = 0;
	float m_rx = 45;
	float m_ry = -45;
	float m_moveW = 0, m_moveS = 0, m_moveA = 0, m_moveD = 0;
	float m_camx = 0, m_camy = 0, m_camz = 0, m_camr = 10;

	uint32_t m_lastTime = 0;
	float m_time = 0;
	float m_timeDelta = 0;

	// mesh hittest
	bool m_mposSet = false;
	float m_mpos[3] = { 0,0,0 };

	// events
	int m_mscroll = 0;
	uint8_t m_mbuttons = 0;
	int m_mx = 0, m_my = 0;
	bool m_rotate = false;
	bool m_done = false;
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
	TimeVal m_startTime[RC_MAX_TIMERS];
	int32_t m_accTime[RC_MAX_TIMERS];

	std::deque<std::string> m_logs;
};