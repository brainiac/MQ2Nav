//
// ZonePicker.h
//

#include "meshgen/ZonePicker.h"
#include "engine/bgfx_utils.h"
#include "imgui/imgui_impl_bgfx.h"

#include <mq/base/String.h>
#include <imgui.h>

#include "ResourceManager.h"

#define EXPANSION_BUTTONS 1

//----------------------------------------------------------------------------

struct ExpansionLogoFile
{
	const char* filename;
	int columnCount;
	int rowCount;
	std::vector<int> skipIndices;
};

std::vector<ExpansionLogoFile> ExpansionLogoFiles = {
	{
		"uifiles\\default\\EQ_expansion_logos.tga",
		4, 5,
		{ 19 },
	},
	{
		"uifiles\\default\\EQ_expansion_logos2.tga",
		5, 5,
		{}
	}
};

struct ExpansionLogoImageInfo
{
	int expansionId;
	int fileIndex;
	ImVec2 imagePos;
};
std::vector<ExpansionLogoImageInfo> ExpansionLogoImages;

const ImVec2 ExpansionLogoSize = { 128, 32 };

void PopulateExpansionLogoImages()
{
	if (!ExpansionLogoImages.empty())
		return;

	int currentExpansion = 0;
	int fileIndex = 0;

	for (const auto& fileInfo : ExpansionLogoFiles)
	{
		int localIndex = 0;

		ExpansionLogoImageInfo imageInfo;
		imageInfo.fileIndex = fileIndex++;

		for (int col = 0; col < fileInfo.columnCount; ++col)
		{
			for (int row = 0; row < fileInfo.rowCount; ++row, ++localIndex)
			{
				// Check if we should skip
				if (std::ranges::find(fileInfo.skipIndices, localIndex) != end(fileInfo.skipIndices))
					continue;

				imageInfo.expansionId = currentExpansion;
				imageInfo.imagePos = ImVec2(ExpansionLogoSize.x * col, ExpansionLogoSize.y * row * 3);
				ExpansionLogoImages.push_back(imageInfo);

				currentExpansion++;
			}
		}
	}
}

std::pair<int, ImVec2> FindExpansionImage(int expansion, bool active)
{
	// get the image index by searching for the expansion number
	auto iter = std::ranges::find_if(ExpansionLogoImages,
		[&](const ExpansionLogoImageInfo& info) { return info.expansionId == expansion; });
	if (iter == end(ExpansionLogoImages))
		return { -1, ImVec2() };

	const auto& info = *iter;

	int yOffset = active ? 2 : 1;
	ImVec2 pos = ImVec2(info.imagePos.x, info.imagePos.y + yOffset * ExpansionLogoSize.y);

	return { info.fileIndex, pos };
}

ZonePicker::ZonePicker(const EQConfig& eqConfig, bool batchMode)
	: m_eqConfig(eqConfig)
	, m_eqDirectory(eqConfig.GetEverquestPath())
	, m_batchMode(batchMode)
{
	auto& mapList = m_eqConfig.GetMapList();
	for (const auto& [exp, maps] : mapList)
	{
		for (const auto& map : maps)
		{
			m_allMaps.emplace_back(map.first, map.second, exp);
		}
	}

	for (const auto& expansionFile : ExpansionLogoFiles)
	{
		std::string path = m_eqDirectory + "\\" + expansionFile.filename;

		bgfx::TextureHandle texture = g_resourceMgr->LoadTexture(path.c_str());
		m_textures.push_back(texture);
	}

	// If we failed to load all the texture then disable the buttons
	if (std::ranges::all_of(m_textures, [](const auto& tex) { return !isValid(tex); }))
	{
		m_showExpansionButtons = false;
	}
	else
	{
		PopulateExpansionLogoImages();
	}
}

ZonePicker::~ZonePicker()
{
	for (const auto& texture : m_textures)
	{
		if (isValid(texture))
			bgfx::destroy(texture);
	}
}

static bool ExpansionButton(bgfx::TextureHandle texture, const ImVec2& pos)
{
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));

	bool result = ImGui::ImageButton("##ExpansionButton", texture,
		ExpansionLogoSize,
		ImVec2(pos.x / 512.0f, pos.y / 512.0f),
		ImVec2((pos.x + ExpansionLogoSize.x) / 512.0f, (pos.y + ExpansionLogoSize.y) / 512.0f));

	ImGui::PopStyleColor(3);
	return result;
}

bool ZonePicker::DrawExpansionGroup(const EQConfig::Expansion& expansion, bool showExpansions)
{
	bool result = false;

	for (const auto& zonePair : expansion.second)
	{
		const std::string& longName = zonePair.first;
		const std::string& shortName = zonePair.second;

		ImGui::PushID(shortName.c_str());

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		bool selected = false;
		if (ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns/* | ImGuiSelectableFlags_MenuItem*/))
			selected = true;

		ImGui::TableNextColumn();
		ImGui::Text("%s", shortName.c_str());

		ImGui::TableNextColumn();
		if (showExpansions)
		{
			ImGui::Text("%s", expansion.first.c_str());
		}

		ImGui::PopID();

		if (selected)
		{
			m_selectedZone = zonePair.second;
			result = true;
			break;
		}
	}

	return result;
}

bool ZonePicker::Show(bool focus)
{
	bool result = false;

	float width = 900;
	ImVec2 avail = ImGui::GetIO().DisplaySize;

	float xPos = std::max((avail.x - width) / 2.f, 0.f);
	width = std::min(width, avail.x);

	float yPos = 30;
	float height = avail.y - (yPos * 2);

	ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	const auto& mapList = m_eqConfig.GetMapList();

	bool show = true;

	if (ImGui::Begin("Open Zone", &show,
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::Text("Select a zone or type to filter by name");

		bool selectSingle = false;
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x);
		if (ImGui::InputText("##ZonePicker", m_filterText, 64, ImGuiInputTextFlags_EnterReturnsTrue))
			selectSingle = true;
		if (focus) ImGui::SetKeyboardFocusHere(-1);

		ImGui::PopItemWidth();

		std::string text(m_filterText);

		if (m_showExpansionButtons)
		{
			ImGui::BeginChild("##ExpansionList", ImVec2(148, ImGui::GetWindowHeight() - 115));

			for (int i = static_cast<int>(mapList.size()) - 1; i >= 0; --i)
			{
				ImGui::PushID(i);
				if (auto data = FindExpansionImage(i, m_selectedExpansion == i); data.first < m_textures.size())
				{
					if (auto [fileId, pos] = data; ExpansionButton(m_textures[fileId], pos))
					{
						m_selectedExpansion = (m_selectedExpansion == i ? -1 : i);
					}
				}
				ImGui::PopID();
			}

			ImGui::EndChild();
			ImGui::SameLine();
		}

		if (ImGui::BeginTable("##ZoneList", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
			ImVec2(0, ImGui::GetWindowHeight() - 115)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Long Name");
			ImGui::TableSetupColumn("Short Name");
			ImGui::TableSetupColumn("Expansion");
			ImGui::TableHeadersRow();

			if (text.empty())
			{
				ImGui::PushID("ZonesByExpansion");

				if (m_selectedExpansion == -1)
				{
					// if there is no filter we will display the tree of zones
					for (const auto& expansionInfo : mapList)
					{
						const std::string& expansionName = expansionInfo.first;

						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						if (ImGui::TreeNode(expansionName.c_str()))
						{
							if (DrawExpansionGroup(expansionInfo, true))
								result = true;

							ImGui::TreePop();
						}
					}
				}
				else
				{
					const EQConfig::Expansion& expansion = mapList[m_selectedExpansion];

					if (DrawExpansionGroup(expansion, true))
						result = true;
				}

				ImGui::PopID();
			}
			else
			{
				int count = 0;
				std::string lastZone;

				ImGui::PushID("ZonesByFilter");

				for (const auto& map : m_allMaps)
				{
					const std::string& shortName = map.shortName;
					const std::string& longName = map.longName;
					const std::string& expansion = map.expansion;

					if (mq::ci_find_substr(shortName, text) != -1 || mq::ci_find_substr(longName, text) != -1)
					{
						ImGui::PushID(shortName.c_str());

						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						lastZone = shortName;

						bool selected = false;
						if (ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns))
							selected = true;

						ImGui::TableNextColumn();
						ImGui::Text("%s", shortName.c_str());

						ImGui::TableNextColumn();
						ImGui::Text("%s", expansion.c_str());

						ImGui::PopID();
						if (selected)
						{
							m_selectedZone = shortName;
							result = true;
							break;
						}
					}
					count++;
				}

				if (count == 1 && selectSingle)
				{
					m_selectedZone = lastZone;
					result = true;
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::PushItemWidth(350);
		if (ImGui::Button("Cancel") || result) {
			m_filterText[0] = 0;
			result = true;
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::Checkbox("Load Navmesh if available", &m_loadNavMesh);

	}
	ImGui::End();

	if (!show) result = true;

	return result;
}
