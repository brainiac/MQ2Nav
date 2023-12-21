//
// ZonePicker.h
//

#include "meshgen/ZonePicker.h"

#include <mq/base/String.h>
#include <glm/glm.hpp>

#include <SDL2/SDL_opengl.h>

#pragma warning (push)
#pragma warning (disable : 4312 4244)

#include <imgui.h>
#include <stb/stb_image.h>

#pragma warning (pop)

#define EXPANSION_BUTTONS 0

//----------------------------------------------------------------------------

std::vector<std::string> ExpansionLogoFiles = {
	"uifiles\\default\\EQ_expansion_logos.tga",
	"uifiles\\default\\EQ_expansion_logos2.tga"
};


std::vector<int> ExpansionSlots = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22, 23, 24, 25,
};
const int EmptyExpansion = 26;

// each image can hold 5 sets vertically, and four across.
const int ExpansionsPerRow = 4;
const int ExpansionsPerColumn = 5;

const glm::ivec2 ExpansionLogoSize = { 128, 32 };

std::pair<int, glm::ivec2> FindExpansionImage(int expansion, bool active)
{
	// get the image index
	int imageIndex = (expansion < ExpansionSlots.size()) ? ExpansionSlots[expansion] : EmptyExpansion;

	const int ExpansionsPerFile = ExpansionsPerRow * ExpansionsPerColumn;

	// figure out which image file we're in
	int imageFile = imageIndex / ExpansionsPerFile;
	imageIndex = imageIndex % ExpansionsPerFile;

	int imageRow = imageIndex % ExpansionsPerColumn;
	int imageCol = imageIndex / ExpansionsPerColumn;

	int yOffset = active ? 2 : 1;

	glm::ivec2 pos{ ExpansionLogoSize.x * imageCol, ExpansionLogoSize.y * (imageRow * 3 + yOffset) };
	return{ imageFile, pos };
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
		std::string path = m_eqDirectory + "\\" + expansionFile;

		IMAGEDATA image;
		image.data = stbi_load(path.c_str(), &image.width, &image.height, &image.bits, 4);
		if (image.data)
		{
			glGenTextures(1, &image.textureId);
			glBindTexture(GL_TEXTURE_2D, image.textureId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data);

			m_tgaData.push_back(image);
		}
	}
#endif
}

ZonePicker::~ZonePicker()
{
	for (const auto& image : m_tgaData)
	{
		glDeleteTextures(1, &image.textureId);
		free(image.data);
	}
}

#if EXPANSION_BUTTONS
static bool ExpansionButton(const IMAGEDATA& tgaData, const glm::ivec2& pos)
{
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));

	bool result = ImGui::ImageButton((ImTextureID)tgaData.textureId,
		ImVec2((float)ExpansionLogoSize.x,(float)ExpansionLogoSize.y),
		ImVec2((float)pos.x / (float)tgaData.width, (float)pos.y / (float)tgaData.height),
		ImVec2((float)(pos.x + ExpansionLogoSize.x) / (float)tgaData.width, (float)(pos.y + ExpansionLogoSize.y) / (float)tgaData.height),
		0);

	ImGui::PopStyleColor(3);
	return result;
}
#endif

bool ZonePicker::Show(bool focus, std::string* selected_zone /* = nullptr */)
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
		ImGui::PushItemWidth(130);
		static int selectedIndex = 0;

		ImGui::PushID("ExpansionButtons");

		for (int i = 0; i < m_mapList.size(); i++)
		{
			ImGui::PushID(i);
			auto data = FindExpansionImage(i, selectedIndex == i);
			if (data.first < m_tgaData.size())
			{
				int fileId = data.first;
				glm::ivec2 pos = data.second;

				if (ExpansionButton(m_tgaData[fileId], pos))
				{
					selectedIndex = i;
				}
			}
			ImGui::PopID();
		}

		ImGui::PopID();

		ImGui::PopItemWidth();
#endif

		ImGui::BeginChild("##ZoneList", ImVec2(ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x,
			ImGui::GetWindowHeight() - 115), false);

		if (text.empty())
		{
			ImGui::PushID("ZonesByExpansion");

			// if there is no filter we will display the tree of zones
			for (const auto& mapIter : m_mapList)
			{
				const std::string& expansionName = mapIter.first;

				if (ImGui::TreeNode(expansionName.c_str()))
				{
					ImGui::Columns(2);

					for (const auto& zonePair : mapIter.second)
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

						if (selected) {
							*selected_zone = zonePair.second;
							result = true;
							break;
						}
					}
					ImGui::Columns(1);

					ImGui::TreePop();
				}
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
					if (selected) {
						*selected_zone = shortName;
						result = true;
						break;
					}
				}
				count++;
			}

			ImGui::Columns(1);


			if (count == 1 && selectSingle) {
				*selected_zone = lastZone;
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
