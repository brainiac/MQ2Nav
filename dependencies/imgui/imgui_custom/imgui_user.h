
#pragma once

#include <imgui.h>

#include <functional>
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

enum ImGuiColorEditFlags_
{
	ImGuiColorEditFlags_Alpha = 1 << 0,   // ColorEdit/ColorPicker: show/edit Alpha component. Must be 0x01 for compatibility with old API taking bool
	ImGuiColorEditFlags_RGB = 1 << 1,   // ColorEdit: Choose one among RGB/HSV/HEX. User can still use the options menu to change. ColorPicker: Choose any combination or RGB/HSX/HEX.
	ImGuiColorEditFlags_HSV = 1 << 2,
	ImGuiColorEditFlags_HEX = 1 << 3,
	ImGuiColorEditFlags_NoPicker = 1 << 4,   // ColorEdit: Disable picker when clicking on colored square
	ImGuiColorEditFlags_NoOptions = 1 << 5,   // ColorEdit: Disable toggling options menu when right-clicking colored square
	ImGuiColorEditFlags_NoColorSquare = 1 << 6,   // ColorEdit: Disable colored square
	ImGuiColorEditFlags_NoSliders = 1 << 7,   // ColorEdit: Disable sliders, show only a button. ColorPicker: Disable all RGB/HSV/HEX sliders.
	ImGuiColorEditFlags_SmallButton = 1 << 8,
	ImGuiColorEditFlags_ModeMask_ = ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_HSV | ImGuiColorEditFlags_HEX
};

namespace ImGuiEx
{
	void SetupImGuiStyle2();

	bool ColoredButton(const char* text, const ImVec2& size, float hue);

	typedef int ImGuiColorEditFlags;    // color edit mode for ColorEdit*()     // enum ImGuiColorEditFlags_

	IMGUI_API bool          ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);     // click on colored squared to open a color picker, right-click for options. Hint: 'float col[3]' function argument is same as 'float* col'. You can pass address of first element out of a contiguous set, e.g. &myvector.x
	IMGUI_API bool          ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0x01);  // 0x01 = ImGuiColorEditFlags_Alpha = very dodgily backward compatible with 'bool show_alpha=true'
	IMGUI_API bool          ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
	IMGUI_API bool          ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0x01);

	bool ColorCombo(const char* label, int* current_item, bool(*items_getter)(void*, int, ImColor* color, const char**),
		void* data, int items_count, int height_in_items);
}
