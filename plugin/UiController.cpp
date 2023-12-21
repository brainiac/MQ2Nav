//
// UiController.cpp
//

#include "pch.h"
#include "UiController.h"

#include "plugin/ModelLoader.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/NavigationPath.h"
#include "plugin/PluginSettings.h"
#include "plugin/SwitchHandler.h"
#include "plugin/Waypoints.h"
#include "common/Utilities.h"

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

namespace
{
	static const char* s_tabNames[(size_t)TabPage::Max] = {
		"Navigation",
		"Waypoints",
		"Settings",
		"Debug Tools",
	};
}

//----------------------------------------------------------------------------

void UiController::Initialize()
{
}

void UiController::Shutdown()
{
}

bool UiController::IsUiOn() const
{
	return nav::GetSettings().show_ui;
}

void UiController::OnBeginZone()
{
}

void UiController::OnEndZone()
{
}

void UiController::PerformUpdateUI()
{
	OnUpdateUI();

	bool show_ui = IsUiOn();
	if (!show_ui)
		return;

	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
	bool do_ui = ImGui::Begin("MQ2Nav Tools", &show_ui, 0);

	if (!show_ui) {
		nav::GetSettings().show_ui = false;
		nav::SaveSettings();
	}

	if (!do_ui) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("##main", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton | ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (int i = 0; i < static_cast<int>(TabPage::Max); ++i)
		{
			bool isActive = m_selectedTab == i;
			if (ImGui::BeginTabItem(s_tabNames[i]))
			{
				m_selectedTab = i;
				TabPage selectedTab = static_cast<TabPage>(m_selectedTab);

				ImGui::Separator();

				PerformUpdateTab(selectedTab);
				OnTabUpdate(selectedTab);

				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}
	ImGui::End();
}

void UiController::DrawNavSettingsPanel()
{
	DrawSettingsUI(true);
}

void UiController::DrawSettingsUI(bool fromSettingsPanel)
{
	bool changed = false;
	auto& settings = nav::GetSettings();

	auto DrawGeneralSettings = [&]()
	{
		//============================================================================
		// General - Navigation Options
		//============================================================================

		if (fromSettingsPanel)
		{
			if (ImGui::Button("Toggle Nav UI"))
			{
				nav::GetSettings().show_ui = !nav::GetSettings().show_ui;
				nav::SaveSettings(false);
			}
		}

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
		{
			ImGui::SetTooltip(
				"Auto Break Behavior\n"
				"-------------------------------------------------\n"
				"What happens when a movement key is pressed.\n"
				"  Disable - Auto break is turned off\n"
				"  Stop - Stop the navigation\n"
				"  Pause - Pause navigation. /nav pause to unpause");
		}

		ImGui::NewLine();

		if (ImGui::Checkbox("Automatically click nearby doors", &settings.open_doors))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Scans for nearby doors to open automatically while navigating.\nTries to avoid clicking teleporters and elevators.");
		}
		ImGui::Indent();

		if (ImGui::Checkbox("Ignore scripted doors", &settings.ignore_scripted_doors))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Some doors are flagged as performing a scripted action.\nIgnore these \"doors\" and don't try to click them");
		}

		ImGui::Unindent();

		ImGui::NewLine();
		if (ImGui::Checkbox("Attempt to get unstuck", &settings.attempt_unstuck))
			changed = true;
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Automatically try to get unstuck of movement is impeeded.\nThis will do things like jump and click randomly. Use with caution!");
		}


		ImGui::NewLine();
		ImGui::TextColored(ImColor(255, 255, 0), "Advanced Options");
		ImGuiEx::CenteredSeparator();

		ImGui::Checkbox("Periodic path updates enabled", &settings.poll_navigation_path);
	};

	auto DrawMeshSettings = [&]()
	{
		//============================================================================
		// Advanced - Mesh Options
		//============================================================================
		if (ImGui::Checkbox("Auto update nav mesh", &settings.autoreload))
		{
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Automatically reload the navmesh when it is modified");

		//============================================================================
		// Advanced - Mesh Options
		//============================================================================
		ImGui::NewLine();
		ImGui::TextColored(ImColor(255, 255, 0), "Advanced Options");
		ImGuiEx::CenteredSeparator();

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
			if (ImGui::InputFloat3("Polygon search extents", glm::value_ptr(settings.find_polygon_extents), "%.1f"))
				changed = true;
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("When finding a location on the navmesh, the search distance along\neach axis to find a polygon ([X, Y, Z] - 3d coordinates. Y is up/down)");
			}
		}
	};

	auto DrawDisplaySettings = [&]()
	{
		//============================================================================
		// Display - In-Game Path Display
		//============================================================================
		ImGui::TextColored(ImColor(255, 255, 0), "In-Game Path Display");
		ImGuiEx::CenteredSeparator();

		if (ImGui::Checkbox("Show In-Game Navigation Path", &settings.show_nav_path))
		{
			auto path = g_mq2Nav->GetActivePath();
			if (path)
			{
				path->SetShowNavigationPaths(settings.show_nav_path);
			}
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Render the active navigation path on the 3d world.");

		if (ImGui::ColorEdit3("Border Color", (float*)&gNavigationLineStyle.borderColor))
			changed = true;
		if (ImGui::ColorEdit3("Visible Color", (float*)&gNavigationLineStyle.visibleColor))
			changed = true;
		if (ImGui::ColorEdit3("Hidden Color", (float*)&gNavigationLineStyle.hiddenColor))
			changed = true;
		if (ImGui::ColorEdit3("Link Color", (float*)&gNavigationLineStyle.linkColor))
			changed = true;
		if (ImGui::SliderFloat("Opacity", &gNavigationLineStyle.opacity, 0.0f, 1.0f))
			changed = true;
		if (ImGui::SliderFloat("Hidden Opacity", &gNavigationLineStyle.hiddenOpacity, 0.0f, 1.0f))
			changed = true;
		if (ImGui::SliderFloat("Outer Width", &gNavigationLineStyle.lineWidth, 0.1f, 20.0f))
			changed = true;

		float innerWidth = gNavigationLineStyle.lineWidth - (2 * gNavigationLineStyle.borderWidth);
		if (ImGui::SliderFloat("Inner Width", &innerWidth, 0.1f, 20.00f))
		{
			gNavigationLineStyle.borderWidth = (gNavigationLineStyle.lineWidth - innerWidth) / 2;
			changed = true;
		}

		if (ImGui::Button("Reset to Defaults"))
		{
			gNavigationLineStyle = NavigationLine::LineStyle{};
			changed = true;
		}

		//============================================================================
		// Display - MQ2Map Line
		//============================================================================
		ImGui::NewLine();
		ImGui::TextColored(ImColor(255, 255, 0), "MQ2Map Line");
		ImGuiEx::CenteredSeparator();

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
		if (ImGui::ColorEdit3("Map line color", mapLineRGB, ImGuiColorEditFlags_DisplayRGB))
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

		//============================================================================
		// Display - Misc
		//============================================================================
		ImGui::NewLine();
		ImGui::TextColored(ImColor(255, 255, 0), "Misc Draw Settings");
		ImGuiEx::CenteredSeparator();

		if (ImGui::Checkbox("Draw bounding box around door target", &settings.render_doortarget))
			changed = true;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("When a door/switch is targeted, draw a bounding box around the object on the screen.");
	};

	if (fromSettingsPanel)
	{
		if (ImGui::BeginTabBar("##NavSettingsTabBar"))
		{
			if (ImGui::BeginTabItem("General"))
			{
				DrawGeneralSettings();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Mesh"))
			{
				DrawMeshSettings();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Display"))
			{
				DrawDisplaySettings();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	else
	{
		ImGui::Columns(2, "##SettingsTab");
		ImGui::SetColumnWidth(0, 100);

		if (ImGui::Selectable("General", m_settingsSection == Navigation))
			m_settingsSection = Navigation;
		if (ImGui::Selectable("Mesh", m_settingsSection == Mesh))
			m_settingsSection = Mesh;
		if (ImGui::Selectable("Display", m_settingsSection == Display))
			m_settingsSection = Display;

		ImGui::NextColumn();

		if (m_settingsSection == Navigation)
		{
			DrawGeneralSettings();
		}

		if (m_settingsSection == Display)
		{
			DrawDisplaySettings();
		}

		if (m_settingsSection == Mesh)
		{
			DrawMeshSettings();
		}
	}

	// save the settings
	//
	if (changed)
		nav::SaveSettings();

	ImGui::Columns(1);
}

void UiController::PerformUpdateTab(TabPage page)
{
	auto modelLoader = g_mq2Nav->Get<ModelLoader>();
	modelLoader->OnUpdateUI(page == TabPage::Tools);

	if (page == TabPage::Settings)
	{
		DrawSettingsUI(false);
	}
	else if (page == TabPage::Waypoints)
	{
		nav::RenderWaypointsUI();
	}
	else if (page == TabPage::Tools)
	{
		if (ImGui::CollapsingHeader("Door Handler Debug"))
		{
			g_mq2Nav->Get<SwitchHandler>()->DebugUI();
		}

		if (ImGui::CollapsingHeader("Pathing Debug"))
		{
			bool settingsChanged = false;
			auto& settings = nav::GetSettings();

			if (ImGui::Checkbox("Render pathing debug draw", &settings.debug_render_pathing))
				settingsChanged = true;

			if (settingsChanged)
				nav::SaveSettings();

			auto activePath = g_mq2Nav->GetActivePath();
			if (activePath)
			{
				// destination is already in eq coordinates
				auto destPos = activePath->GetDestination();
				glm::vec3 myPos = GetMyPosition();

				glm::vec3 locWp = toEQ(g_mq2Nav->m_currentWaypoint);
				ImGui::LabelText("My Position", "(%.2f, %.2f, %.2f)", myPos.x, myPos.y, myPos.z);
				ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f)", locWp.x, locWp.y, locWp.z);

				ImGui::LabelText("Distance to Waypoint", "%.2f", glm::distance(locWp, myPos));

				ImGui::LabelText("Destination", "(%.2f, %.2f, %.2f)", destPos.x, destPos.y, destPos.z);
				ImGui::LabelText("Distance", "%.2f", glm::distance(destPos, myPos));
				ImGui::LabelText("Distance (2d)", "%.2f", glm::distance(destPos.xy(), myPos.xy()));

				ImGui::LabelText("Line of Sight", "%s",
					CastRay(GetCharInfo()->pSpawn, destPos.x, destPos.y, destPos.z + 10) ? "true" : "false");
				ImGui::LabelText("Line of Sight (mesh)", "%s",
					activePath->CanSeeDestination() ? "true" : "false");
			}
			else {
				ImGui::LabelText("Destination", "<none>");
				ImGui::LabelText("Distance", "");
			}

			ImGui::Separator();
			ImGui::LabelText("Ending Door", "%s", g_mq2Nav->m_pEndingSwitch ? g_mq2Nav->m_pEndingSwitch->Name : "<none>");
			ImGui::LabelText("Ending Item", "%s", g_mq2Nav->m_endingGround ? g_mq2Nav->m_endingGround.Name() : "<none>");
			ImGui::LabelText("Is Active", "%s", g_mq2Nav->m_isActive ? "true" : "false");
			ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f)",
				g_mq2Nav->m_currentWaypoint.x, g_mq2Nav->m_currentWaypoint.y, g_mq2Nav->m_currentWaypoint.z);
			ImGui::LabelText("Stuck Data", "(%.2f, %.2f) %d", g_mq2Nav->m_stuckX, g_mq2Nav->m_stuckY, g_mq2Nav->m_stuckTimer.time_since_epoch());
			ImGui::LabelText("Last Click", "%d", g_mq2Nav->m_lastClick.time_since_epoch() / 1000000);
			ImGui::LabelText("Pathfind Timer", "%d", g_mq2Nav->m_pathfindTimer.time_since_epoch() / 1000000);
			ImGui::LabelText("Velocity", "%d", static_cast<int>(glm::round(GetMyVelocity())));
		}
	}
}
