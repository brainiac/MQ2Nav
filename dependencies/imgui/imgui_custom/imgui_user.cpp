
#include "imgui_user.h"

#include <imgui.h>
#include <imgui_internal.h>

enum ImGuiAlign_
{
	ImGuiAlign_Left = 1 << 0,
	ImGuiAlign_Center = 1 << 1,
	ImGuiAlign_Right = 1 << 2,
	ImGuiAlign_Top = 1 << 3,
	ImGuiAlign_VCenter = 1 << 4,
	ImGuiAlign_Default = ImGuiAlign_Left | ImGuiAlign_Top
};

static void RenderTextOverlay(ImVec2 pos, const ImVec4& color, ImGuiAlign_ alignment,
	const char* text, const char* text_end)
{
	auto& g = *GImGui;

	if (!text_end)
		text_end = text + strlen(text); // FIXME-OPT
	const char* text_display_end = text_end;

	// if the position is negative, then make it relative to the opposite end of the screen
	if (pos.y < 0)
		pos.y = ImGui::GetIO().DisplaySize.y + pos.y;
	if (pos.x < 0)
		pos.x = ImGui::GetIO().DisplaySize.x + pos.x;

	const int text_len = (int)(text_display_end - text);

	if (text_len > 0)
	{
		if (alignment == ImGuiAlign_Center)
		{
			float displayWidth = ImGui::GetIO().DisplaySize.x;

			// if centered, figure out the text width. We want to wrap at screen edges, so use
			// the screen width minus the distance from center as the available width.
			float availableWidth = displayWidth - ((displayWidth / 2) - pos.x);

			ImVec2 size = ImGui::CalcTextSize(text, text_end, false, availableWidth);

			// subtract half of the size from the x position to center text at that point.
			pos.x -= size.x / 2;
		}
		else if (alignment == ImGuiAlign_Right)
		{
			// if right aligned, then subtract width of the string from the position.
			ImVec2 size = ImGui::CalcTextSize(text, text_end, false);

			// subtract half of the size from the x position to center text at that point.
			pos.x -= size.x;
		}

		g.OverlayDrawList.AddText(g.Font, g.FontSize, pos, ImGui::ColorConvertFloat4ToU32(color),
			text, text_end);
	}
}

void ImGui::RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(pos, color, ImGuiAlign_Left, g.TempBuffer, text_end);
}

void ImGui::RenderText(int x, int y, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(ImVec2((float)x, (float)y), color, ImGuiAlign_Left, g.TempBuffer, text_end);
}

void ImGui::RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(pos, color, ImGuiAlign_Center, g.TempBuffer, text_end);
}

void ImGui::RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(ImVec2((float)x, (float)y), color, ImGuiAlign_Center, g.TempBuffer, text_end);
}

void ImGui::RenderTextRight(const ImVec2& pos, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(pos, color, ImGuiAlign_Right, g.TempBuffer, text_end);
}

void ImGui::RenderTextRight(int x, int y, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	auto& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(ImVec2((float)x, (float)y), color, ImGuiAlign_Right, g.TempBuffer, text_end);
}

bool ImGui::TabLabels(int numTabs, const char** tabLabels, int& selectedIndex, const char** tabLabelTooltips, bool autoLayout, int *pOptionalHoveredIndex)
{
	ImGuiStyle& style = ImGui::GetStyle();

	const ImVec2 itemSpacing = style.ItemSpacing;
	const ImVec4 color = style.Colors[ImGuiCol_Button];
	const ImVec4 colorActive = style.Colors[ImGuiCol_ButtonActive];
	const ImVec4 colorHover = style.Colors[ImGuiCol_ButtonHovered];
	style.ItemSpacing.x = 1;
	style.ItemSpacing.y = 1;


	if (numTabs > 0 && (selectedIndex < 0 || selectedIndex >= numTabs)) selectedIndex = 0;
	if (pOptionalHoveredIndex) *pOptionalHoveredIndex = -1;

	// Parameters to adjust to make autolayout work as expected:----------
	// The correct values are probably the ones in the comments, but I took some margin so that they work well
	// with a (medium size) vertical scrollbar too [Ok I should detect its presence and use the appropriate values...].
	const float btnOffset = 2.f*style.FramePadding.x;   // [2.f*style.FramePadding.x] It should be: ImGui::Button(text).size.x = ImGui::CalcTextSize(text).x + btnOffset;
	const float sameLineOffset = 2.f*style.ItemSpacing.x;    // [style.ItemSpacing.x]      It should be: sameLineOffset = ImGui::SameLine().size.x;
	const float uniqueLineOffset = 2.f*style.WindowPadding.x;  // [style.WindowPadding.x]    Width to be sutracted by windowWidth to make it work.
															   //--------------------------------------------------------------------

	float windowWidth = 0.f, sumX = 0.f;
	if (autoLayout) windowWidth = ImGui::GetWindowWidth() - uniqueLineOffset;

	bool selection_changed = false;
	for (int i = 0; i < numTabs; i++)
	{
		// push the style
		if (i == selectedIndex)
		{
			style.Colors[ImGuiCol_Button] = colorActive;
			style.Colors[ImGuiCol_ButtonActive] = colorActive;
			style.Colors[ImGuiCol_ButtonHovered] = colorActive;
		}
		else
		{
			style.Colors[ImGuiCol_Button] = color;
			style.Colors[ImGuiCol_ButtonActive] = colorActive;
			style.Colors[ImGuiCol_ButtonHovered] = colorHover;
		}

		ImGui::PushID(i);   // otherwise two tabs with the same name would clash.

		if (!autoLayout) { if (i>0) ImGui::SameLine(); }
		else if (sumX > 0.f) {
			sumX += sameLineOffset;   // Maybe we can skip it if we use SameLine(0,0) below
			sumX += ImGui::CalcTextSize(tabLabels[i]).x + btnOffset;
			if (sumX > windowWidth) sumX = 0.f;
			else ImGui::SameLine();
		}

		// Draw the button
		if (ImGui::Button(tabLabels[i])) { selection_changed = (selectedIndex != i); selectedIndex = i; }
		if (autoLayout && sumX == 0.f) {
			// First element of a line
			sumX = ImGui::GetItemRectSize().x;
		}
		if (pOptionalHoveredIndex) {
			if (ImGui::IsItemHovered()) {
				*pOptionalHoveredIndex = i;
				if (tabLabelTooltips && tabLabelTooltips[i] && strlen(tabLabelTooltips[i]) > 0)  ImGui::SetTooltip("%s", tabLabelTooltips[i]);
			}
		}
		else if (tabLabelTooltips && tabLabelTooltips[i] && ImGui::IsItemHovered() && strlen(tabLabelTooltips[i]) > 0) ImGui::SetTooltip("%s", tabLabelTooltips[i]);
		ImGui::PopID();
	}

	// Restore the style
	style.Colors[ImGuiCol_Button] = color;
	style.Colors[ImGuiCol_ButtonActive] = colorActive;
	style.Colors[ImGuiCol_ButtonHovered] = colorHover;
	style.ItemSpacing = itemSpacing;

	return selection_changed;
}
