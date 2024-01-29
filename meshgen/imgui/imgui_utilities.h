
#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Colors
{
	constexpr auto accent = IM_COL32(236, 158, 36, 255);
	constexpr auto highlight = IM_COL32(39, 185, 242, 255);
	constexpr auto niceBlue = IM_COL32(83, 232, 254, 255);
	constexpr auto compliment = IM_COL32(78, 151, 166, 255);
	constexpr auto background = IM_COL32(36, 36, 36, 255);
	constexpr auto backgroundDark = IM_COL32(26, 26, 26, 255);
	constexpr auto titlebar = IM_COL32(21, 21, 21, 255);
	constexpr auto titlebarOrange = IM_COL32(186, 66, 30, 255);
	constexpr auto titlebarGreen = IM_COL32(18, 88, 30, 255);
	constexpr auto titlebarRed = IM_COL32(185, 30, 30, 255);
	constexpr auto propertyField = IM_COL32(15, 15, 15, 255);
	constexpr auto text = IM_COL32(192, 192, 192, 255);
	constexpr auto textBrighter = IM_COL32(210, 210, 210, 255);
	constexpr auto textDarker = IM_COL32(128, 128, 128, 255);
	constexpr auto textError = IM_COL32(230, 51, 51, 255);
	constexpr auto muted = IM_COL32(77, 77, 77, 255);
	constexpr auto groupHeader = IM_COL32(47, 47, 47, 255);
	constexpr auto selection = IM_COL32(237, 192, 119, 255);
	constexpr auto selectionMuted = IM_COL32(237, 201, 142, 23);
	constexpr auto backgroundPopup = IM_COL32(50, 50, 50, 255);
	constexpr auto validPrefab = IM_COL32(82, 179, 222, 255);
	constexpr auto invalidPrefab = IM_COL32(222, 43, 43, 255);
	constexpr auto missingMesh = IM_COL32(230, 102, 76, 255);
	constexpr auto meshNotSet = IM_COL32(250, 101, 23, 255);
}

namespace mq::imgui {

inline ImColor ColorWithMultipliedValue(const ImColor& color, float multiplier)
{
	const ImVec4& colRaw = color.Value;
	float hue, sat, val;
	ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
	return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
}

inline void ShiftCursor(float x, float y)
{
	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(cursor.x + x, cursor.y + y));
}

inline void ShiftCursorX(float distance)
{
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + distance);
}

inline void DrawUnderline(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f)
{
	if (fullWidth)
	{
		if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
			ImGui::PushColumnsBackground();
		else if (ImGui::GetCurrentTable() != nullptr)
			ImGui::TablePushBackgroundChannel();
	}

	const float width = fullWidth ? ImGui::GetWindowWidth() : ImGui::GetContentRegionAvail().x;
	const ImVec2 cursor = ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddLine(ImVec2(cursor.x + offsetX, cursor.y + offsetY),
		ImVec2(cursor.x + width, cursor.y + offsetY),
		Colors::backgroundDark, 1.0f);

	if (fullWidth)
	{
		if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
			ImGui::PopColumnsBackground();
		else if (ImGui::GetCurrentTable() != nullptr)
			ImGui::TablePopBackgroundChannel();
	}
}

} // namespace mq::imgui
