
#include "UiController.h"

#include "MQ2Nav_Settings.h"
#include "MQ2Navigation.h"
#include "ModelLoader.h"
#include "ImGuiRenderer.h"
#include "Waypoints.h"

#include <imgui.h>
#include "imgui_custom/imgui_user.h"

namespace
{
	static const char* s_tabNames[TabPage::Max] = {
		"Navigation",
		"Waypoints",
		"Settings",
		"Tools",
		"Theme",
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

	ImGui::TabLabels(static_cast<int>(TabPage::Max), s_tabNames, m_selectedTab,
		nullptr, true, nullptr);
	TabPage selectedTab = static_cast<TabPage>(m_selectedTab);

	ImGui::Separator();

	PerformUpdateTab(selectedTab);
	OnTabUpdate(selectedTab);

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
			ImGui::SetTooltip("Automatically try to get unstuck of movement is impeeded.\nThis will do things like jump and click randomly. Use with caution!");

		if (ImGui::Checkbox("Auto update nav mesh", &settings.autoreload))
		{
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Automatically reload the navmesh when it is modified");
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

	else if (page == TabPage::Theme)
	{
		ImGui::ShowStyleEditor();
	}
}
