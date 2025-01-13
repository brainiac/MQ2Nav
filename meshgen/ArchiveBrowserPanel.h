#pragma once

#include "meshgen/PanelManager.h"

#include <set>

class Editor;
class ZoneResourceManager;
class ZoneScene;

class ArchiveBrowserPanel : public PanelWindow
{
public:
	ArchiveBrowserPanel(Editor* app);
	~ArchiveBrowserPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	void DrawArchiveTable(ZoneResourceManager* resourceMgr);

private:
	Editor* m_editor;
	std::shared_ptr<ZoneProject> m_project;
};
