//
// BitmapBrowserPanel.h
//

#pragma once

#include "meshgen/PanelManager.h"

class Editor;
class MGBitmap;

void DrawBitmapPreview(MGBitmap* bitmap, ImVec2 size = ImVec2(0, 0));

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

	virtual void Initialize() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

	void ShowBitmap(MGBitmap* bitmap);

private:
	void DrawToolbar();
	void DrawBitmapList();
	void DrawBitmapGrid();
	void DrawBitmapPreview();

	void SaveSettings();
	bool MatchesFilter(std::string_view name) const;

	Editor* m_editor;
	std::shared_ptr<ZoneProject> m_project;
	MGBitmap* m_selectedBitmap = nullptr;
	ViewMode m_viewMode = ViewMode::List;
	float m_gridThumbnailSize = 64.0f;
	bool m_showPreview = true;
	bool m_scrollToSelection = false;
	float m_previewHeight = 200.0f;
	char m_filterBuffer[256] = {};
};
