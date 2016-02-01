
#pragma once

#include <imgui.h>

namespace ImGui
{
	void RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	// alternate form
	void RenderText(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(int x, int y, const ImVec4& color, const char* fmt, ...);

	bool TabLabels(int numTabs, const char** tabLabels, int& selectedIndex, const char** tabLabelTooltips, bool autoLayout, int *pOptionalHoveredIndex);
}
