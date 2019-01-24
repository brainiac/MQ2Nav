//
// OffMeshConnectionTool.cpp
//

#include "OffMeshConnectionTool.h"

#include "common/NavMeshData.h"
#include "meshgen/InputGeom.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ImGuiWidgets.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourDebugDraw.h>

#include <fmt/format.h>
#include <imgui/imgui.h>
#include <imgui/custom/imgui_user.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

OffMeshConnectionTool::OffMeshConnectionTool()
{
}

OffMeshConnectionTool::~OffMeshConnectionTool()
{
}

void OffMeshConnectionTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;

	m_state = (OffMeshConnectionToolState*)m_meshTool->getToolState(type());
	if (!m_state)
	{
		m_state = new OffMeshConnectionToolState();
		m_meshTool->setToolState(type(), m_state);
	}

	m_state->init(m_meshTool);
}

void OffMeshConnectionTool::reset()
{
	m_editing = false;
	m_state->reset();
}

void OffMeshConnectionTool::handleMenu()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh) return;

	// show list of existing connections
	ImGui::Text("%d Connections", navMesh->GetConnectionCount());
	ImGui::BeginChild("ConnectionList", ImVec2(0, 200), true);

	ImGuiStyle& style = ImGui::GetStyle();
	float w = ImGui::GetContentRegionAvail().x;
	float spacing = style.ItemInnerSpacing.x;
	float button_sz = ImGui::GetFrameHeight();

	for (size_t i = 0; i < navMesh->GetConnectionCount(); ++i)
	{
		OffMeshConnection* conn = navMesh->GetConnection(i);
		const PolyAreaType& area = navMesh->GetPolyArea(conn->areaType);
		ImGui::PushID((int)i);

		std::string connName;
		if (conn->name.empty())
		{
			if (conn->bidirectional)
				connName = "Connection";
			else
				connName = "One-Way Connection";
		}
		else
			connName = conn->name;

		if (!conn->valid)
			connName += " *INVALID*";

		ImColor textColor(255, 255, 255);

		std::string areaName;
		if (!area.valid)
			areaName = fmt::format("Invalid Area {}", conn->areaType);
		else if (area.name.empty())
			areaName = fmt::format("Area {}", conn->areaType);
		else
			areaName = fmt::format("{}", area.name);

		if (!area.valid || !conn->valid)
		{
			textColor = ImColor(255, 0, 0);
		}

		std::string label = fmt::format("{:04}: {} ({})", conn->id, connName, areaName);
		bool selected = (m_state->m_currentConnectionId == conn->id);

		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)textColor);

		if (ImGui::Selectable(label.c_str(), &selected))
		{
			if (selected)
			{
				m_state->reset();
				m_state->m_editConnection = *conn;
				m_state->m_currentConnectionId = conn->id;
				m_editing = false;
			}
		}
		ImGui::PopStyleColor(1);

		ImGui::PopID();
	}
	ImGui::EndChild();

	{
		ImGui::BeginChild("##buttons", ImVec2(0, 30), false);
		ImGui::Columns(3, 0, false);

		ImGui::NextColumn();
		ImGui::NextColumn();

		if (!m_editing
			&& m_state->m_currentConnectionId != 0
			&& ImGuiEx::ColoredButton("Delete", ImVec2(-1, 0), 0.0))
		{
			auto modifiedTiles = navMesh->GetTilesIntersectingConnection(m_state->m_currentConnectionId);
			navMesh->DeleteConnectionById(m_state->m_currentConnectionId);

			if (!modifiedTiles.empty())
			{
				m_meshTool->RebuildTiles(modifiedTiles);
			}

			reset();
		}

		ImGui::Columns(1);
		ImGui::EndChild();
	}

	ImGui::NewLine();

	// Edit / Properties frame
	{
		if (m_state->m_currentConnectionId != 0)
		{
			ImGui::TextColored(ImColor(255, 255, 0), "Connection %d:", m_state->m_currentConnectionId);
		}
		else
		{
			ImGui::TextColored(ImColor(255, 255, 0), "New Connection Properties:");
		}
		ImGui::Separator();

		if (ImGui::Checkbox("Bi-directional", &m_state->m_editConnection.bidirectional))
		{
			m_state->m_modified = true;
		}

		m_state->m_modified |= AreaTypeCombo(navMesh.get(), &m_state->m_editConnection.areaType);

		if (ImGui::InputText("Name (optional)", &m_state->m_editConnection.name))
		{
			m_state->m_modified = true;
		}

		if (m_state->m_currentConnectionId == 0)
		{
			ImGui::TextColored(ImColor(0, 255, 0), "Place two points to complete a connection");
		}
		else if (!m_state->m_editConnection.valid)
		{
			ImGui::TextColored(ImColor(255, 0, 0), "Connection is not valid. A connection can only extend between two adjacent tiles. Delete it and try again.");
		}

		if (m_state->m_currentConnectionId != 0 && m_state->m_modified && ImGui::Button("Save Changes"))
		{
			if (OffMeshConnection* conn = navMesh->GetConnectionById(m_state->m_currentConnectionId))
			{
				conn->name = m_state->m_editConnection.name;

				bool update = false;
				if (conn->areaType != m_state->m_editConnection.areaType)
				{
					conn->areaType = m_state->m_editConnection.areaType;
					update = true;
				}
				if (conn->bidirectional != m_state->m_editConnection.bidirectional)
				{
					conn->bidirectional = m_state->m_editConnection.bidirectional;
					update = true;
				}

				if (update)
				{
					auto modifiedTiles = m_state->UpdateConnection(conn);
					if (!modifiedTiles.empty())
					{
						m_meshTool->RebuildTiles(modifiedTiles);
					}
				}
			}

			m_state->m_modified = false;
		}
	}
}

void OffMeshConnectionTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (!m_meshTool) return; // ??

	if (!m_editing)
		reset();
	m_editing = true;

	auto navMesh = m_meshTool->GetNavMesh();
	std::vector<dtTileRef> modifiedTiles = m_state->handleConnectionClick(p, shift);

	if (!modifiedTiles.empty())
	{
		m_editing = false;
		m_meshTool->RebuildTiles(modifiedTiles);
	}
}

void OffMeshConnectionTool::handleUpdate(float /*dt*/)
{
}

void OffMeshConnectionTool::handleRender()
{
}

void OffMeshConnectionTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	// Draw start and end point labels
	if (m_state->m_hitPosSet)
	{
		glm::vec3 pos = glm::project(m_state->m_hitPos, model, proj, view);

		ImGui::RenderText((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220), "Start");
	}

	// Tool help
	if (!m_state->m_hitPosSet)
	{
		ImGui::TextColored(ImVec4(255, 255, 255, 192),
			"LMB: Create new connection.\nSHIFT+LMB: Click to delete existing connection.");
	}
	else
	{
		ImGui::TextColored(ImVec4(255, 255, 255, 192),
			"LMB: Set connection end point and finish.");
	}
}

//============================================================================

void OffMeshConnectionToolState::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
}

void OffMeshConnectionToolState::reset()
{
	m_hitPosSet = false;
	m_currentConnectionId = 0;
	m_editConnection = OffMeshConnection{};
	m_modified = false;
}

void OffMeshConnectionToolState::handleRender()
{
	auto navMesh = m_meshTool->GetNavMesh();
	const auto& connections = navMesh->GetConnections();

	DebugDrawGL dd;
	const float s = navMesh->GetNavMeshConfig().agentRadius;

	if (m_hitPosSet)
		duDebugDrawCross(&dd, m_hitPos[0], m_hitPos[1] + 0.1f, m_hitPos[2], s, duRGBA(0, 0, 0, 128), 2.0f);

	unsigned int conColor = duRGBA(0, 192, 128, 192);
	unsigned int badColor = duRGBA(192, 0, 128, 192);
	unsigned int activeColor = duRGBA(255, 255, 0, 192);
	unsigned int baseColor = duRGBA(0, 0, 0, 64);
	dd.depthMask(false);

	dd.begin(DU_DRAW_LINES, 2.0f);

	for (const auto& connection : connections)
	{
		const glm::vec3& from = connection->start;
		const glm::vec3& to = connection->end;

		dd.vertex(glm::value_ptr(from), baseColor);
		dd.vertex(glm::value_ptr(from + glm::vec3(0.0f, 0.2f, 0.0f)), connection->id == m_currentConnectionId ? activeColor : baseColor);

		dd.vertex(glm::value_ptr(to), baseColor);
		dd.vertex(glm::value_ptr(to + glm::vec3(0.0f, 0.2f, 0.0f)), connection->id == m_currentConnectionId ? activeColor : baseColor);

		duAppendCircle(&dd, from.x, from.y + 0.1f, from.z, s, connection->id == m_currentConnectionId ? activeColor : baseColor);
		duAppendCircle(&dd, to.x, to.y + 0.1f, to.z, s, connection->id == m_currentConnectionId ? activeColor : baseColor);

		duAppendArc(&dd, from.x, from.y, from.z, to.x, to.y, to.z, 0.25f,
			connection->bidirectional ? 4.f : 0.0f, 4.f,
			connection->id == m_currentConnectionId ? activeColor : connection->valid ? conColor : badColor);
	}
	dd.end();
	dd.depthMask(true);
}

void OffMeshConnectionToolState::handleRenderOverlay(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view)
{

}

std::vector<dtTileRef> OffMeshConnectionToolState::handleConnectionClick(const glm::vec3& p, bool shift)
{
	std::vector<dtTileRef> modifiedTiles;
	auto navMesh = m_meshTool->GetNavMesh();

	if (shift)
	{
		// Delete
		uint32_t nearestId = 0;
		const auto& connections = navMesh->GetConnections();

		// Find nearest link end-point
		float nearestDistance = FLT_MAX;

		for (const auto& connection : connections)
		{
			float d = glm::distance2(p, connection->start);
			if (d < nearestDistance)
			{
				nearestDistance = d;
				nearestId = connection->id;

				continue; // skip checking end, since it would have same id if closer.
			}

			d = glm::distance2(p, connection->end);
			if (d < nearestDistance)
			{
				nearestDistance = d;
				nearestId = connection->id;
			}
		}

		if (nearestId != 0
			&& glm::sqrt(nearestDistance) < navMesh->GetNavMeshConfig().agentRadius)
		{
			modifiedTiles = navMesh->GetTilesIntersectingConnection(nearestId);

			navMesh->DeleteConnectionById(nearestId);
		}
	}
	else
	{
		// Create
		if (!m_hitPosSet)
		{
			m_hitPos = p;
			m_hitPosSet = true;
		}
		else
		{
			m_hitPosSet = false;

			auto connection = std::make_unique<OffMeshConnection>(m_editConnection);
			connection->start = m_hitPos;
			connection->end = p;
			connection->valid = false;

			auto conn = navMesh->AddConnection(std::move(connection));
			modifiedTiles = UpdateConnection(conn);

			reset();
			m_currentConnectionId = conn->id;
		}
	}

	return modifiedTiles;
}

std::vector<dtTileRef> OffMeshConnectionToolState::UpdateConnection(OffMeshConnection* conn)
{
	auto navMesh = m_meshTool->GetNavMesh();

	auto modifiedTiles = navMesh->GetTileRefsForPoint(conn->start);
	auto endTiles = navMesh->GetTileRefsForPoint(conn->end);
	std::copy(modifiedTiles.begin(), modifiedTiles.end(), std::back_inserter(endTiles));

	if (!modifiedTiles.empty())
	{
		int bminx = INT_MAX, bmaxx = 0;
		int bminy = INT_MAX, bmaxy = 0;

		for (dtTileRef ref : endTiles)
		{
			const dtMeshTile* tile = navMesh->GetNavMesh()->getTileByRef(ref);
			if (!tile)
			{
				break;
			}

			bminx = std::min(bminx, tile->header->x);
			bminy = std::min(bminy, tile->header->y);

			bmaxx = std::max(bmaxx, tile->header->x);
			bmaxy = std::max(bmaxy, tile->header->y);
		}

		if (std::abs(bmaxx - bminx) <= 1 && std::abs(bmaxy - bminy) <= 1)
			conn->valid = true;
	}

	if (conn->valid)
		return modifiedTiles;

	return {};
}
