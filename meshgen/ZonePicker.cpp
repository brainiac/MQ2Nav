//
// ZonePicker.h
//

#include "meshgen/ZonePicker.h"
#include "engine/bgfx_utils.h"
#include "imgui/imgui_impl_bgfx.h"
#include "eqlib/Constants.h"

#include <mq/base/String.h>
#include <glm/glm.hpp>
#include <imgui.h>

#define EXPANSION_BUTTONS 1

//----------------------------------------------------------------------------

struct ExpansionLogoFile
{
	const char* filename;
	int expansionsPerRow;
	int expansionsPerColumn;
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

	int index = 0;
	int currentExpansion = 0;
	int fileIndex = 0;

	for (const auto& fileInfo : ExpansionLogoFiles)
	{
		int localIndex = 0;

		ExpansionLogoImageInfo imageInfo;
		imageInfo.fileIndex = fileIndex++;

		for (int col = 0; col < fileInfo.expansionsPerColumn; ++col)
		{
			for (int row = 0; row < fileInfo.expansionsPerRow; ++row, ++localIndex)
			{
				// Check if we should skip
				if (std::find(begin(fileInfo.skipIndices), end(fileInfo.skipIndices), localIndex) != end(fileInfo.skipIndices))
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
	auto iter = std::find_if(
		begin(ExpansionLogoImages), end(ExpansionLogoImages),
		[&](const ExpansionLogoImageInfo& info) { return info.expansionId == expansion; });
	if (iter == end(ExpansionLogoImages))
		return { -1, ImVec2() };

	const auto& info = *iter;

	int yOffset = active ? 2 : 1;
	ImVec2 pos = ImVec2(info.imagePos.x, info.imagePos.y + yOffset * ExpansionLogoSize.y);

	return { info.fileIndex, pos };
}

ZonePicker::ZonePicker(const EQConfig& eqConfig, bool batchMode)
	: m_mapList(eqConfig.GetMapList())
	, m_allMaps(eqConfig.GetAllMaps())
	, m_eqDirectory(eqConfig.GetEverquestPath())
	, m_batchMode(batchMode)
{
#if EXPANSION_BUTTONS
	for (const auto& expansionFile : ExpansionLogoFiles)
	{
		std::string path = m_eqDirectory + "\\" + expansionFile.filename;

		bgfx::TextureHandle texture = loadTexture(path.c_str());
		m_textures.push_back(texture);
	}

	PopulateExpansionLogoImages();
#endif
}

ZonePicker::~ZonePicker()
{
	for (const auto& texture : m_textures)
	{
		if (!isValid(texture))
			bgfx::destroy(texture);
	}
}

#if EXPANSION_BUTTONS
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
#endif

bool ZonePicker::DrawExpansionGroup(const EQConfig::Expansion& expansion)
{
	bool result = false;
	ImGui::Columns(2);

	for (const auto& zonePair : expansion.second)
	{
		const std::string& longName = zonePair.first;
		const std::string& shortName = zonePair.second;

		ImGui::PushID(shortName.c_str());

		bool selected = false;
		if (ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns/* | ImGuiSelectableFlags_MenuItem*/))
			selected = true;
		ImGui::NextColumn();
		ImGui::SetColumnOffset(-1, 300);
		ImGui::Text(shortName.c_str());
		ImGui::NextColumn();

		ImGui::PopID();

		if (selected)
		{
			m_selectedZone = zonePair.second;
			result = true;
			break;
		}
	}
	ImGui::Columns(1);

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

#if EXPANSION_BUTTONS
		ImGui::BeginChild("##ExpansionList", ImVec2(148, ImGui::GetWindowHeight() - 115));

		for (int i = (int)m_mapList.size() - 1; i >= 0; --i)
		{
			ImGui::PushID(i);
			auto data = FindExpansionImage(i, m_selectedExpansion == i);
			if (data.first < m_textures.size())
			{
				auto [fileId, pos] = data;

				if (ExpansionButton(m_textures[fileId], pos))
				{
					m_selectedExpansion = (m_selectedExpansion == i ? -1 : i);
				}
			}
			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::SameLine();
#endif

		ImGui::BeginChild("##ZoneList", ImVec2(ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x,
			ImGui::GetWindowHeight() - 115), false);

		if (text.empty())
		{
			ImGui::PushID("ZonesByExpansion");

			if (m_selectedExpansion == -1)
			{
				// if there is no filter we will display the tree of zones
				for (const auto& expansionInfo : m_mapList)
				{
					const std::string& expansionName = expansionInfo.first;

					if (ImGui::TreeNode(expansionName.c_str()))
					{
						if (DrawExpansionGroup(expansionInfo))
							result = true;

						ImGui::TreePop();
					}
				}
			}
			else
			{
				const EQConfig::Expansion& expansion = m_mapList[m_selectedExpansion];

				if (DrawExpansionGroup(expansion))
					result = true;
			}

			ImGui::PopID();
		}
		else
		{
			int count = 0;
			std::string lastZone;

			ImGui::PushID("ZonesByFilter");

			ImGui::Columns(2);

			for (const auto& mapIter : m_allMaps)
			{
				const std::string& shortName = mapIter.first;
				const std::string& longName = mapIter.second;

				if (mq::ci_find_substr(shortName, text) != -1
					|| mq::ci_find_substr(longName, text) != -1)
				{
					ImGui::PushID(shortName.c_str());

					std::string displayName = longName + " (" + shortName + ")";
					lastZone = shortName;
					bool selected = false;
					if (ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns/* | ImGuiSelectableFlags_MenuItem*/))
						selected = true;
					ImGui::NextColumn();
					ImGui::SetColumnOffset(-1, 300);
					ImGui::Text(shortName.c_str());
					ImGui::NextColumn();

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

			ImGui::Columns(1);


			if (count == 1 && selectSingle)
			{
				m_selectedZone = lastZone;
				result = true;
			}

			ImGui::PopID();
		}

		ImGui::EndChild();

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
