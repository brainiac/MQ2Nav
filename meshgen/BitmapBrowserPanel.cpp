//
// BitmapBrowserPanel.cpp
//

#include "pch.h"
#include "BitmapBrowserPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/MGBitmap.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneResourceManager.h"

#include "imgui/fonts/IconsFontAwesome.h"
#include "imgui/imgui_impl_bgfx.h"

BitmapBrowserPanel::BitmapBrowserPanel(Editor* editor)
	: PanelWindow("Bitmap Browser", "BitmapPane")
	, m_editor(editor)
{
}

BitmapBrowserPanel::~BitmapBrowserPanel()
{
}

void BitmapBrowserPanel::DrawBitmapList(ZoneResourceManager* resourceMgr)
{
	auto* eqgResourceMgr = resourceMgr->GetResourceManager();
	const auto& bitmaps = eqgResourceMgr->GetResourcesByType(eqg::ResourceType::Bitmap);

	if (bitmaps.empty())
	{
		ImGui::Text("No bitmaps loaded");
		return;
	}

	ImGui::Text("%zu bitmaps loaded", bitmaps.size());
	ImGui::Separator();

	// Calculate available space for list and preview
	float availableHeight = ImGui::GetContentRegionAvail().y;
	float listHeight = availableHeight - ((m_showPreview ? m_previewHeight : 0) + ImGui::GetTextLineHeightWithSpacing() + 14.0f);
	listHeight = std::max(listHeight, 50.0f);

	if (ImGui::BeginChild("BitmapList", ImVec2(0, listHeight), ImGuiChildFlags_Borders))
	{
		if (ImGui::BeginTable("##Bitmaps", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (const auto& [name, resource] : bitmaps)
			{
				MGBitmap* bitmap = static_cast<MGBitmap*>(resource.get());

				ImGui::TableNextRow();

				// Name column
				ImGui::TableNextColumn();
				bool isSelected = (m_selectedBitmap == bitmap);
				if (ImGui::Selectable(std::string(name).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					m_selectedBitmap = bitmap;
				}

				// Size column
				ImGui::TableNextColumn();
				ImGui::Text("%ux%u", bitmap->GetWidth(), bitmap->GetHeight());
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	// Draw preview below the list
	DrawBitmapPreview();
}

void BitmapBrowserPanel::DrawBitmapGrid(ZoneResourceManager* resourceMgr)
{
	auto* eqgResourceMgr = resourceMgr->GetResourceManager();
	const auto& bitmaps = eqgResourceMgr->GetResourcesByType(eqg::ResourceType::Bitmap);

	if (bitmaps.empty())
	{
		ImGui::Text("No bitmaps loaded");
		return;
	}

	ImGui::Text("%zu bitmaps loaded", bitmaps.size());
	ImGui::Separator();

	// Calculate available space for grid and preview
	float availableHeight = ImGui::GetContentRegionAvail().y;
	float gridHeight = availableHeight - ((m_showPreview ? m_previewHeight : 0) + ImGui::GetTextLineHeightWithSpacing() + 14.0f);
	gridHeight = std::max(gridHeight, 50.0f);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	if (ImGui::BeginChild("BitmapGrid", ImVec2(0, gridHeight), ImGuiChildFlags_Borders))
	{
		float availableWidth = ImGui::GetContentRegionAvail().x;
		float cellPadding = 4.0f;
		float labelHeight = ImGui::GetTextLineHeight() + 4.0f;
		float cellWidth = m_gridThumbnailSize + cellPadding * 2;
		float cellHeight = m_gridThumbnailSize + labelHeight + cellPadding * 2;
		float spacing = ImGui::GetStyle().ItemSpacing.x;

		int columns = std::max(1, static_cast<int>((availableWidth + spacing) / (cellWidth + spacing)));

		int itemIndex = 0;
		for (const auto& [name, resource] : bitmaps)
		{
			MGBitmap* bitmap = static_cast<MGBitmap*>(resource.get());

			int column = itemIndex % columns;
			int row = itemIndex / columns;

			// Calculate fixed position for this cell
			float cellX = column * (cellWidth + spacing);
			float cellY = row * (cellHeight + spacing);

			ImGui::SetCursorPos(ImVec2(cellX, cellY));

			ImGui::PushID(bitmap);

			bool isSelected = (m_selectedBitmap == bitmap);

			// Draw selection background
			ImVec2 cellScreenPos = ImGui::GetCursorScreenPos();
			ImVec2 cellEnd = ImVec2(cellScreenPos.x + cellWidth, cellScreenPos.y + cellHeight);

			if (isSelected)
			{
				drawList->AddRectFilled(cellScreenPos, cellEnd, ImGui::GetColorU32(ImGuiCol_Header), 4.0f);
			}

			// Invisible button for the entire cell to handle selection
			if (ImGui::InvisibleButton("##cell", ImVec2(cellWidth, cellHeight)))
			{
				m_selectedBitmap = bitmap;
			}
			bool hovered = ImGui::IsItemHovered();

			// Draw the texture centered within the cell
			if (bitmap->HasValidTexture())
			{
				// Calculate thumbnail size maintaining aspect ratio
				float bitmapWidth = static_cast<float>(bitmap->GetWidth());
				float bitmapHeight = static_cast<float>(bitmap->GetHeight());
				float aspectRatio = bitmapWidth / bitmapHeight;

				float thumbWidth = m_gridThumbnailSize;
				float thumbHeight = m_gridThumbnailSize;

				if (aspectRatio > 1.0f)
				{
					thumbHeight = thumbWidth / aspectRatio;
				}
				else
				{
					thumbWidth = thumbHeight * aspectRatio;
				}

				// Center the thumbnail within the cell
				float offsetX = (cellWidth - thumbWidth) * 0.5f;
				float offsetY = cellPadding + (m_gridThumbnailSize - thumbHeight) * 0.5f;

				ImVec2 imagePos = ImVec2(cellScreenPos.x + offsetX, cellScreenPos.y + offsetY);

				ImGui::GetWindowDrawList()->AddImage(
					ImGui::toId(bitmap->GetTextureHandle(), BGFX_IMGUI_FLAGS_ALPHA_BLEND, 0),
					imagePos,
					ImVec2(imagePos.x + thumbWidth, imagePos.y + thumbHeight));
			}
			else
			{
				// Draw placeholder rectangle
				float offsetX = cellPadding;
				float offsetY = cellPadding;
				ImVec2 placeholderPos = ImVec2(cellScreenPos.x + offsetX, cellScreenPos.y + offsetY);
				ImGui::GetWindowDrawList()->AddRect(
					placeholderPos,
					ImVec2(placeholderPos.x + m_gridThumbnailSize, placeholderPos.y + m_gridThumbnailSize),
					ImGui::GetColorU32(ImGuiCol_Border));
			}

			// Draw filename label centered below the thumbnail
			std::string nameStr(name);
			ImVec2 textSize = ImGui::CalcTextSize(nameStr.c_str(), nullptr, false, cellWidth - cellPadding * 2);
			float textX = cellScreenPos.x + (cellWidth - std::min(textSize.x, cellWidth - cellPadding * 2)) * 0.5f;
			float textY = cellScreenPos.y + m_gridThumbnailSize + cellPadding * 2;

			// Clip the text to the cell width
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(textX, textY),
				ImGui::GetColorU32(ImGuiCol_Text),
				nameStr.c_str());

			// Tooltip with full name and dimensions
			if (hovered)
			{
				ImGui::BeginTooltip();
				ImGui::Text("%s", nameStr.c_str());
				ImGui::Text("%ux%u", bitmap->GetWidth(), bitmap->GetHeight());
				ImGui::EndTooltip();
			}

			ImGui::PopID();
			itemIndex++;
		}

		// Set content size for scrolling
		int totalRows = (static_cast<int>(bitmaps.size()) + columns - 1) / columns;
		float contentHeight = totalRows * (cellHeight + spacing);
		ImGui::SetCursorPosY(contentHeight);
	}
	ImGui::EndChild();

	// Draw preview below the grid
	DrawBitmapPreview();
}

void BitmapBrowserPanel::DrawBitmapPreview()
{
	float y1 = ImGui::GetCursorPos().y;

	// Draw resize handle
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));

	ImGui::Button("##PreviewResizer", ImVec2(-1, 4.0f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		m_previewHeight = std::clamp(m_previewHeight - delta, 50.0f - ImGui::GetTextLineHeightWithSpacing(), 600.0f);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	ImGui::PopStyleColor(3);

	float y2 = ImGui::GetCursorPos().y;

	// Collapsible preview header
	m_showPreview = ImGui::CollapsingHeader("Preview",
		m_showPreview ? ImGuiTreeNodeFlags_DefaultOpen : 0);

	if (!m_showPreview)
	{
		return;
	}

	if (!m_selectedBitmap)
	{
		ImGui::Text("Select a bitmap to preview");
		return;
	}

	float y3 = ImGui::GetCursorPos().y;

	float lineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing();


	if (ImGui::BeginChild("PreviewContent", ImVec2(0, m_previewHeight), ImGuiChildFlags_None))
	{
		ImGui::Text("File: %s", m_selectedBitmap->GetFileName().c_str());
		ImGui::Text("Dimensions: %ux%u", m_selectedBitmap->GetWidth(), m_selectedBitmap->GetHeight());

		if (m_selectedBitmap->GetRawData())
		{
			ImGui::SameLine();
			ImGui::Text("| Raw Size: %zu bytes", m_selectedBitmap->GetRawDataSize());
		}

		if (m_selectedBitmap->HasValidTexture())
		{
			ImGui::Separator();

			// Calculate preview size maintaining aspect ratio
			float maxPreviewWidth = ImGui::GetContentRegionAvail().x;
			float maxPreviewHeight = ImGui::GetContentRegionAvail().y;

			float bitmapWidth = static_cast<float>(m_selectedBitmap->GetWidth());
			float bitmapHeight = static_cast<float>(m_selectedBitmap->GetHeight());

			if (bitmapWidth > 0 && bitmapHeight > 0 && maxPreviewHeight > 0)
			{
				float aspectRatio = bitmapWidth / bitmapHeight;

				float previewWidth = maxPreviewWidth;
				float previewHeight = previewWidth / aspectRatio;

				if (previewHeight > maxPreviewHeight)
				{
					previewHeight = maxPreviewHeight;
					previewWidth = previewHeight * aspectRatio;
				}

				// Clamp to reasonable size
				previewWidth = std::min(previewWidth, bitmapWidth);
				previewHeight = std::min(previewHeight, bitmapHeight);

				if (previewWidth > 0 && previewHeight > 0)
				{
					ImGui::Image(
						m_selectedBitmap->GetTextureHandle(),
						BGFX_IMGUI_FLAGS_ALPHA_BLEND,
						0, // mip level
						ImVec2(previewWidth, previewHeight));
				}
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Texture not loaded on GPU");
		}
	}
	ImGui::EndChild();
}

void BitmapBrowserPanel::OnImGuiRender(bool* p_open)
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

			auto* resourceMgr = m_project->GetResourceManager();
			if (m_viewMode == ViewMode::List)
			{
				DrawBitmapList(resourceMgr);
			}
			else
			{
				DrawBitmapGrid(resourceMgr);
			}
		}
	}
	ImGui::End();
}

void BitmapBrowserPanel::DrawToolbar()
{
	// View mode toggle buttons
	bool isListMode = (m_viewMode == ViewMode::List);
	bool isGridMode = (m_viewMode == ViewMode::Grid);

	if (isListMode)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button(ICON_FA_LIST "##ListView"))
	{
		m_viewMode = ViewMode::List;
	}
	if (isListMode)
		ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("List View");

	ImGui::SameLine();

	if (isGridMode)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button(ICON_FA_TH "##GridView"))
	{
		m_viewMode = ViewMode::Grid;
	}
	if (isGridMode)
		ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Grid View");

	// Thumbnail size slider (only visible in grid mode)
	if (m_viewMode == ViewMode::Grid)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderFloat("##ThumbnailSize", &m_gridThumbnailSize, 32.0f, 256.0f, "%.0f px");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Thumbnail Size");
	}

	ImGui::Separator();
}

void BitmapBrowserPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_project = zoneProject;
	m_selectedBitmap = nullptr; // Clear selection when project changes
}
