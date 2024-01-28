
#pragma once

#include "meshgen/PanelManager.h"

class Application;

class ToolsPanel : public PanelWindow
{
public:
	ToolsPanel(Application* app);
	virtual ~ToolsPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

private:
	Application* m_app;
};
