#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class ZoneScene;

class PropertiesPanel : public PanelWindow
{
public:
	PropertiesPanel(Editor* app);
	~PropertiesPanel() override;

	void OnImGuiRender(bool* p_open) override;

private:
	Editor* m_editor;
};
