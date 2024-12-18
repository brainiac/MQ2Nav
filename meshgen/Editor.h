//
// Editor.h
//

#pragma once

#include "meshgen/Camera.h"

#include <glm/glm.hpp>

class BackgroundTaskManager;
class ImportExportSettingsDialog;
class ZoneContext;
class ZonePicker;
class NavMeshTool;
class PanelManager;

enum class InteractMode
{
	Select,
	Move,
	Rotate,
	Translate
};

class Editor
{
public:
	Editor();
	~Editor();

	void OnInit();
	void OnShutdown();

	void OnUpdate(float timeStep);
	void OnImGuiRender();

	void OnKeyDown(const SDL_KeyboardEvent& event);
	void OnMouseDown(const SDL_MouseButtonEvent& event);
	void OnMouseUp(const SDL_MouseButtonEvent& event);
	void OnMouseMotion(const SDL_MouseMotionEvent& event);
	void OnWindowSizeChanged(const SDL_WindowEvent& event);

	PanelManager* GetPanelManager() const { return m_panelManager.get(); }
	NavMeshTool& GetMeshTool() const { return *m_meshTool; }
	Camera& GetCamera() { return m_camera; }

	// Set the zone context. This is called when the zone is loaded.
	void SetZoneContext(const std::shared_ptr<ZoneContext>& zoneContext);
	std::shared_ptr<ZoneContext> GetZoneContext() const { return m_zoneContext; }

	std::shared_ptr<ZoneContext> GetLoadingZoneContext() const { return m_loadingZoneContext; }

	void ShowNotificationDialog(const std::string& title, const std::string& message, bool modal = true);

private:
	void UI_UpdateViewport();
	void UI_DrawMainMenuBar();
	void UI_DrawToolbar();
	void UI_DrawZonePicker();
	void UI_DrawSettingsDialog();
	void UI_DrawOverlayText();
	void UI_DrawImportExportSettingsDialog();
	void UI_DrawAreaTypesEditor();

	bool IsBlockingOperationActive();

	void ShowZonePicker();
	void ShowSettingsDialog();
	void ShowImportExportSettingsDialog(bool import);

	void OpenZone(const std::string& shortZoneName, bool loadMesh);

	void OpenMesh();
	void SaveMesh();
	void Halt();

	// FIXME
	void ResetCamera();
	void UpdateCamera(float timeStep);

private:
	// Our main camera
	Camera m_camera;

	// UI Panel Manager
	std::unique_ptr<PanelManager> m_panelManager;

	// UI Zone Picker
	std::unique_ptr<ZonePicker> m_zonePicker;

	// UI Import/Export Settings Dialog
	std::unique_ptr<ImportExportSettingsDialog> m_importExportSettings;

	// The mesh tool that we use to build/manipulate the mesh
	std::unique_ptr<NavMeshTool> m_meshTool;

	// The currently active zone context
	std::shared_ptr<ZoneContext> m_zoneContext;

	// The currently loading zone context
	std::shared_ptr<ZoneContext> m_loadingZoneContext;

	float m_timeAccumulator = 0.0f;

	bool m_showSettingsDialog = false;
	bool m_showDemo = false;
	bool m_showMetricsWindow = false;
	bool m_showOverlay = true;
	bool m_showMapAreas = false;
	InteractMode m_interactMode = InteractMode::Select;

	// FIXME
	glm::ivec2 m_mousePos;
	glm::ivec2 m_lastMouseLook;
	bool m_rotate = false;
	glm::ivec2 m_windowSize;
};
