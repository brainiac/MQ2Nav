#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class Scene;

class SceneHierarchyPanel : public PanelWindow
{
public:
	explicit SceneHierarchyPanel(Editor* app);
	~SceneHierarchyPanel() override;

	void OnImGuiRender(bool* p_open) override;

	void SetScene(const std::shared_ptr<Scene>& scene);

private:
	Editor* m_editor;
	std::shared_ptr<Scene> m_scene;
};
