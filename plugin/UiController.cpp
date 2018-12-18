//
// UiController.cpp
//

#include "UiController.h"

#include "MQ2Nav_Settings.h"
#include "MQ2Navigation.h"
#include "ModelLoader.h"
#include "ImGuiRenderer.h"
#include "Waypoints.h"

#define IMGUI_INCLUDE_IMGUI_USER_H
#include <imgui.h>
#include <imgui_custom/imgui_user.h>

#include <glm/gtc/type_ptr.hpp>

namespace
{
	static const char* s_tabNames[TabPage::Max] = {
		"Navigation",
		"Waypoints",
		"Settings",
		"Debug Tools",
#if defined(_DEBUG)
		"Theme",
#endif
	};
}

//----------------------------------------------------------------------------

void UiController::Initialize()
{
	m_uiConn = g_imguiRenderer->OnUpdateUI.Connect([this]() { PerformUpdateUI(); });
}

void UiController::Shutdown()
{
}

bool UiController::IsUiOn() const
{
	return mq2nav::GetSettings().show_ui;
}

void UiController::OnBeginZone()
{
	// hide imgui while zoning
	if (g_imguiRenderer)
		g_imguiRenderer->SetVisible(false);
}

void UiController::OnEndZone()
{
	// restore imgui after zoning
	if (g_imguiRenderer)
		g_imguiRenderer->SetVisible(true);
}

void UiController::PerformUpdateUI()
{
	OnUpdateUI();

	bool show_ui = IsUiOn();
	if (!show_ui)
		return;

	bool do_ui = ImGui::Begin("MQ2Nav Tools", &show_ui, ImVec2(400, 400), -1, 0);

	if (!show_ui) {
		mq2nav::GetSettings().show_ui = false;
		mq2nav::SaveSettings();
	}

	if (!do_ui) {
		ImGui::End();
		return;
	}

	ImGui::TabLabels(static_cast<int>(TabPage::Max), s_tabNames, m_selectedTab);
	TabPage selectedTab = static_cast<TabPage>(m_selectedTab);

	ImGui::Separator();

	ImGui::BeginChild("tab frame");

	PerformUpdateTab(selectedTab);
	OnTabUpdate(selectedTab);

	ImGui::EndChild();
	ImGui::End();
}

void UiController::PerformUpdateTab(TabPage page)
{
	if (page == TabPage::Settings)
	{
		// "Settings" section (maybe make a separate window?)
		bool changed = false;
		auto& settings = mq2nav::GetSettings();

		enum BreakBehavior {
			DoNothing = 0,
			Stop = 1,
			Pause = 2
		};

		int current = DoNothing;
		if (settings.autobreak)
			current = Stop;
		else  if (settings.autopause)
			current = Pause;

		if (ImGui::Combo("Break Behavior", &current, "Disabled\0Stop Navigation\0Pause Navigation"))
		{
			settings.autobreak = current == Stop;
			settings.autopause = current == Pause;
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Auto Break Behavior\n"
				"-------------------------------------------------\n"
				"What happens when a movement key is pressed.\n"
				"  Disable - Auto break is turned off\n"
				"  Stop - Stop the navigation\n"
				"  Pause - Pause navigation. /nav pause to unpause");

		if (ImGui::Checkbox("Attempt to get unstuck", &settings.attempt_unstuck))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Automatically try to get unstuck of movement is impeeded.\nThis will do things like jump and click randomly. Use with caution!");
		}

		if (ImGui::Checkbox("Auto update nav mesh", &settings.autoreload))
		{
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Automatically reload the navmesh when it is modified");

		if (ImGui::Checkbox("Use FloorHeight for path planning", &settings.use_spawn_floor_height))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Use FloorHeight instead of Z for vertical spawn position when\ncalculating positions for pathing");
		}

		if (ImGui::Checkbox("Custom Polygon search extents", &settings.use_find_polygon_extents))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Customize the distance to search when locating positions outside the mesh.\nWARNING: setting these values can produce erratic results");
		}
		if (settings.use_find_polygon_extents)
		{
			if (ImGui::InputFloat3("Polygon search extents", glm::value_ptr(settings.find_polygon_extents), 1))
				changed = true;
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("When finding a location on the navmesh, the search distance along\neach axis to find a polygon ([X, Y, Z] - 3d coordinates. Y is up/down)");
			}
		}

		// map line
		//
		if (ImGui::Checkbox("Enable navigation path map line", &settings.map_line_enabled))
		{
			g_mq2Nav->GetMapLine()->SetEnabled(settings.map_line_enabled);
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("When a navigation path is active, highlight the path on the map. Requires MQ2Map to be loaded.");

		ARGBCOLOR mapLineColor;
		mapLineColor.ARGB = g_mq2Nav->GetMapLine()->GetColor();
		float mapLineRGB[3] = { mapLineColor.R / 255.f, mapLineColor.G / 255.f, mapLineColor.B / 255.f };
		if (ImGuiEx::ColorEdit3("Map line color", mapLineRGB, ImGuiColorEditFlags_RGB))
		{
			mapLineColor.R = mapLineRGB[0] * 255;
			mapLineColor.G = mapLineRGB[1] * 255;
			mapLineColor.B = mapLineRGB[2] * 255;
			g_mq2Nav->GetMapLine()->SetColor(mapLineColor.ARGB);
			settings.map_line_color = mapLineColor.ARGB;
			changed = true;
		}

		if (ImGui::SliderInt("Map line layer", &settings.map_line_layer, 0, 3))
		{
			g_mq2Nav->GetMapLine()->SetLayer(settings.map_line_layer);
			changed = true;
		}

		// save the settings
		//
		if (changed)
			mq2nav::SaveSettings();
	}

	else if (page == TabPage::Tools)
	{
		auto modelLoader = g_mq2Nav->Get<ModelLoader>();
		modelLoader->OnUpdateUI();
	}

	else if (page == TabPage::Waypoints)
	{
		mq2nav::RenderWaypointsUI();
	}

#if defined(_DEBUG)
	else if (page == TabPage::Theme)
	{
		ImGui::ShowStyleEditor();
		ImGui::ShowTestWindow();
	}
#endif
}
