//
// Application.h
//

// Main GUI interface for EQNavigation

#pragma once

#include "meshgen/Camera.h"
#include "meshgen/ApplicationConfig.h"
#include "common/NavMesh.h"
#include "common/Utilities.h"
#include "imgui/ImGuiUtils.h"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <spdlog/spdlog.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <mutex>
#include <thread>

class RecastContext;
class InputGeom;
class NavMeshTool;
class ZonePicker;
class ImportExportSettingsDialog;
class rcContext;
class PanelManager;

// TEMP
class PropertiesPanel;
class DebugPanel;
class ToolsPanel;

class Application
{
	friend class DebugPanel;
	friend class PropertiesPanel;
	friend class ToolsPanel;

public:
	Application();
	virtual ~Application();

	bool Initialize(int argc, const char* const* argv);
	int Shutdown();

	bool update();

	rcContext& GetContext() { return *m_rcContext; }

	//------------------------------------------------------------------------

	void ShowSettingsDialog();

	void PushEvent(const std::function<void()>& cb);


private:
	bool InitSystem();
	void InitImGui();
	void UpdateImGui();

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

	void UpdateCamera();

	void DrawAreaTypesEditor();
	void ShowImportExportSettingsDialog(bool import);

	void DrawZonePickerDialog();
	void ShowZonePickerDialog();

	void UpdateViewport();

private:
	HWND              m_hWnd = nullptr;
	SDL_Window*       m_window = nullptr;
	glm::ivec4        m_windowRect = { 0, 0, 0, 0 };

	// The build context. Everything passes this around. We own it.
	std::unique_ptr<rcContext> m_rcContext;

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

	// UI Panel Manager
	std::unique_ptr<PanelManager> m_panelManager;


	float m_progress = 0.0f;
	std::string m_activityMessage;

	bool m_showLog = false;
	bool m_showFailedToOpenDialog = false;

	uint64_t m_lastTime = 0;
	float m_time = 0.0f;
	float m_timeDelta = 0;
	float m_timeAccumulator = 0;

	// mesh hittest
	bool m_mposSet = false;
	glm::vec3 m_mpos;

	// events
	glm::ivec2 m_mousePos;
	glm::ivec2 m_lastMouseLook;
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

	bool m_showFailedToLoadZone = false;
	std::string m_failedZoneMsg;

	// current navmesh build worker thread
	std::thread m_buildThread;

	std::unique_ptr<ZonePicker> m_zonePicker;
	std::unique_ptr<ImportExportSettingsDialog> m_importExportSettings;

	std::vector<std::function<void()>> m_callbackQueue;
	std::mutex m_callbackMutex;
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
