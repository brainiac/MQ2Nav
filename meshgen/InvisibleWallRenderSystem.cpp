//
// InvisibleWallRenderSystem.cpp
//

#include "pch.h"
#include "InvisibleWallRenderSystem.h"

#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"

#include "spdlog/spdlog.h"
#include "imgui/imgui.h"

//============================================================================

InvisibleWallRenderSystem::InvisibleWallRenderSystem(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	m_volumeProgram = g_resourceMgr->GetProgramHandle("areavolume");
	m_linesProgram = g_resourceMgr->GetProgramHandle("lines");
}

InvisibleWallRenderSystem::~InvisibleWallRenderSystem()
{
	if (m_registry)
	{
		m_invisibleWallConstructConnection.release();
		m_invisibleWallDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	DestroyBuffers();
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void InvisibleWallRenderSystem::SetRegistry(entt::registry* registry)
{
	if (m_registry)
	{
		m_invisibleWallConstructConnection.release();
		m_invisibleWallDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	m_registry = registry;

	if (m_registry)
	{
		m_invisibleWallConstructConnection = m_registry->on_construct<InvisibleWallComponent>()
			.connect<&InvisibleWallRenderSystem::OnInvisibleWallConstruct>(this);
		m_invisibleWallDestroyConnection = m_registry->on_destroy<InvisibleWallComponent>()
			.connect<&InvisibleWallRenderSystem::OnInvisibleWallDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&InvisibleWallRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&InvisibleWallRenderSystem::OnHiddenDestroy>(this);
	}

	m_dirty = true;
}

void InvisibleWallRenderSystem::OnInvisibleWallConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void InvisibleWallRenderSystem::OnInvisibleWallDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void InvisibleWallRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<InvisibleWallComponent>(entity))
	{
		m_dirty = true;
	}
}

void InvisibleWallRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<InvisibleWallComponent>(entity))
	{
		m_dirty = true;
	}
}

void InvisibleWallRenderSystem::DestroyBuffers()
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

	m_indexCount = 0;
	m_edgeCount = 0;
}

void InvisibleWallRenderSystem::RebuildBuffers()
{
	DestroyBuffers();

	if (!m_registry)
		return;

	std::vector<SimpleColoredVertex> allVertices;
	std::vector<uint16_t> allIndices;
	std::vector<LineInstanceVertex> allLineInstances;

	// Query all visible invisible wall entities
	auto view = m_registry->view<InvisibleWallComponent, InvisibleWallRenderComponent>();

	// Pre-calculate size
	size_t totalSegments = 0;
	for (auto entity : view)
	{
		// Skip hidden entities
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& wallComp = view.get<InvisibleWallComponent>(entity);
		if (wallComp.vertices.size() >= 2)
		{
			totalSegments += wallComp.vertices.size() - 1;
		}
	}

	if (totalSegments == 0)
	{
		m_dirty = false;
		return;
	}

	// Reserve space - each segment is a quad (4 vertices, 12 indices for double-sided, 4 edges)
	allVertices.reserve(totalSegments * 4);
	allIndices.reserve(totalSegments * 12);
	allLineInstances.reserve(totalSegments * 4);

	size_t wallCount = 0;

	// Build wall geometry from components
	for (auto entity : view)
	{
		// Skip hidden entities
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& wallComp = view.get<InvisibleWallComponent>(entity);
		const auto& renderComp = view.get<InvisibleWallRenderComponent>(entity);

		const auto& verts = wallComp.vertices;
		float wallHeight = wallComp.wallHeight;

		if (verts.size() < 2)
			continue;

		// Get colors from render component
		uint32_t fillColor = renderComp.fillColor;
		uint32_t outlineColor = renderComp.outlineColor;
		float lineWidth = renderComp.lineWidth;

		ImColor outlineImColor = ImColor(outlineColor);
		glm::vec4 outlineCol(outlineImColor.Value.x, outlineImColor.Value.y, outlineImColor.Value.z, outlineImColor.Value.w);

		// Build quads for each segment of the wall
		for (size_t j = 0; j + 1 < verts.size(); ++j)
		{
			glm::vec3 v1 = verts[j];
			glm::vec3 v2 = verts[j + 1];

			// Extrude upward by wall height
			glm::vec3 v3 = v1;
			v3.z += wallHeight;

			glm::vec3 v4 = v2;
			v4.z += wallHeight;

			// Create quad vertices: v1 (bottom-left), v2 (bottom-right), v3 (top-left), v4 (top-right)
			uint16_t baseIndex = static_cast<uint16_t>(allVertices.size());

			allVertices.emplace_back(v1, fillColor);
			allVertices.emplace_back(v2, fillColor);
			allVertices.emplace_back(v3, fillColor);
			allVertices.emplace_back(v4, fillColor);

			// Two triangles for the quad (front-facing)
			// Triangle 1: v1, v2, v3
			allIndices.push_back(baseIndex + 0);
			allIndices.push_back(baseIndex + 1);
			allIndices.push_back(baseIndex + 2);

			// Triangle 2: v2, v4, v3
			allIndices.push_back(baseIndex + 1);
			allIndices.push_back(baseIndex + 3);
			allIndices.push_back(baseIndex + 2);

			// Back-facing triangles for double-sided rendering
			// Triangle 3: v3, v2, v1
			allIndices.push_back(baseIndex + 2);
			allIndices.push_back(baseIndex + 1);
			allIndices.push_back(baseIndex + 0);

			// Triangle 4: v3, v4, v2
			allIndices.push_back(baseIndex + 2);
			allIndices.push_back(baseIndex + 3);
			allIndices.push_back(baseIndex + 1);

			// Outline edges - 4 edges per quad
			// Bottom edge
			allLineInstances.emplace_back(v1, lineWidth, outlineCol, v2, lineWidth, outlineCol);
			// Top edge
			allLineInstances.emplace_back(v3, lineWidth, outlineCol, v4, lineWidth, outlineCol);
			// Left edge
			allLineInstances.emplace_back(v1, lineWidth, outlineCol, v3, lineWidth, outlineCol);
			// Right edge
			allLineInstances.emplace_back(v2, lineWidth, outlineCol, v4, lineWidth, outlineCol);
		}

		++wallCount;
	}

	// Create GPU buffers
	if (!allVertices.empty() && !allIndices.empty())
	{
		m_volumeVB = bgfx::createVertexBuffer(
			bgfx::copy(allVertices.data(), static_cast<uint32_t>(allVertices.size() * sizeof(SimpleColoredVertex))),
			SimpleColoredVertex::ms_layout);

		m_volumeIB = bgfx::createIndexBuffer(
			bgfx::copy(allIndices.data(), static_cast<uint32_t>(allIndices.size() * sizeof(uint16_t))));

		m_indexCount = static_cast<uint32_t>(allIndices.size());
	}

	if (!allLineInstances.empty())
	{
		m_outlineVB = bgfx::createVertexBuffer(
			bgfx::copy(allLineInstances.data(), static_cast<uint32_t>(allLineInstances.size() * sizeof(LineInstanceVertex))),
			LineInstanceVertex::ms_layout);

		m_edgeCount = static_cast<uint32_t>(allLineInstances.size());
	}

	SPDLOG_DEBUG("InvisibleWallRenderSystem: Built {} walls with {} vertices, {} indices, {} edges",
		wallCount, allVertices.size(), allIndices.size(), allLineInstances.size());

	m_dirty = false;
}

void InvisibleWallRenderSystem::Update()
{
	if (m_dirty)
	{
		RebuildBuffers();
	}
}

void InvisibleWallRenderSystem::Render()
{
	if (!m_visible)
		return;

	if (!bgfx::isValid(m_volumeVB) || !bgfx::isValid(m_volumeIB))
		return;

	// Render filled volumes
	if (m_indexCount > 0)
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_volumeVB);
		encoder->setIndexBuffer(m_volumeIB, 0, m_indexCount);
		encoder->setState(
			BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA |
			BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z);

		encoder->submit(0, m_volumeProgram);
	}

	// Render outlines
	if (bgfx::isValid(m_outlineVB) && m_edgeCount > 0)
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, g_zoneRenderManager->GetShared()->m_quad1VB);
		encoder->setIndexBuffer(g_zoneRenderManager->GetShared()->m_quadIB);
		encoder->setInstanceDataBuffer(m_outlineVB, 0, m_edgeCount);
		encoder->setState(BGFX_STATE_MSAA |
			BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA |
			BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z);

		encoder->submit(0, m_linesProgram);
	}
}
