
#pragma once

#include "meshgen/PanelManager.h"

class Editor;

class ViewPanel : public PanelWindow
{
public:
	ViewPanel(Editor* app);
	virtual ~ViewPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	Editor* m_editor;
	std::shared_ptr<ZoneProject> m_project;
};
