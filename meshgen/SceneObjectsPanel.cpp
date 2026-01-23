
#include "pch.h"
#include "SceneObjectsPanel.h"

#include "EQComponents.h"
#include "imgui/scoped_helpers.h"
#include "meshgen/Editor.h"
#include "meshgen/Scene.h"

#include "entt/entity/handle.hpp"

SceneObjectsPanel::SceneObjectsPanel(Editor* app)
	: PanelWindow("Scene Objects", "SceneObjectsPanel")
	, m_editor(app)
{
	if ((m_project = m_editor->GetProject()))
		m_scene = m_project->GetScene();
}

SceneObjectsPanel::~SceneObjectsPanel()
{
}

void SceneObjectsPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	if ((m_project = zoneProject))
		m_scene = m_project->GetScene();
	else
		m_scene = nullptr;
}

void SceneObjectsPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (m_project && !m_project->IsBusy())
		{

			ImGui::Checkbox("Filter by distance", &m_filterByDistance);

			if (m_filterByDistance)
			{
				ImGui::SliderFloat("Distance Filter", &m_distanceFilter, 0.0f, 100.0f);
			}

			// Entity list

			mq::imgui::ScopedStyleStack cellPadding(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 0.0f));

			// Table
			{
				ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX
					| ImGuiTableFlags_Resizable
					| ImGuiTableFlags_Reorderable
					| ImGuiTableFlags_ScrollY;

				constexpr int numColumns = 2;
				glm::vec3 cameraPos = m_editor->GetCamera().GetPosition();
				float distanceFilterSq = m_distanceFilter * m_distanceFilter;

				if (ImGui::BeginTable("##SceneObjectsTable", numColumns, tableFlags, ImGui::GetContentRegionAvail()))
				{
					ImGui::TableSetupColumn("Name");
					ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);

					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					auto view = m_scene->GetAllEntitiesWith<IdentityComponent>();
					for (auto entity : view)
					{
						entt::handle handle{ m_scene->GetRegistry(), entity };

						if (!handle.any_of<HierarchicalComponent>())
							continue;

						if (m_filterByDistance)
						{
							if (auto xform = handle.try_get<TransformComponent>())
							{
								float dist = glm::distance2(xform->position, cameraPos);

								if (dist > distanceFilterSq)
									continue;
							}
						}

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

void SceneObjectsPanel::DrawEntityRow(const entt::handle& entity)
{
	IdentityComponent& tag = entity.get<IdentityComponent>();
	std::string_view name = tag.name;
	if (name.empty())
		name = "Unnamed";

	ImGui::PushID(static_cast<int>(entity.entity()));

	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("%.*s", static_cast<int>(name.size()), name.data());

	ImGui::TableNextColumn();

	if (auto actor = entity.try_get<ActorComponent>())
	{
		if (actor->actor->GetSimpleModel())
			ImGui::TextUnformatted("Simple Actor");
		else if (actor->actor->GetHierarchicalModel())
			ImGui::TextUnformatted("Hierarchical Actor");
		else
			ImGui::TextUnformatted("Actor (Unknown)");
	}
	else if (entity.try_get<PointLightComponent>())
	{
		ImGui::TextUnformatted("Point Light");
	}
	else if (entity.try_get<AreaComponent>())
	{
		ImGui::TextUnformatted("Area Volume");
	}
	else if (entity.try_get<WldAreaComponent>())
	{
		ImGui::TextUnformatted("Area Volume (old)");
	}

	ImGui::PopID();
}
