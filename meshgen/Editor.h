//
// Editor.h
//

#pragma once

#include "meshgen/Camera.h"
#include "meshgen/ZoneProject.h"

#include <glm/glm.hpp>
#include <SDL2/SDL_events.h>
#include <memory>


class BackgroundTaskManager;
class ImportExportSettingsDialog;
class NavMeshProject;
class NavMeshTool;
class PanelManager;
class SelectionManager;
class ZonePicker;

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

	void OnProjectLoaded(const std::shared_ptr<ZoneProject>& project);

	PanelManager* GetPanelManager() const { return m_panelManager.get(); }
	NavMeshTool& GetMeshTool() const { return *m_meshTool; }
	Camera& GetCamera() { return m_camera; }

	std::shared_ptr<ZoneProject> GetProject() const { return m_project; }

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

	std::tuple<glm::vec3, glm::vec3> CastRay(float mx, float my);

	void ShowZonePicker();
	void ShowSettingsDialog();
	void ShowImportExportSettingsDialog(bool import);

	void CreateEmptyProject();
	void OpenProject(const std::string& name, bool loadMesh);
	void CloseProject();

	void SetProject(const std::shared_ptr<ZoneProject>& zoneProject);
	void SetNavMeshProject(const std::shared_ptr<NavMeshProject>& navMeshProject);

	void UpdateCamera(float timeStep);

private:
	Camera m_camera;
	glm::ivec4 m_viewport;
	glm::ivec2 m_viewportMouse;

	std::unique_ptr<SelectionManager> m_selectionMgr;
	std::shared_ptr<ZoneProject> m_project;
	std::shared_ptr<Scene> m_scene;

	std::unique_ptr<PanelManager> m_panelManager;         // UI Panel Manager
	std::unique_ptr<ZonePicker>   m_zonePicker;           // UI Zone Picker
	std::unique_ptr<ImportExportSettingsDialog> m_importExportSettings; // UI Import/Export Settings Dialog
	std::unique_ptr<NavMeshTool> m_meshTool;              // The mesh tool that we use to build/manipulate the mesh

	bool m_showSettingsDialog = false;
	bool m_showDemo = false;
	bool m_showMetricsWindow = false;
	bool m_showOverlay = true;
	bool m_showMapAreas = false;
	InteractMode m_interactMode = InteractMode::Select;
	float m_timeAccumulator = 0.0f;

	// FIXME
	glm::ivec2 m_mousePos;
	glm::ivec2 m_lastMouseLook;
	bool m_rotate = false;
};
