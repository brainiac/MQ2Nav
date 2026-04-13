#pragma once

#include "meshgen/PanelManager.h"

#include "entt/entity/fwd.hpp"

class Editor;
class Scene;

class SceneObjectsPanel : public PanelWindow
{
public:
	explicit SceneObjectsPanel(Editor* app);
	virtual ~SceneObjectsPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	void DrawEntityRow(const entt::handle& handle);

private:
	Editor* m_editor;
	std::shared_ptr<Scene> m_scene;
	std::shared_ptr<ZoneProject> m_project;

	float m_distanceFilter = 100.0f;
	bool m_filterByDistance = false;
};
