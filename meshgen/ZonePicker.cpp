//
// ZonePicker.h
//

#include "pch.h"
#include "meshgen/ZonePicker.h"
#include "meshgen/ResourceManager.h"
#include "engine/bgfx_utils.h"
#include "imgui/imgui_impl_bgfx.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <mq/base/String.h>
#include <algorithm>
#include <ranges>


#define EXPANSION_BUTTONS 1

static constexpr const char* s_dialogName = "Open Zone##ZonePicker";

constexpr int BottomBarHeight = 115;

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

constexpr ImVec2 ExpansionLogoSize = { 128, 32 };

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

//----------------------------------------------------------------------------

// ZonePicker::ZonePicker() = default

ZonePicker::~ZonePicker()
{
	ClearZones();
}

void ZonePicker::LoadZones()
{
	if (m_loaded)
		return;

	auto& mapList = g_config.GetMapList();
	for (const auto& [exp, maps] : mapList)
	{
		for (const auto& map : maps)
		{
			m_allMaps.emplace_back(map.first, map.second, exp);
		}
	}

	std::string eqDirectory = g_config.GetEverquestPath();

	for (const auto& expansionFile : ExpansionLogoFiles)
	{
		std::string path = eqDirectory + "\\" + expansionFile.filename;

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

	m_loaded = true;
}

void ZonePicker::ClearZones()
{
	for (const auto& texture : m_textures)
	{
		if (isValid(texture))
			bgfx::destroy(texture);
	}

	m_allMaps.clear();
	m_textures.clear();
	m_selectedExpansion = -1;
	m_loaded = false;
	m_filterText[0] = 0;
	m_selectedIndex = -1;
	m_lastSelectedIndex = -1;
	m_lastItemCount = -1;
	m_toggleTreeNodeState = { false, -1 };
}

static bool ExpansionButton(bgfx::TextureHandle texture, const ImVec2& pos)
{
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));

	if (ImGui::ImageButton("##ExpansionButton", texture,
		ExpansionLogoSize,
		ImVec2(pos.x / 512.0f, pos.y / 512.0f),
		ImVec2((pos.x + ExpansionLogoSize.x) / 512.0f, (pos.y + ExpansionLogoSize.y) / 512.0f)))
	{
		ImGui::PopStyleColor(3);
		return true;
	}

	ImGui::PopStyleColor(3);
	return false;
}

bool ZonePicker::DrawZoneList(const char* str_id, std::ranges::input_range auto&& input_range, bool enterPressed, int* baseIndex)
{
	int index = baseIndex ? *baseIndex : 0;
	std::string lastZone;
	bool didSelect = false;

	ImGui::PushID(str_id);

	for (const auto& map : input_range)
	{
		const std::string& shortName = map.shortName;
		const std::string& longName = map.longName;
		const std::string& expansion = map.expansion;

		ImGui::PushID(shortName.c_str());

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		lastZone = shortName;

		bool selected = !m_selectedZone.empty() && m_selectedZone == shortName;
		bool clickedSelect = false;

		if (ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns))
			clickedSelect = true;

		ImGui::TableNextColumn();
		ImGui::Text("%s", shortName.c_str());

		ImGui::TableNextColumn();
		ImGui::Text("%s", expansion.c_str());

		ImGui::PopID();

		// If we changed the selected index, update what is selected
		if (index == m_selectedIndex
			&& m_selectedIndex != m_lastSelectedIndex
			&& m_selectedZone.empty())
		{
			m_selectedZone = shortName;
			m_lastSelectedIndex = index;
			m_selectedIndex = index;

			ImGui::SetScrollHereY();
		}

		if (clickedSelect)
		{
			m_selectedZone = shortName;
			m_selectedIndex = index;
			m_foundSelected = true;
			didSelect = true;
		}

		if (m_selectedZone == shortName)
		{
			m_selectedIndex = index;
			m_foundSelected = true;
		}

		++index;
	}

	if (enterPressed)
	{
		if (index == 1)
		{
			m_selectedZone = lastZone;
			didSelect = true;
		}
		else if (!m_selectedZone.empty())
		{
			didSelect = true;
		}
	}

	ImGui::PopID();
	m_lastItemCount = index;

	if (baseIndex) *baseIndex = index;
	return didSelect;
}

void ZonePicker::CheckSelection()
{
	if (!m_foundSelected && m_selectedIndex != -1)
	{
		m_selectedIndex = -1;
		m_selectedZone.clear();
	}

	m_lastSelectedIndex = m_selectedIndex;
}

void ZonePicker::Show()
{
	m_showNextDraw = true;
	m_selectedZone.clear();
}

void ZonePicker::Close()
{
	m_isShowing = false;

	ClearZones();
}

bool ZonePicker::Draw()
{
	if (!m_isShowing && !m_showNextDraw) return false;
	bool activateSelection = false;

	if (m_showNextDraw)
	{
		ImGui::OpenPopup(s_dialogName);
		m_isShowing = true;
		m_setFocus = true;
		m_showNextDraw = false;
	}

	float width = 900;
	ImVec2 avail = ImGui::GetIO().DisplaySize;

	float xPos = std::max((avail.x - width) / 2.f, 0.f);
	width = std::min(width, avail.x);

	float yPos = 30;
	float height = avail.y - (yPos * 2);

	ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	const auto& mapList = g_config.GetMapList();
	bool open = m_isShowing;
	bool closeWindow = false;

	if (ImGui::BeginPopupModal(s_dialogName, &open,
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
	{
		LoadZones();

		ImGui::Text("Select a zone or type to filter by name (arrow keys to select, enter to accept)");

		bool enterPressed = false;
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x);
		if (ImGui::InputText("##ZonePickerFilter", m_filterText, 64, ImGuiInputTextFlags_EnterReturnsTrue))
			enterPressed = true;

		int newSelectedExpansion = -1;

		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
		{
			if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && !mapList.empty())
			{
				if (m_selectedExpansion == -1)
					m_selectedExpansion = static_cast<int>(mapList.size()) - 1;
				else
					m_selectedExpansion = std::max(0, m_selectedExpansion - 1);

				newSelectedExpansion = m_selectedExpansion;
			}
			else
			{
				if (m_selectedIndex < m_lastItemCount)
				{
					m_selectedIndex = std::min(m_lastItemCount - 1, m_selectedIndex + 1);
					if (m_selectedIndex != m_lastSelectedIndex)
						m_selectedZone.clear();
				}
			}
			m_setFocus = true;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
		{
			if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && !mapList.empty())
			{
				if (m_selectedExpansion == -1)
					m_selectedExpansion = 0;
				else
					m_selectedExpansion = std::min(static_cast<int>(mapList.size()) - 1, m_selectedExpansion + 1);

				newSelectedExpansion = m_selectedExpansion;
			}
			else
			{
				if (m_selectedIndex > 0)
				{
					m_selectedIndex = std::max(0, m_selectedIndex - 1);
					if (m_selectedIndex != m_lastSelectedIndex)
						m_selectedZone.clear();
				}
			}
			m_setFocus = true;
		}

		// Swallow navigation inputs here so we can control selection with arrow keys
		ImGui::NavMoveRequestCancel();

		// Ensure that we keep focus on the input box
		if (m_setFocus || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			ImGui::SetKeyboardFocusHere(-1);
			m_setFocus = false;
		}

		ImGui::PopItemWidth();

		std::string_view text(m_filterText);

		if (m_showExpansionButtons)
		{
			ImGui::BeginChild("##ExpansionList", ImVec2(148, ImGui::GetWindowHeight() - BottomBarHeight),
				0, ImGuiWindowFlags_NoFocusOnAppearing);

			for (int i = static_cast<int>(mapList.size()) - 1; i >= 0; --i)
			{
				ImGui::PushID(i);
				if (auto data = FindExpansionImage(i, m_selectedExpansion == i); data.first < static_cast<int>(m_textures.size()))
				{
					if (auto [fileId, pos] = data; ExpansionButton(m_textures[fileId], pos))
					{
						m_selectedExpansion = (m_selectedExpansion == i ? -1 : i);
					}

					if (newSelectedExpansion == i)
						ImGui::SetScrollHereY();
				}
				ImGui::PopID();
			}

			ImGui::EndChild();
			ImGui::SameLine();
		}

		if (ImGui::BeginTable("##ZoneList", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
			ImVec2(0, ImGui::GetWindowHeight() - BottomBarHeight)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Long Name");
			ImGui::TableSetupColumn("Short Name");
			ImGui::TableSetupColumn("Expansion");
			ImGui::TableHeadersRow();

			m_foundSelected = false;

			if (text.empty())
			{
				ImGui::PushID("ZonesByExpansion");

				if (m_selectedExpansion == -1)
				{
					int index = 0;

					// if there is no filter we will display the tree of zones
					for (const auto& expansionInfo : mapList)
					{
						const std::string& expansionName = expansionInfo.first;

						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						int treeNodeFlags = 0;
						bool setToggle = false;

						if (m_selectedIndex == index)
						{
							treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
							m_foundSelected = true;
							setToggle = enterPressed;

							if (index == m_toggleTreeNodeState.second)
							{
								ImGui::SetNextItemOpen(m_toggleTreeNodeState.first);
								m_toggleTreeNodeState.second = -1;
							}
						}

						bool expand = ImGui::TreeNodeEx(expansionName.c_str(), treeNodeFlags);

						if (setToggle)
						{
							m_toggleTreeNodeState = { !expand, index };
						}

						index++;

						if (expand)
						{
							auto filterByExpansion = [&](const MapInfo& map) {
								return map.expansion == expansionName;
							};

							activateSelection = DrawZoneList("ZoneList", std::views::filter(m_allMaps, filterByExpansion), enterPressed, &index);

							ImGui::TreePop();
						}
					}

					m_lastItemCount = index;
					CheckSelection();
				}
				else
				{
					const ApplicationConfig::Expansion& expansion = mapList[m_selectedExpansion];

					auto filterByExpansion = [&](const MapInfo& map) {
						return map.expansion == expansion.first;
					};

					activateSelection = DrawZoneList("ZoneList", std::views::filter(m_allMaps, filterByExpansion), enterPressed);

					CheckSelection();
				}

				ImGui::PopID();
			}
			else
			{
				auto filterByName = [&](const MapInfo& map) {
					return mq::ci_find_substr(map.shortName, text) != -1 || mq::ci_find_substr(map.longName, text) != -1;
				};

				activateSelection = DrawZoneList("ZoneZoneListsByFilter", std::views::filter(m_allMaps, filterByName), enterPressed);

				CheckSelection();
			}

			ImGui::EndTable();
		}

		ImGui::PushItemWidth(350);
		if (ImGui::Button("Cancel") || activateSelection)
		{
			m_filterText[0] = 0;
			m_isShowing = false;
			closeWindow = true;
		}

		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::Checkbox("Load Navmesh if available", &m_loadNavMesh);

		ImGui::EndPopup();
	}
	else
	{
		closeWindow = true;
	}

	if (closeWindow || !open)
	{
		m_isShowing = false;
		m_setFocus = false;
		// Allow us to reload maps after closing
		ClearZones();
	}

	return activateSelection;
}
