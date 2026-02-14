//
// AreaVolumeRenderSystem.cpp
//

#include "pch.h"
#include "meshgen/AreaVolumeRenderSystem.h"

#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/GeometryUtils.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"

#include "spdlog/spdlog.h"
#include "glm/gtc/type_ptr.hpp"
#include "imgui/imgui.h"

#include <map>

//============================================================================

AreaVolumeRenderSystem::AreaVolumeRenderSystem()
{
}

AreaVolumeRenderSystem::~AreaVolumeRenderSystem()
{
	Shutdown();
}

void AreaVolumeRenderSystem::Init(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	// Load shader programs
	m_volumeProgram = g_resourceMgr->GetProgramHandle("areavolume");
	m_linesProgram = g_resourceMgr->GetProgramHandle("lines");
}

void AreaVolumeRenderSystem::SetRegistry(entt::registry* registry)
{
	// Disconnect from old registry if any
	if (m_registry)
	{
		m_areaVolumeConstructConnection.release();
		m_areaVolumeDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();

		m_entityToBatch.clear();
		m_entityColorCache.clear();
	}

	m_registry = registry;

	// Register component callbacks on new registry
	if (m_registry)
	{
		m_areaVolumeConstructConnection = m_registry->on_construct<AreaVolumeComponent>()
			.connect<&AreaVolumeRenderSystem::OnAreaVolumeConstruct>(this);
		m_areaVolumeDestroyConnection = m_registry->on_destroy<AreaVolumeComponent>()
			.connect<&AreaVolumeRenderSystem::OnAreaVolumeDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&AreaVolumeRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&AreaVolumeRenderSystem::OnHiddenDestroy>(this);
	}

	m_dirty = true;
}

void AreaVolumeRenderSystem::OnAreaVolumeConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void AreaVolumeRenderSystem::OnAreaVolumeDestroy(entt::registry& registry, entt::entity entity)
{
	m_entityToBatch.erase(entity);
	m_entityColorCache.erase(entity);
	m_dirty = true;
}

void AreaVolumeRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	// Only care about entities with AreaVolumeComponent
	if (registry.any_of<AreaVolumeComponent>(entity))
	{
		m_dirty = true;
	}
}

void AreaVolumeRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	// Only care about entities with AreaVolumeComponent
	if (registry.any_of<AreaVolumeComponent>(entity))
	{
		m_dirty = true;
	}
}

void AreaVolumeRenderSystem::Shutdown()
{
	if (m_registry)
	{
		m_areaVolumeConstructConnection.release();
		m_areaVolumeDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	DestroyBuffers();

	m_batches.clear();
	m_entityToBatch.clear();
	m_entityColorCache.clear();
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void AreaVolumeRenderSystem::DestroyBuffers()
{
	if (bgfx::isValid(m_volumeVB))
	{
		bgfx::destroy(m_volumeVB);
		m_volumeVB = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_volumeIB))
	{
		bgfx::destroy(m_volumeIB);
		m_volumeIB = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_outlineVB))
	{
		bgfx::destroy(m_outlineVB);
		m_outlineVB = BGFX_INVALID_HANDLE;
	}
}

void AreaVolumeRenderSystem::RebuildBuffers()
{
	DestroyBuffers();

	m_batches.clear();
	m_entityToBatch.clear();

	if (!m_registry)
		return;

	// Collect visible entities grouped by fill color.
	std::map<uint32_t, std::vector<entt::entity>> colorGroups;

	auto view = m_registry->view<AreaVolumeComponent, AreaVolumeRenderComponent, TransformComponent>();
	for (auto entity : view)
	{
		// Skip hidden entities
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& renderComp = view.get<AreaVolumeRenderComponent>(entity);
		uint32_t color = renderComp.color;

		m_entityToBatch[entity] = color;
		m_entityColorCache[entity] = color;

		colorGroups[color].push_back(entity);
	}

	// Build unified vertex and index arrays, recording batch ranges
	std::vector<SimpleColoredVertex> allVertices;
	std::vector<uint16_t> allIndices;
	std::vector<LineInstanceVertex> allLineInstances;

	for (auto& [color, group] : colorGroups)
	{
		// Combine volumes in this color group
		std::vector<glm::vec3> vertices;
		std::vector<std::array<uint16_t, 3>> faces;
		std::vector<std::array<uint16_t, 2>> edges;
		std::vector<uint32_t> debugFaceColors;  // Per-face colors for debug mode

		for (entt::entity entity : group)
		{
			const auto& volumeComp = view.get<AreaVolumeComponent>(entity);
			const auto& transformComp = view.get<TransformComponent>(entity);

			if (volumeComp.vertices.empty() || volumeComp.faces.empty())
				continue;

			glm::mat4 worldMat = transformComp.GetMatrix();
			uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

			// Transform vertices to world space and append
			vertices.reserve(vertices.size() + volumeComp.vertices.size());
			for (const auto& vert : volumeComp.vertices)
			{
				vertices.push_back(glm::vec3(worldMat * glm::vec4(vert, 1.0f)));
			}

			for (const auto& face : volumeComp.faces)
			{
				std::array<uint16_t, 3> adjustedFace{};
				adjustedFace[0] = face[0] + baseIndex;
				adjustedFace[1] = face[1] + baseIndex;
				adjustedFace[2] = face[2] + baseIndex;
				faces.push_back(adjustedFace);
			}

			for (const auto& edge : volumeComp.outerEdges)
			{
				std::array<uint16_t, 2> adjustedEdge{};
				adjustedEdge[0] =  edge[0] + baseIndex;
				adjustedEdge[1] =  edge[1] + baseIndex;
				edges.push_back(adjustedEdge);
			}
		}

		if (faces.empty())
		{
			continue;
		}

		// Store debug colors for use when building vertices
		if (m_debugColorByPlane)
		{
			debugFaceColors = DebugColorFacesByPlane(vertices, faces);
		}

		// Build buffers/batch for this color group
		AreaVolumeDrawBatch batch;
		batch.color = color;
		batch.indexStart = static_cast<uint32_t>(allIndices.size());
		batch.edgeStart = static_cast<uint32_t>(allLineInstances.size());
	
		// outline color - same RGB as fill, but full alpha
		uint32_t outlineColor = color | 0xFF000000;
		ImColor outlineImColor = ImColor(outlineColor);
		glm::vec4 outlineCol(outlineImColor.Value.x, outlineImColor.Value.y, outlineImColor.Value.z, outlineImColor.Value.w);

		// In debug mode, we emit vertices per-face so each face can have its own color
		if (m_debugColorByPlane && !debugFaceColors.empty())
		{
			// emit separate vertices for each face with per-face color
			for (size_t faceIdx = 0; faceIdx < faces.size(); ++faceIdx)
			{
				const auto& face = faces[faceIdx];
				uint32_t faceColor = (faceIdx < debugFaceColors.size()) ? debugFaceColors[faceIdx] : color;

				// Add vertices for this face
				uint16_t faceBaseVertex = static_cast<uint16_t>(allVertices.size());
				for (uint16_t idx : face)
				{
					SimpleColoredVertex v;
					v.pos = vertices[idx];
					v.color = faceColor;
					allVertices.push_back(v);
				}

				// Faces are triangulated
				allIndices.push_back(faceBaseVertex + 0);
				allIndices.push_back(faceBaseVertex + 1);
				allIndices.push_back(faceBaseVertex + 2);
			}
		}
		else
		{
			uint16_t vertexOffset = static_cast<uint16_t>(allVertices.size());

			allVertices.reserve(vertices.size() + allVertices.size());
			allIndices.reserve(faces.size() + allIndices.size());
			allLineInstances.reserve(faces.size() + allLineInstances.size());

			for (const auto& vert : vertices)
			{
				SimpleColoredVertex v;
				v.pos = vert;
				v.color = color;
				allVertices.push_back(v);
			}

			for (const auto& face : faces)
			{
				// Front
				allIndices.push_back(vertexOffset + face[0]);
				allIndices.push_back(vertexOffset + face[1]);
				allIndices.push_back(vertexOffset + face[2]);
				// Back
				// allIndices.push_back(vertexOffset + face[0]);
				// allIndices.push_back(vertexOffset + face[2]);
				// allIndices.push_back(vertexOffset + face[1]);
			}

			for (const auto& edge : edges)
			{
				uint16_t a = edge[0];
				uint16_t b = edge[1];
				if (a > b)
					std::swap(a, b);

				const glm::vec3& vert0 = vertices[a];
				const glm::vec3& vert1 = vertices[b];

				allLineInstances.emplace_back(vert0, m_lineWidth, outlineCol, vert1, m_lineWidth, outlineCol);
			}
		}

		batch.indexCount = static_cast<uint32_t>(allIndices.size()) - batch.indexStart;
		batch.edgeCount = static_cast<uint32_t>(allLineInstances.size()) - batch.edgeStart;

		m_batches.push_back(batch);
	}

	// Create unified GPU buffers
	if (!allVertices.empty() && !allIndices.empty())
	{
		m_volumeVB = bgfx::createVertexBuffer(
			bgfx::copy(allVertices.data(), static_cast<uint32_t>(allVertices.size() * sizeof(SimpleColoredVertex))),
			SimpleColoredVertex::ms_layout);

		m_volumeIB = bgfx::createIndexBuffer(
			bgfx::copy(allIndices.data(), static_cast<uint32_t>(allIndices.size() * sizeof(uint16_t))));
	}

	if (!allLineInstances.empty())
	{
		m_outlineVB = bgfx::createVertexBuffer(
			bgfx::copy(allLineInstances.data(), static_cast<uint32_t>(allLineInstances.size() * sizeof(LineInstanceVertex))),
			LineInstanceVertex::ms_layout);
	}

	m_dirty = false;
}

void AreaVolumeRenderSystem::Update()
{
	if (!m_registry)
		return;

	// Check for color changes on existing entities
	if (!m_dirty)
	{
		auto view = m_registry->view<AreaVolumeComponent, AreaVolumeRenderComponent>();
		for (auto entity : view)
		{
			const auto& renderComp = view.get<AreaVolumeRenderComponent>(entity);

			auto it = m_entityColorCache.find(entity);
			if (it != m_entityColorCache.end())
			{
				if (it->second != renderComp.color)
				{
					m_dirty = true;
					break;
				}
			}
		}
	}

	if (m_dirty)
	{
		RebuildBuffers();
	}
}

void AreaVolumeRenderSystem::Render()
{
	if (!m_registry)
		return;

	if (!bgfx::isValid(m_volumeVB) || !bgfx::isValid(m_volumeIB))
		return;

	// Render filled volumes - one draw call per color batch
	for (const auto& batch : m_batches)
	{
		if (batch.indexCount == 0)
			continue;

		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_volumeVB);
		encoder->setIndexBuffer(m_volumeIB, batch.indexStart, batch.indexCount);
		encoder->setState(
			BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA |
			BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z |
			BGFX_STATE_CULL_CW);

		encoder->submit(0, m_volumeProgram);
	}

	// Render outlines - one draw call per color batch
	if (!bgfx::isValid(m_outlineVB))
		return;

	for (const auto& batch : m_batches)
	{
		if (batch.edgeCount == 0)
			continue;

		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, g_zoneRenderManager->GetShared()->m_quad1VB);
		encoder->setIndexBuffer(g_zoneRenderManager->GetShared()->m_quadIB);
		encoder->setInstanceDataBuffer(m_outlineVB, batch.edgeStart, batch.edgeCount);
		encoder->setState(BGFX_STATE_MSAA |
			BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA |
			BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z);

		encoder->submit(0, m_linesProgram);
	}
}
