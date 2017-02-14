// requires:
// defining IMGUI_INCLUDE_IMGUI_USER_H and IMGUI_INCLUDE_IMGUI_USER_INL
// at the project level

#include "imgui_custom/imgui_user.h"

//----------------------------------------------------------------------------

std::shared_ptr<ImGuiContext> GImGuiGlobalSharedPointer;

struct ImGuiContextInitializer
{
	ImGuiContextInitializer() { GImGuiGlobalSharedPointer = std::make_shared<ImGuiContext>(); }
	~ImGuiContextInitializer() {}
};

ImGuiContextInitializer s_imGuiContextIniializer;


#pragma warning (push)
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#pragma warning (disable: 4800)

#include "addons/imgui_user.inl"

#pragma warning (pop)

bool ImGuiEx::ColorCombo(const char* label, int* current_item, bool(*items_getter)(void*, int, ImColor* color, const char**),
	void* data, int items_count, int height_in_items)
{
	using namespace ImGui;

	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const float w = CalcItemWidth();

	const ImVec2 label_size = CalcTextSize(label, NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y*2.0f));
	const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
	ItemSize(total_bb, style.FramePadding.y);
	if (!ItemAdd(total_bb, &id))
		return false;

	const float arrow_size = (g.FontSize + style.FramePadding.x * 2.0f);
	const bool hovered = IsHovered(frame_bb, id);
	bool popup_open = IsPopupOpen(id);
	bool popup_opened_now = false;

	ImRect value_bb(frame_bb.Min, frame_bb.Max - ImVec2(arrow_size, 0.0f));
	RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
	RenderFrame(ImVec2(frame_bb.Max.x - arrow_size, frame_bb.Min.y), frame_bb.Max, GetColorU32(popup_open || hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), true, style.FrameRounding); // FIXME-ROUNDING
	RenderCollapseTriangle(ImVec2(frame_bb.Max.x - arrow_size, frame_bb.Min.y) + style.FramePadding, true);

	auto draw_item = [&style, &g](const ImColor& color, const char* item_text, ImRect rect)
	{
		const float square_size = g.FontSize - 2;
		const ImRect bb(rect.Min + ImVec2(style.FramePadding.x, style.FramePadding.y - 1),
			rect.Min + ImVec2(square_size + style.FramePadding.y * 2, square_size + (style.FramePadding.y * 2)));

		bool outline = (color.Value.x == 0 && color.Value.y == 0 && color.Value.z == 0);
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		bool showBorders = (window->Flags & ImGuiWindowFlags_ShowBorders) != 0;

		if (outline)
		{
			window->Flags |= ImGuiWindowFlags_ShowBorders;
			ImGui::PushStyleColor(ImGuiCol_Border, ImColor(255, 255, 255, 64));
			ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImColor(255, 255, 255, 64));
		}

		ImGui::RenderFrame(bb.Min, bb.Max, color, true, 0.0);

		if (outline)
		{
			if (!showBorders)
				window->Flags &= ~ImGuiWindowFlags_ShowBorders;

			ImGui::PopStyleColor(2);
		}

		rect.Min.x += bb.GetWidth() + style.FramePadding.x * 2;
		rect.Min.y += 2;
		RenderTextClipped(rect.Min, rect.Max, item_text, NULL, NULL, ImVec2(0.0f, 0.0f));
	};

	if (*current_item >= 0 && *current_item < items_count)
	{
		const char* item_text;
		ImColor color;
		if (items_getter(data, *current_item, &color, &item_text))
		{
			draw_item(color, item_text, value_bb);
		}
	}

	if (label_size.x > 0)
		RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

	if (hovered)
	{
		SetHoveredID(id);
		if (g.IO.MouseClicked[0])
		{
			SetActiveID(0);
			if (IsPopupOpen(id))
			{
				ClosePopup(id);
			}
			else
			{
				FocusWindow(window);
				OpenPopup(label);
				popup_open = popup_opened_now = true;
			}
		}
	}

	bool value_changed = false;
	if (IsPopupOpen(id))
	{
		// Size default to hold ~7 items
		if (height_in_items < 0)
			height_in_items = 7;

		float popup_height = (label_size.y + style.ItemSpacing.y) * ImMin(items_count, height_in_items) + (style.FramePadding.y * 3);
		float popup_y1 = frame_bb.Max.y;
		float popup_y2 = ImClamp(popup_y1 + popup_height, popup_y1, g.IO.DisplaySize.y - style.DisplaySafeAreaPadding.y);
		if ((popup_y2 - popup_y1) < ImMin(popup_height, frame_bb.Min.y - style.DisplaySafeAreaPadding.y))
		{
			// Position our combo ABOVE because there's more space to fit! (FIXME: Handle in Begin() or use a shared helper. We have similar code in Begin() for popup placement)
			popup_y1 = ImClamp(frame_bb.Min.y - popup_height, style.DisplaySafeAreaPadding.y, frame_bb.Min.y);
			popup_y2 = frame_bb.Min.y;
		}
		ImRect popup_rect(ImVec2(frame_bb.Min.x, popup_y1), ImVec2(frame_bb.Max.x, popup_y2));
		SetNextWindowPos(popup_rect.Min);
		SetNextWindowSize(popup_rect.GetSize());
		PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);

		const ImGuiWindowFlags flags = ImGuiWindowFlags_ComboBox | ((window->Flags & ImGuiWindowFlags_ShowBorders) ? ImGuiWindowFlags_ShowBorders : 0);
		if (BeginPopupEx(label, flags))
		{
			// Display items
			Spacing();
			for (int i = 0; i < items_count; i++)
			{
				PushID((void*)(intptr_t)i);
				const bool item_selected = (i == *current_item);
				const char* item_text;
				ImColor color;
				if (!items_getter(data, i, &color, &item_text))
					item_text = "*Unknown item*";

				bool selected = Selectable("##comboitem", item_selected);

				const ImRect bb(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

				draw_item(color, item_text, bb);

				if (selected)
				{
					SetActiveID(0);
					value_changed = true;
					*current_item = i;
				}
				if (item_selected && popup_opened_now)
					SetScrollHere();
				PopID();
			}
			EndPopup();
		}
		PopStyleVar();
	}
	return value_changed;
}

