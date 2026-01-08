#pragma once


#include "meshgen/PanelManager.h"
#include "entt/entity/fwd.hpp"

class Editor;
class Scene;

class SceneHierarchyPanel : public PanelWindow
{
public:
	explicit SceneHierarchyPanel(Editor* app);
	virtual ~SceneHierarchyPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	void DrawEntityRow(const entt::handle& handle);

private:
	Editor* m_editor;
	std::shared_ptr<Scene> m_scene;
	std::shared_ptr<ZoneProject> m_project;
};
