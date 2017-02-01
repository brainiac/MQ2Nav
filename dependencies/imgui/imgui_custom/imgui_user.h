
#pragma once

#include <imgui.h>

#include <memory>


namespace ImGui
{
	void RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	// alternate form
	void RenderText(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(int x, int y, const ImVec4& color, const char* fmt, ...);

	//bool TabLabels(int numTabs, const char** tabLabels, int& selectedIndex, const char** tabLabelTooltips, bool autoLayout, int *pOptionalHoveredIndex);

	bool CollapsingSubHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags = 0);
	bool CollapsingSubHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

}


namespace ImGuiEx
{
	void SetupImGuiStyle2();

	bool ColoredButton(const char* text, const ImVec2& size, float hue);
}
