//
// BitmapBrowserPanel.h
//

#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class MGBitmap;
class ZoneResourceManager;

class BitmapBrowserPanel : public PanelWindow
{
public:
	enum class ViewMode
	{
		List,
		Grid
	};

	BitmapBrowserPanel(Editor* editor);
	virtual ~BitmapBrowserPanel() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	void DrawToolbar();
	void DrawBitmapList(ZoneResourceManager* resourceMgr);
	void DrawBitmapGrid(ZoneResourceManager* resourceMgr);
	void DrawBitmapPreview();

private:
	Editor* m_editor;
	std::shared_ptr<ZoneProject> m_project;
	MGBitmap* m_selectedBitmap = nullptr;
	ViewMode m_viewMode = ViewMode::List;
	float m_gridThumbnailSize = 64.0f;
	bool m_showPreview = true;
	float m_previewHeight = 200.0f;
};
