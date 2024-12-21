
#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class NavMeshProject;

class ToolsPanel : public PanelWindow
{
public:

	ToolsPanel(Editor* app);
	~ToolsPanel() override;

	void OnImGuiRender(bool* p_open) override;

private:
	Editor* m_editor;
};
