#pragma once

#include "imgui.h"

namespace ImGui
{
	void RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	// alternate form
	void RenderText(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...);
}