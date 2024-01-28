#pragma once

#include "meshgen/PanelManager.h"

class Application;

class PropertiesPanel : public PanelWindow
{
public:
	PropertiesPanel(Application* app);
	virtual ~PropertiesPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

private:
	Application* m_app;
};
