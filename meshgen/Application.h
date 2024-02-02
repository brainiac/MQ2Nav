//
// Application.h
//

// Main GUI interface for EQNavigation

#pragma once

#include "meshgen/Camera.h"
#include "common/NavMesh.h"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <string>
#include <mutex>

class BackgroundTaskManager;
class RecastContext;
class NavMeshTool;
class ZonePicker;
class ImportExportSettingsDialog;
class rcContext;
class PanelManager;
class ZoneContext;

// TEMP
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
	bool Update();

	//------------------------------------------------------------------------

	void BeginLoadZone(const std::string& shortName, bool loadMesh);
	void FinishLoading(const std::shared_ptr<ZoneContext>& context, bool success, bool clearLoading);

	// Set the zone context. This is called when the zone is loaded.
	void SetZoneContext(const std::shared_ptr<ZoneContext>& zoneContext);
	std::shared_ptr<ZoneContext> GetZoneContext() const { return m_zoneContext; }

	std::shared_ptr<ZoneContext> GetLoadingZoneContext() const { return m_loadingZoneContext; }


	void ShowNotificationDialog(const std::string& title, const std::string& message, bool modal = true);

	PanelManager* GetPanelManager() const { return m_panelManager.get(); }
	BackgroundTaskManager* GetBackgroundTaskManager() const { return m_backgroundTaskManager.get(); }

private:
	bool InitSystem();
	void InitImGui();
	void UpdateImGui();

	void Halt();

	// Reset the camera to the starting point
	void ResetCamera();

	// Menu Item Handling
	void OpenMesh();
	void SaveMesh();

	// input event handling
	bool HandleEvents();

	void UpdateCamera();
	void UpdateViewport();

	void DrawAreaTypesEditor();
	void ShowImportExportSettingsDialog(bool import);
	void ShowSettingsDialog();

private:
	HWND              m_hWnd = nullptr;
	SDL_Window*       m_window = nullptr;
	glm::ivec4        m_windowRect = { 0, 0, 0, 0 };

	// The build context. Everything passes this around. We own it.
	std::unique_ptr<rcContext> m_rcContext;

	// rendering requires a hold on this lock. Used for updating the
	// render state from other threads
	std::mutex m_renderMutex;

	// The nav mesh object
	std::shared_ptr<NavMesh> m_navMesh;

	// The mesh tool that we use to build/manipulate the mesh
	std::unique_ptr<NavMeshTool> m_meshTool;

	// UI Panel Manager
	std::unique_ptr<PanelManager> m_panelManager;

	// ImGui Ini file.
	std::string m_iniFile;

	uint64_t m_lastTime = 0;
	float m_time = 0.0f;
	float m_timeDelta = 0;
	float m_timeAccumulator = 0;

	// events
	glm::ivec2 m_mousePos;
	glm::ivec2 m_lastMouseLook;
	bool m_rotate = false;
	bool m_done = false;

	// maps display
	bool m_zoneLoaded = false;
	bool m_showSettingsDialog = false;
	bool m_showMapAreas = false;
	bool m_showOverlay = true;
	bool m_showDemo = false;
	bool m_showMetricsWindow = false;

	std::unique_ptr<BackgroundTaskManager> m_backgroundTaskManager;
	std::unique_ptr<ZonePicker> m_zonePicker;
	std::unique_ptr<ImportExportSettingsDialog> m_importExportSettings;
	std::shared_ptr<ZoneContext> m_zoneContext;
	std::shared_ptr<ZoneContext> m_loadingZoneContext;

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
