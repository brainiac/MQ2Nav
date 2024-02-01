
#pragma once

#include "meshgen/PanelManager.h"

class Application;
class ZoneContext;

class ToolsPanel : public PanelWindow
{
public:

	ToolsPanel(Application* app);
	~ToolsPanel() override;

	void OnImGuiRender(bool* p_open) override;
	void SetZoneContext(const std::shared_ptr<ZoneContext>& context) override;

private:
	Application* m_app;
	std::shared_ptr<ZoneContext> m_zoneContext;
};
