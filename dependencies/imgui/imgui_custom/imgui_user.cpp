
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

/*
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
*/


// From https://github.com/procedural/gpulib/blob/master/gpulib_imgui.h
struct ImVec3 { float x, y, z; ImVec3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) { x = _x; y = _y; z = _z; } };

void imgui_easy_theming(ImVec3 color_for_text, ImVec3 color_for_head, ImVec3 color_for_area, ImVec3 color_for_body, ImVec3 color_for_pops)
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.Colors[ImGuiCol_Text] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 0.58f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(color_for_body.x, color_for_body.y, color_for_body.z, 0.95f);
	style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 0.58f);
	style.Colors[ImGuiCol_Border] = ImVec4(color_for_body.x, color_for_body.y, color_for_body.z, 0.00f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(color_for_body.x, color_for_body.y, color_for_body.z, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.78f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 0.75f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 0.47f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.21f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.78f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_ComboBg] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.80f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.50f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.50f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.86f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.76f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.86f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_Column] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.32f);
	style.Colors[ImGuiCol_ColumnHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.78f);
	style.Colors[ImGuiCol_ColumnActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.15f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.78f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_CloseButton] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 0.16f);
	style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 0.39f);
	style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 0.63f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(color_for_text.x, color_for_text.y, color_for_text.z, 0.63f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(color_for_head.x, color_for_head.y, color_for_head.z, 0.43f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(color_for_pops.x, color_for_pops.y, color_for_pops.z, 0.92f);
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(color_for_area.x, color_for_area.y, color_for_area.z, 0.73f);
}

void ImGuiEx::SetupImGuiStyle2()
{
	static ImVec3 color_for_text = ImVec3(236.f / 255.f, 240.f / 255.f, 241.f / 255.f);
	static ImVec3 color_for_head = ImVec3(41.f / 255.f, 128.f / 255.f, 185.f / 255.f);
	static ImVec3 color_for_area = ImVec3(57.f / 255.f, 79.f / 255.f, 105.f / 255.f);
	static ImVec3 color_for_body = ImVec3(44.f / 255.f, 62.f / 255.f, 80.f / 255.f);
	static ImVec3 color_for_pops = ImVec3(33.f / 255.f, 46.f / 255.f, 60.f / 255.f);
	imgui_easy_theming(color_for_text, color_for_head, color_for_area, color_for_body, color_for_pops);
}


bool ImGui::CollapsingSubHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags)
{
	ImGui::PushStyleColor(ImGuiCol_Header, ImColor(96, 96, 96, 128));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImColor(96, 96, 96, 196));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImColor(96, 96, 96, 255));

	bool result = ImGui::CollapsingHeader(label, p_open, flags);

	ImGui::PopStyleColor(3);

	return result;
}

bool ImGui::CollapsingSubHeader(const char* label, ImGuiTreeNodeFlags flags)
{
	ImGui::PushStyleColor(ImGuiCol_Header, ImColor(96, 96, 96, 128));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImColor(96, 96, 96, 196));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImColor(96, 96, 96, 255));

	bool result = ImGui::CollapsingHeader(label, flags);

	ImGui::PopStyleColor(3);

	return result;
}

bool ImGuiEx::ColoredButton(const char* text, const ImVec2& size, float hue)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue, 0.8f, 0.8f));
	bool clicked = ImGui::Button(text, size);
	ImGui::PopStyleColor(3);
	return clicked;
}
