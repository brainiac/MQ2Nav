
#include "pch.h"
#include "PanelManager.h"

#include "meshgen/ApplicationConfig.h"
#include <mq/base/Config.h>

#include "imgui_internal.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <ranges>

PanelWindow::PanelWindow(std::string_view panelName, std::string_view settingsName)
	: panelName(panelName)
	, settingsName(settingsName)
{
}

void PanelWindow::Initialize()
{
	if (isOpenPersistent)
	{
		isOpen = mq::GetPrivateProfileBool(settingsName, "Open", isOpen, g_config.GetSettingsFileName());
	}
}

void PanelWindow::SetIsOpen(bool open)
{
	isOpen = open;

	if (isOpenPersistent)
	{
		mq::WritePrivateProfileBool(settingsName, "Open", isOpen, g_config.GetSettingsFileName());
	}
}

PanelManager::PanelManager()
{
}

PanelManager::~PanelManager()
{
}

void PanelManager::PrepareDockSpace()
{
	// Re-implementation of DockSpaceOverViewport
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(mainViewport->WorkPos);
	ImGui::SetNextWindowSize(mainViewport->WorkSize);
	ImGui::SetNextWindowViewport(mainViewport->ID);

	ImGuiWindowFlags host_window_flags = 0;
	host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
	host_window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus;
	host_window_flags |= ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	int dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("MainDockSpace", nullptr, host_window_flags);
	ImGui::PopStyleVar(3);

	m_dockspaceID = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(m_dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags, nullptr);

	m_dockspaceIDs["MainDockSpace"] = m_dockspaceID;

	ApplyDockLayout();

	ImGui::End();
}

void PanelManager::ApplyDockLayout()
{
	if (ImGui::GetFrameCount() == 1)
	{
		// Load settings and check for our current layout
		std::string initialLayout = mq::GetPrivateProfileString("Window", "Layout", "", g_config.GetSettingsFileName());

		if (initialLayout.empty())
		{
			m_resetLayout = true;
		}
		else
		{
			// Find layout by name.
			auto iter = std::ranges::find_if(m_dockingLayouts,
				[&initialLayout](const auto& layout) { return layout.layoutName == initialLayout; });
			if (iter == end(m_dockingLayouts))
			{
				m_resetLayout = true;
			}
			else
			{
				m_activeLayout = &*iter;
			}
		}

		// We need to wait a frame in order to have windows to position.
		return;
	}

	if (m_resetLayout)
	{
		m_resetLayout = false;
		DockingLayout* currentLayout = m_activeLayout;

		ImGui::DockContextClearNodes(ImGui::GetCurrentContext(), m_dockspaceID, false);

		if (m_activeLayout == nullptr)
		{
			for (DockingLayout& layout : m_dockingLayouts)
			{
				if (layout.isDefault)
					m_activeLayout = &layout;
			}
		}

		if (!m_activeLayout && !m_dockingLayouts.empty())
			m_activeLayout = &m_dockingLayouts[0];

		if (!m_activeLayout)
			return;

		if (currentLayout != m_activeLayout)
		{
			mq::WritePrivateProfileString("Window", "Layout", m_activeLayout->layoutName, g_config.GetSettingsFileName());
		}

		// Split the dock nodes
		for (DockingSplit& split : m_activeLayout->splits)
		{
			IM_ASSERT(m_dockspaceIDs.contains(split.initialDock));

			ImGuiID dockId = m_dockspaceIDs[split.initialDock];
			ImGuiID newDockId = ImGui::DockBuilderSplitNode(dockId, split.direction, split.ratio, nullptr, &dockId);

			m_dockspaceIDs[split.initialDock] = dockId;
			m_dockspaceIDs[split.newDock] = newDockId;

			ImGuiDockNode* dockNode = ImGui::DockBuilderGetNode(newDockId);
			dockNode->SetLocalFlags(split.nodeFlags);
		}

		// Apply the window locations
		for (auto& assignment : m_activeLayout->assignments)
		{
			ImGui::DockBuilderDockWindow(assignment.panelName.c_str(), m_dockspaceIDs[assignment.dockName]);

			if (!assignment.external)
			{
				// Get the panel matching this name
				auto panel = m_panels[hash::fnv_1a()(assignment.panelName)];
				IM_ASSERT(panel != nullptr);

				panel->SetIsOpen(assignment.open);
			}
		}

		ImGui::DockBuilderFinish(m_dockspaceID);
	}
}

void PanelManager::AddDockingLayout(DockingLayout&& layout)
{
	m_dockingLayouts.push_back(std::move(layout));
}

void PanelManager::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	for (auto& panel : m_panels | std::views::values)
	{
		panel->OnProjectChanged(zoneProject);
	}
}

void PanelManager::OnNavMeshProjectChanged(const std::shared_ptr<NavMeshProject>& navMeshProject)
{
	for (auto& panel : m_panels | std::views::values)
	{
		panel->OnNavMeshProjectChanged(navMeshProject);
	}
}


void PanelManager::UpdatePopups()
{
	std::unique_lock lock(m_mutex);

	// Display stored popups
	for (auto iter = begin(m_popups); iter != end(m_popups);)
	{
		PopupNotification& notif = *iter;

		if (!notif.shown)
		{
			notif.shown = true;
			ImGui::OpenPopup(notif.titleAndId.c_str());
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(400, 60), ImVec2(FLT_MAX, FLT_MAX));

		bool show;
		if (notif.modal)
			show = ImGui::BeginPopupModal(notif.titleAndId.c_str());
		else
			show = ImGui::BeginPopup(notif.titleAndId.c_str());

		if (show)
		{
			ImGui::Text("%s", notif.text.c_str());

			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();

			++iter;
		}
		else
		{
			iter = m_popups.erase(iter);
		}
	}
}

void PanelManager::OnImGuiRender()
{
	for (auto& panel : m_panels | std::views::values)
	{
		bool changed = false;

		if (panel->isOpen)
		{
			panel->OnImGuiRender(panel->isClosable ? &panel->isOpen : nullptr);
			if (panel->isClosable && !panel->isOpen)
				changed = true;
		}

		if (changed)
		{
			panel->SetIsOpen(panel->isOpen);
		}
	}

	UpdatePopups();
}

bool PanelManager::FocusPanel(std::string_view panelName)
{
	size_t hash = hash::fnv_1a()(panelName);

	auto iter = m_panels.find(hash);
	if (iter == m_panels.end())
		return false;

	iter->second->focusNextFrame = true;
	return true;
}

void PanelManager::Shutdown()
{
	// TODO: Persistence?

	m_panels.clear();
	m_dockingLayouts.clear();
	m_dockspaceIDs.clear();
}

void PanelManager::DoWindowMenu()
{
	for (size_t key : m_panelsSorted)
	{
		auto& panel = m_panels[key];

		// TODO: Keybinds
		char label[64];
		sprintf_s(label, "Show %s", panel->panelName.c_str());
		if (ImGui::MenuItem(label, nullptr, panel->isOpen))
		{
			panel->SetIsOpen(!panel->isOpen);
		}
	}
}

void PanelManager::DoLayoutsMenu()
{
	if (m_dockingLayouts.size() > 1)
	{
		if (ImGui::BeginMenu("Layouts"))
		{
			if (ImGui::MenuItem("Reset Layout"))
			{
				m_resetLayout = true;
			}

			ImGui::Separator();

			for (DockingLayout& layout : m_dockingLayouts)
			{
				bool selected = m_activeLayout == &layout;

				if (ImGui::MenuItem(layout.layoutName.c_str(), nullptr, selected))
				{
					m_activeLayout = &layout;
					mq::WritePrivateProfileString("Window", "Layout", m_activeLayout->layoutName, g_config.GetSettingsFileName());
					m_resetLayout = true;
				}
			}

			ImGui::EndMenu();
		}
	}
	else
	{
		if (ImGui::MenuItem("Reset Layout"))
		{
			m_resetLayout = true;
		}
	}
}

void PanelManager::ShowNotificationDialog(std::string_view title, std::string_view contents, bool modal)
{
	std::unique_lock lock(m_mutex);

	m_popups.emplace_back(
		fmt::format("{}##{}", title, m_nextPopupId++),
		std::string(contents),
		modal,
		false
	);
}
