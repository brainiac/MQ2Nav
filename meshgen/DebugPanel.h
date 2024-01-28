
#pragma once

#include "meshgen/PanelManager.h"

class Application;

class DebugPanel : public PanelWindow
{
public:
	DebugPanel(Application* app);
	virtual ~DebugPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

private:
	Application* m_app;
};
