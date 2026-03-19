//
// MaterialBrowserPanel.h
//

#pragma once

#include "meshgen/PanelManager.h"
#include "eqglib/eqg_types_fwd.h"

class Editor;


class MaterialBrowserPanel : public PanelWindow
{
public:
	MaterialBrowserPanel(Editor* editor);
	virtual ~MaterialBrowserPanel() override;

	virtual void Initialize() override;

	virtual void OnImGuiRender(bool* p_open) override;
	virtual void OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject) override;

private:
	void DrawToolbar();
	void DrawMaterialList();
	void DrawMaterialPreview();

	void DrawMaterialProperties(eqg::Material* material);
	void DrawTextureSetPreview(const char* label, eqg::STextureSet* textureSet);
	void DrawTextureSetTableRow(const char* label, eqg::STextureSet* textureSet);
	void DrawMaterialEffectParameters(eqg::Material* material);

	Editor* m_editor;
	std::shared_ptr<ZoneProject> m_project;

	eqg::MaterialPalettePtr m_selectedMaterialPalette;
	int m_selectedMaterialIndex = -1;

	bool m_showPreview = true;
	float m_previewHeight = 600.0f;
};
