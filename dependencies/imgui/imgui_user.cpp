
// stupid hack to make visual studio happy. This is defined in imgui.cpp
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.cpp"
#include "imgui.h"
#include "imgui_internal.h"
#endif

static void RenderTextOverlay(ImVec2 pos, const ImVec4& color, bool centered,
	const char* text, const char* text_end)
{
	ImGuiState& g = *GImGui;

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
		g.OverlayDrawList.AddText(g.Font, g.FontSize, pos, ImGui::ColorConvertFloat4ToU32(color),
			text, text_end);
	}
}

void ImGui::RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ImGuiState& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(pos, color, false, g.TempBuffer, text_end);
}

void ImGui::RenderText(int x, int y, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ImGuiState& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(ImVec2((float)x, (float)y), color, false, g.TempBuffer, text_end);
}

void ImGui::RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ImGuiState& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(pos, color, true, g.TempBuffer, text_end);
}

void ImGui::RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	ImGuiState& g = *GImGui;
	const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);

	RenderTextOverlay(ImVec2((float)x, (float)y), color, true, g.TempBuffer, text_end);
}
