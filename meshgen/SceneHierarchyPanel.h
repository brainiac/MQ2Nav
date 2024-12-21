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

private:
	Editor* m_editor;
};
