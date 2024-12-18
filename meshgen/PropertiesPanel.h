#pragma once

#include "meshgen/PanelManager.h"

class Editor;

class PropertiesPanel : public PanelWindow
{
public:
	PropertiesPanel(Editor* app);
	~PropertiesPanel() override;

	void OnImGuiRender(bool* p_open) override;
	void SetZoneContext(const std::shared_ptr<ZoneContext>& context) override;

private:
	Editor* m_editor;
	std::shared_ptr<ZoneContext> m_zoneContext;
	std::string m_loadedName;
};
