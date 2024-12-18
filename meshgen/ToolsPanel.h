
#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class ZoneContext;

class ToolsPanel : public PanelWindow
{
public:

	ToolsPanel(Editor* app);
	~ToolsPanel() override;

	void OnImGuiRender(bool* p_open) override;
	void SetZoneContext(const std::shared_ptr<ZoneContext>& context) override;

private:
	Editor* m_editor;
	std::shared_ptr<ZoneContext> m_zoneContext;
};
