
#pragma once

#include "meshgen/PanelManager.h"

class Editor;

class DebugPanel : public PanelWindow
{
public:
	DebugPanel(Editor* app);
	virtual ~DebugPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

private:
	Editor* m_editor;
};
