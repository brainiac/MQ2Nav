//
// MaterialBrowserPanel.cpp
//

#include "pch.h"
#include "MaterialBrowserPanel.h"

#include "BitmapBrowserPanel.h"
#include "meshgen/Editor.h"
#include "meshgen/MGBitmap.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneResourceManager.h"

#include "imgui/fonts/IconsFontAwesome.h"
#include "imgui/imgui_impl_bgfx.h"
#include "mq/base/Config.h"

MaterialBrowserPanel::MaterialBrowserPanel(Editor* editor)
	: PanelWindow("Material Browser", "MaterialPanel")
	, m_editor(editor)
{
}

MaterialBrowserPanel::~MaterialBrowserPanel()
{
}

void MaterialBrowserPanel::Initialize()
{
	PanelWindow::Initialize();
}

void MaterialBrowserPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (!m_project)
		{
			ImGui::Text("No zone loaded");
		}
		else
		{
			DrawToolbar();

			DrawMaterialList();

			DrawMaterialPreview();
		}
	}

	ImGui::End();
}

void MaterialBrowserPanel::DrawToolbar()
{

}

void MaterialBrowserPanel::DrawMaterialList()
{
	ZoneResourceManager* resourceMgr = m_project->GetResourceManager();
	eqg::ResourceManager* eqgResourceMgr = resourceMgr->GetResourceManager();

	const auto& materialPalettes = eqgResourceMgr->GetResourcesByType(eqg::ResourceType::MaterialPalette);

	if (materialPalettes.empty())
	{
		ImGui::Text("No material palettes loaded");
		return;
	}

	// Calculate available space for list and preview
	float availableHeight = ImGui::GetContentRegionAvail().y;
	float listHeight = availableHeight - ((m_showPreview ? m_previewHeight : 0) + ImGui::GetTextLineHeightWithSpacing() + 14.0f);
	listHeight = std::max(listHeight, 50.0f);

	if (ImGui::BeginChild("MaterialList", ImVec2(0, listHeight), ImGuiChildFlags_Borders))
	{
		if (ImGui::BeginTable("##MaterialPalettes", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (const auto& [name, resource] : materialPalettes)
			{
				auto palettePtr = std::static_pointer_cast<eqg::MaterialPalette>(resource);

				ImGui::PushID(palettePtr.get());

				// Material Palette Row
				ImGui::TableNextRow();

				// Name column
				ImGui::TableNextColumn();

				bool mpIsSelected = m_selectedMaterialPalette == palettePtr && m_selectedMaterialIndex == -1;
				bool node_open = ImGui::TreeNodeEx("##MPNode", ImGuiTreeNodeFlags_OpenOnArrow);
				ImGui::SameLine(0, 0);
				if (ImGui::Selectable(palettePtr->GetTagStr().c_str(), mpIsSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					m_selectedMaterialPalette = palettePtr;
					m_selectedMaterialIndex = -1;
				}

				// Type Column
				ImGui::TableNextColumn();

				// Count column
				ImGui::TableNextColumn();
				ImGui::Text("%u", palettePtr->GetNumMaterials());

				if (node_open)
				{
					ImGui::Indent();

					for (uint32_t index = 0; index < palettePtr->GetNumMaterials(); ++index)
					{
						// Material Rows
						ImGui::TableNextRow();

						bool matIsSelected = m_selectedMaterialPalette == palettePtr && m_selectedMaterialIndex == (int)index;
						eqg::Material* material = palettePtr->GetMaterial(index);

						// Name column
						ImGui::TableNextColumn();
	
						if (ImGui::Selectable(material->GetTagStr().c_str(), matIsSelected,
							ImGuiSelectableFlags_SpanAllColumns))
						{
							m_selectedMaterialPalette = palettePtr;
							m_selectedMaterialIndex = (int)index;
						}

						// Type Column
						ImGui::TableNextColumn();
						ImGui::Text("%s", eqg::MaterialTypeToString(material->GetType()));

						// Detail column
						ImGui::TableNextColumn();
						ImGui::Text("%zu",
							(material->m_textureSet ? material->m_textureSet->textures.size() : 0ull)
							+ (material->m_textureSetAlt ? material->m_textureSetAlt->textures.size() : 0ull));
					}

					ImGui::Unindent();
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}

	ImGui::EndChild();
}

void MaterialBrowserPanel::DrawTextureSetPreview(const char* label, eqg::STextureSet* textureSet)
{
	if (!textureSet)
		return;

	if (ImGui::CollapsingHeader(label))
	{
		for (size_t i = 0; i < textureSet->textures.size(); ++i)
		{
			auto& texture = textureSet->textures[i];

			for (size_t j = 0; j < 8; ++j)
			{
				MGBitmap* bitmap = static_cast<MGBitmap*>(texture.textures[j].get());

				if (bitmap)
				{
					::DrawBitmapPreview(bitmap, ImVec2(128, 128));

					if (ImGui::BeginItemTooltip())
					{
						ImGui::Text("Frame: %zu", i + 1);
						ImGui::Text("Name: %s", texture.filename.c_str());
						ImGui::Text("Flags: 0x%08x", texture.flags);

						ImGui::Separator();
						ImGui::Text("Click to view in bitmap browser");

						ImGui::EndTooltip();
					}

					if (ImGui::IsItemClicked())
					{
						m_editor->ShowBitmap(bitmap);
					}
				}
			}
		}
	}
}

void MaterialBrowserPanel::DrawTextureSetTableRow(const char* label, eqg::STextureSet* textureSet)
{
	if (!textureSet)
		return;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	if (ImGui::TreeNode(label))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Num Frames");
		ImGui::TableNextColumn();
		ImGui::Text("%zu", textureSet->textures.size());

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Current Frame");
		ImGui::TableNextColumn();
		ImGui::Text("%u", textureSet->currentFrame);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Update Interval");
		ImGui::TableNextColumn();
		ImGui::Text("%u", textureSet->updateInterval);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Next Update");
		ImGui::TableNextColumn();
		ImGui::Text("%lld", textureSet->nextUpdate.time_since_epoch().count());

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Skip Frames");
		ImGui::TableNextColumn();
		ImGui::Text("%s", textureSet->skipFrames ? "Yes" : "No");

		ImGui::TreePop();
	}
}

void MaterialBrowserPanel::DrawMaterialProperties(eqg::Material* material)
{
	if (ImGui::CollapsingHeader("Properties"))
	{
		if (ImGui::BeginTable("##MaterialProperties", 2, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Property");
			ImGui::TableSetupColumn("Value");
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			// Tag
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Tag");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(material->m_tag.c_str());

			if (!material->m_effectName.empty())
			{
				// Effect Name
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Effect Name");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(material->m_effectName.c_str());
			}

			// Material Type
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Material Type");
			ImGui::TableNextColumn();
			ImGui::Text("%s (%u)", eqg::MaterialTypeToString(material->m_type), (uint32_t)material->m_type);

			// Render Material
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Render Material");
			ImGui::TableNextColumn();
			ImGui::Text("%s (%u)", RenderMaterialToString(static_cast<eqg::ERenderMaterial>(material->m_renderMaterial)),
				material->m_renderMaterial);

			// Render Method
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Render Method");
			ImGui::TableNextColumn();
			ImGui::Text("0x%08x", material->GetRenderMethod());

			// Alpha
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Alpha");
			ImGui::TableNextColumn();
			ImGui::Text("%d", material->m_alpha);

			if (material->m_uvShift != glm::vec2(0.0f))
			{
				// UV Shift
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("UV Shift");
				ImGui::TableNextColumn();
				ImGui::Text("%g, %g", material->m_uvShift.x, material->m_uvShift.y);
			}

			// Scaled Ambient
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Scaled Ambient");
			ImGui::TableNextColumn();
			ImGui::Text("%g", material->m_scaledAmbient);

			// Constant Ambient
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Constant Ambient");
			ImGui::TableNextColumn();
			ImGui::Text("%g", material->m_constantAmbient);

			// Detail Scale
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Detail Scale");
			ImGui::TableNextColumn();
			ImGui::Text("%g", material->m_detailScale);

			if (material->m_transparent)
			{
				// Transparent
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Transparent");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Yes");
			}

			if (material->m_hasBumpMap)
			{
				// Has Bump Map
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Has Bump Map");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Yes");
			}

			if (material->m_twoSided)
			{
				// Two Sided
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Two Sided");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Yes");
			}

			if (material->m_hasVertexTint)
			{
				// Has Vertex Tint
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Has Vertex Tint");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Yes");
			}

			if (material->m_hasVertexTint2)
			{
				// Has Secondary Vertex Tint
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Secondary Vertex Tint");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Yes");
			}

			if (material->m_itemSlot != eqg::ItemTextureSlot_None)
			{
				// Item Slot
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Item Slot");
				ImGui::TableNextColumn();
				ImGui::Text("%d", (int32_t)material->m_itemSlot);
			}

			// Texture Set
			DrawTextureSetTableRow("Texture Set###Set0", material->m_textureSet.get());
			DrawTextureSetTableRow("Texture Set (alt)###Set1", material->m_textureSetAlt.get());

			ImGui::EndTable();
		}
	}
}

void MaterialBrowserPanel::DrawMaterialEffectParameters(eqg::Material* material)
{
	if (material->m_effectParams.empty())
		return;

	char label[64];
	sprintf_s(label, "Effect Parameters (%zu)###EffectParamsSection", material->m_effectParams.size());

	if (ImGui::CollapsingHeader(label))
	{
		if (ImGui::BeginTable("##EffectParameters", 3, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Property");
			ImGui::TableSetupColumn("Value");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (const eqg::SFXParameter& param : material->m_effectParams)
			{
				ImGui::PushID(&param);

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextUnformatted(param.name.c_str());

				switch (param.type)
				{
				case eqg::FXParameterType_Texture:
					ImGui::TableNextColumn();
					ImGui::Text("Texture %d", param.index);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Texture Index");
					break;
				case eqg::FXParameterType_Float:
					ImGui::TableNextColumn();
					ImGui::Text("%g", param.f_value);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Float");
					break;
				case eqg::FXParameterType_Int:
					ImGui::TableNextColumn();
					ImGui::Text("%d", param.n_value);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Int");
					break;
				case eqg::FXParameterType_Color:
					ImGui::TableNextColumn();
					{
						ImVec4 color = ImGui::ColorConvertU32ToFloat4(param.n_value);
						ImGui::ColorButton("##Color", color, ImGuiColorEditFlags_NoPicker);
					}
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Color");
					break;
				case eqg::FXParameterType_Unused:
					ImGui::TableNextColumn();
					ImGui::TextDisabled("Unused");
					ImGui::TableNextColumn();
					break;

				default: break;
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
}

void MaterialBrowserPanel::DrawMaterialPreview()
{
	// Draw resize handle
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));

	ImGui::Button("##PreviewResizer", ImVec2(-1, 4.0f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		m_previewHeight = std::clamp(m_previewHeight - delta, 50.0f - ImGui::GetTextLineHeightWithSpacing(), 1600.0f);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	ImGui::PopStyleColor(3);

	// Collapsible preview header
	m_showPreview = ImGui::CollapsingHeader("Material Details", m_showPreview ? ImGuiTreeNodeFlags_DefaultOpen : 0);

	if (!m_showPreview)
	{
		return;
	}

	if (!m_selectedMaterialPalette)
	{
		ImGui::Text("Select a material to preview");
		return;
	}

	if (m_selectedMaterialIndex == -1)
	{
		ImGui::Text("Material Palette Selected");
		return;
	}

	eqg::Material* material = m_selectedMaterialPalette->GetMaterial(m_selectedMaterialIndex);

	if (ImGui::BeginChild("PreviewContent", ImVec2(0, m_previewHeight - 4), ImGuiChildFlags_AutoResizeY))
	{
		
		DrawTextureSetPreview("Texture Frames", material->m_textureSet.get());
		DrawTextureSetPreview("Texture Frames (Alt)", material->m_textureSetAlt.get());
		DrawMaterialEffectParameters(material);
		DrawMaterialProperties(material);
	}
	ImGui::EndChild();
}

void MaterialBrowserPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_project = zoneProject;
	m_selectedMaterialPalette.reset();
	m_selectedMaterialIndex = -1;
}
