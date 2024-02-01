#pragma once

#include "meshgen/PanelManager.h"

class Application;

class PropertiesPanel : public PanelWindow
{
public:
	PropertiesPanel(Application* app);
	~PropertiesPanel() override;

	void OnImGuiRender(bool* p_open) override;
	void SetZoneContext(const std::shared_ptr<ZoneContext>& context) override;

private:
	Application* m_app;
	std::shared_ptr<ZoneContext> m_zoneContext;
	std::string m_loadedName;
};
