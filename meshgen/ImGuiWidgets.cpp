//
// ImGuiWidgets.cpp
//

#include "ImGuiWidgets.h"

#include "common/NavMesh.h"

#include <imgui/imgui.h>

bool AreaTypeCombo(NavMesh* navMesh, uint8_t* areaType)
{
	const auto& polyAreas = navMesh->GetPolyAreas();
	int size = (int)polyAreas.size();
	bool changed = false;
	int selectedIndex = -1;

	ImVec2 combo_pos = ImGui::GetCursorScreenPos();

	// find the selected index
	for (int i = 0; i < size; ++i)
	{
		if (polyAreas[i]->id == *areaType)
		{
			selectedIndex = i;
		}
	}

	ImGuiStyle& style = ImGui::GetStyle();

	if (ImGui::BeginCombo("Area Type", ""))
	{
		float h = ImGui::GetTextLineHeight();

		for (int i = 0; i < size; ++i)
		{
			ImGui::PushID(i);

			bool selected = selectedIndex == i;
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}

			float default_size = ImGui::GetFrameHeight();
			ImVec2 size{ default_size - 6, default_size - 6 };

			ImColor color{ polyAreas[i]->color };
			color.Value.w = 1.0f; // no transparency

			changed |= ImGui::Selectable("", selected);
			ImGui::SameLine();

			ImGui::ColorButton("##color", color, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip, size);

			ImGui::SameLine();
			ImGui::Text(polyAreas[i]->name.c_str());

			if (changed)
			{
				*areaType = polyAreas[i]->id;
			}

			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	if (selectedIndex != -1)
	{
		ImVec2 backup_pos = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(ImVec2(combo_pos.x + style.FramePadding.x, combo_pos.y));
		ImColor color{ polyAreas[selectedIndex]->color };
		color.Value.w = 1.0f; // no transparency
		ImGui::ColorButton("##selectedColor", color, 0);
		ImGui::SameLine();
		ImGui::Text(polyAreas[selectedIndex]->name.c_str());
		ImGui::SetCursorScreenPos(backup_pos);
	}

	return changed;
}
