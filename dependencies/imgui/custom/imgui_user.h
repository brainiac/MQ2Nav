// requires:
// defining IMGUI_INCLUDE_IMGUI_USER_H and IMGUI_INCLUDE_IMGUI_USER_INL
// at the project level

#pragma once

#include <imgui.h>

#include <functional>
#include <memory>

//----------------------------------------------------------------------------

#define NO_IMGUIFILESYSTEM

#pragma warning (push)
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#pragma warning (disable: 4800)
#pragma warning (disable: 4996)

//#include "addons/imgui_user.h"

#pragma warning (pop)

namespace ImGui
{
	void RenderText(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	// alternate form
	void RenderText(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextCentered(int x, int y, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(const ImVec2& pos, const ImVec4& color, const char* fmt, ...);

	void RenderTextRight(int x, int y, const ImVec4& color, const char* fmt, ...);

	bool CollapsingSubHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags = 0);
	bool CollapsingSubHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

}
