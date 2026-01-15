
#include "pch.h"
#include "SceneHierarchyPanel.h"

#include "imgui/scoped_helpers.h"
#include "meshgen/Editor.h"
#include "meshgen/Scene.h"

#include "entt/entity/handle.hpp"

SceneHierarchyPanel::SceneHierarchyPanel(Editor* app)
	: PanelWindow("Scene Hierarchy", "SceneHierarchyPanel")
	, m_editor(app)
{
	if (m_project = m_editor->GetProject())
		m_scene = m_project->GetScene();
}

SceneHierarchyPanel::~SceneHierarchyPanel()
{
}

void SceneHierarchyPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	if ((m_project = zoneProject))
		m_scene = m_project->GetScene();
	else
		m_scene = nullptr;
}

void SceneHierarchyPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (m_project && !m_project->IsBusy())
		{
			// Entity list

			mq::imgui::ScopedStyleStack cellPadding(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 0.0f));

			// Table
			{
				ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX
					| ImGuiTableFlags_Resizable
					| ImGuiTableFlags_Reorderable
					| ImGuiTableFlags_ScrollY;

				constexpr int numColumns = 3;

				if (ImGui::BeginTable("##SceneHierarchyTable", numColumns, tableFlags, ImGui::GetContentRegionAvail()))
				{
					ImGui::TableSetupColumn("Label");
					ImGui::TableSetupColumn("Type");
					ImGui::TableSetupColumn("Visibility");

					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					auto view = m_scene->GetAllEntitiesWith<IdentityComponent>();
					for (auto entity : view)
					{
						entt::handle handle{ m_scene->GetRegistry(), entity };

						if (!handle.any_of<HierarchicalComponent>())
							continue;

						if (!GetParent(handle).valid())
						{
							DrawEntityRow(handle);
						}
					}


					ImGui::EndTable();
				}
			}
		}
		
	}

	ImGui::End();
}

void SceneHierarchyPanel::DrawEntityRow(const entt::handle& entity)
{
	IdentityComponent& tag = entity.get<IdentityComponent>();
	std::string_view name = tag.name;
	if (name.empty())
		name = "Unnamed";

	ImGui::PushID(static_cast<int>(entity.entity()));

	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("%.*s", name.size(), name.data());

	ImGui::PopID();
}
