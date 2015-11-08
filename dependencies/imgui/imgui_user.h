#pragma once

#include "imgui.h"

namespace ImGui
{
	void RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	// alternate form
	void RenderText(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(int x, int y, const ImVec4& color, const char* fmt, ...);

	float ProgressBar(const char* optionalPrefixText, float value, const float minValue = 0.f, const float maxValue = 1.f,
		const char* format = "%1.0f%%", const ImVec2& sizeOfBarWithoutTextInPixels = ImVec2(-1, -1),
		const ImVec4& colorLeft = ImVec4(0, 1, 0, 0.8f), const ImVec4& colorRight = ImVec4(0, 0.4f, 0, 0.8f),
		const ImVec4& colorBorder = ImVec4(0.25, 0.25, 1.0, 1));
}