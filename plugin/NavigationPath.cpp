//
// NavigationPath.cpp
//

#include "NavigationPath.h"

#include "common/Navigator.h"
#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "plugin/DebugDrawDX.h"
#include "plugin/EmbedResources.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/NavMeshLoader.h"
#include "plugin/RenderHandler.h"

#include <DetourNavMesh.h>
#include <DetourCommon.h>
#include <glm/gtc/type_ptr.hpp>

const int MAX_WHATEVER = 8096;

//----------------------------------------------------------------------------

NavigationPath::NavigationPath(const std::shared_ptr<DestinationInfo>& dest)
{
	auto* mesh = g_mq2Nav->Get<NavMesh>();
	m_navMeshConn = mesh->OnNavMeshChanged.Connect(
		[this, mesh]() { SetNavMesh(mesh->GetNavMesh()); });
	m_route = std::make_shared<NavigationRoute>();
	
	SetNavMesh(mesh->GetNavMesh(), false);

	SetDestination(dest);
}

NavigationPath::~NavigationPath()
{
	SetShowNavigationPaths(false);
}

//----------------------------------------------------------------------------

void NavigationPath::SetShowNavigationPaths(bool renderPaths)
{
	if (m_renderPaths == renderPaths)
		return;

	m_renderPaths = renderPaths;

	if (m_renderPaths)
	{
		m_line = std::make_shared<NavigationLine>(this);
		m_line->SetVisible(nav::GetSettings().show_nav_path);
		g_renderHandler->AddRenderable(m_line.get());

		m_debugDrawGrp = std::make_unique<RenderGroup>(g_pDevice);
		g_renderHandler->AddRenderable(m_debugDrawGrp.get());
	}
	else
	{
		if (m_debugDrawGrp)
			g_renderHandler->RemoveRenderable(m_debugDrawGrp.get());

		if (m_line)
			g_renderHandler->RemoveRenderable(m_line.get());

		m_line.reset();
		m_debugDrawGrp.reset();
	}
}

void NavigationPath::SetDestination(const std::shared_ptr<DestinationInfo>& info)
{
	m_destinationInfo = info;

	// do this here because we don't want to keep calculating this every time an update is called
	auto* mesh = g_mq2Nav->Get<NavMesh>();
	if (mesh && m_destinationInfo && m_destinationInfo->heightType == HeightType::Nearest)
	{
		glm::vec3 transformed = {
			m_destinationInfo->eqDestinationPos.x,
			m_destinationInfo->eqDestinationPos.z,
			m_destinationInfo->eqDestinationPos.y
		};

		auto heights = mesh->GetHeights(transformed);

		// we don't actually want to calculate the path at the current height since there
		// is no guarantee it is on the mesh
		float current_distance = std::numeric_limits<float>().max();

		// create a destination location out of the given x/y coordinate and each z coordinate
		// that was hit at that location. Calculate the length of each and take the z coordinate that
		// creates the shortest path.
		for (auto height : heights)
		{
			float current_height = m_destinationInfo->eqDestinationPos.z;
			m_destinationInfo->eqDestinationPos.z = height;

			if (!FindPath() || GetPathTraversalDistance() > current_distance)
			{
				m_destinationInfo->eqDestinationPos.z = current_height;
			} // else leave it, it's a better height
		}
	}

	m_needReplan = true;
}

void NavigationPath::SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh,
	bool updatePath)
{
	m_navMesh = navMesh;
	m_navigator = std::make_unique<Navigator>(g_mq2Nav->Get<NavMesh>(), MAX_WHATEVER); // TODO: max nodes = config

	auto& settings = nav::GetSettings();
	if (settings.use_find_polygon_extents)
	{
		m_navigator->SetExtents(settings.find_polygon_extents);
	}

	auto& filter = m_navigator->GetQueryFilter();
	filter.setIncludeFlags(+PolyFlags::All);
	filter.setExcludeFlags(+PolyFlags::Disabled);
	if (auto* mesh = g_mq2Nav->Get<NavMesh>())
	{
		mesh->FillFilterAreaCosts(filter);
	}

	bool wasRunning = (m_state != NavigationState::Inactive);
	ResetState();

	if (updatePath && wasRunning)
	{
		UpdatePath();
	}
}


void NavigationPath::ResetState()
{
	m_needReplan = true;
	m_offMesh = false;
	m_failed = false;
	m_state = NavigationState::Inactive;
	m_route->clear();
}

bool NavigationPath::FindPath()
{
	if (!m_navMesh)
		return false;

	if (!m_destinationInfo || !m_destinationInfo->valid)
		return false;

	ResetState();
	UpdatePositions();
	m_failed = FindRoute();
	m_state = m_failed ? NavigationState::Inactive : NavigationState::Active;

	return !m_failed && !m_route->empty();
}

void NavigationPath::UpdatePositions()
{
	// we always use our own character as the source.
	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me != nullptr)
	{
		m_pos = { me->X, me->FloorHeight, me->Y };
	}

	if (m_destinationInfo)
	{
		auto& dest = m_destinationInfo->eqDestinationPos;
		m_target = { dest.x, dest.z, dest.y };
	}
}

bool NavigationPath::FindRoute()
{
	bool success =
		m_navigator->FindRoute(m_pos, m_target, m_allowPartial);
	if (success)
	{
		m_navigator->FindCorners(m_route->GetBuffer(), MAX_WHATEVER);
	}
	else
	{
		WriteChatf("\arFailed!");
	}

	return success;
}

void NavigationPath::UpdatePath()
{
	// we need a navmesh and a destination before we can start
	if (m_navMesh == nullptr || m_destinationInfo == nullptr)
		return;

	if (m_failed)
		return;

	UpdatePositions();

	// Update my position
	m_navigator->MovePosition(m_pos);

	// Update target
	m_navigator->MoveTargetPosition(m_target);

	if (!m_offMesh)
	{
		if (m_needReplan)
		{
			if (FindRoute())
			{
				m_needReplan = false;
			}
		}

		m_havePath = !m_needReplan;
		dtPolyRef conPoly = 0;

		if (overOffMeshConnectionStart(m_navigator->getPos(),
			m_route->GetBuffer(), m_navMesh.get(), &conPoly))
		{
			m_navigator->MoveOverOffMeshConnection(conPoly,
				m_offMeshStart, m_offMeshEnd);

			m_offMesh = true;
			m_state = NavigationState::OffMeshLink;
		}
	}

	if (m_offMesh)
	{
		if (withinRadiusOfOffMeshConnection(
			m_navigator->getPos(), m_offMeshEnd, m_offMeshPoly, m_navMesh.get()))
		{
			m_offMesh = false;
			m_offMeshPoly = 0;
			m_offMeshStart = m_offMeshEnd = {};
			m_state = NavigationState::Active;
		}
	}

	if (m_debugDrawGrp)
		m_debugDrawGrp->Reset();

	auto& settings = nav::GetSettings();

	if (m_debugDrawGrp && settings.debug_render_pathing)
	{
		DebugDrawDX dd(m_debugDrawGrp.get());

		// draw current position
		duDebugDrawCross(&dd, m_pos.x, m_pos.y, m_pos.z, 0.5, DXColor(51, 255, 255), 1);
		int i = 0;

		// Draw the waypoints. Green is next point
		for (const auto& node : *m_route)
		{
			int color = DXColor(255, 255, 255);
			if (i == 0)
				color = DXColor(0, 102, 204);
			else if (i == m_route->size() - 1)
				color = DXColor(255, 0, 0);
			else
				color = DXColor(0, 255, 0);

			if (node.flags() & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
			{
				color = DXColor(255, 255, 0);
			}

			const glm::vec3& pos = node.pos();

			duDebugDrawCross(&dd, pos.x, pos.y, pos.z, 0.5, color, 1);
			++i;
		}
	}

	PathUpdated();
}

float NavigationPath::GetPathTraversalDistance() const
{
	return m_route ? m_route->GetPathTraversalDistance() : 0.f;
}

void NavigationPath::RenderUI()
{
	bool showPath = nav::GetSettings().show_nav_path;
	if (ImGui::Checkbox("Show navigation path", &showPath))
	{
		SetShowNavigationPaths(showPath);

		nav::GetSettings().show_nav_path = showPath;
		nav::SaveSettings(false);

		if (m_line)
			m_line->SetVisible(showPath);
	}

#if DEBUG_NAVIGATION_LINES
	if (m_line)
		m_line->RenderUI();
#endif
}

glm::vec3 NavigationPath::GetDestination() const
{
	return m_destinationInfo ? m_destinationInfo->eqDestinationPos : glm::vec3{};
}

//----------------------------------------------------------------------------

NavigationLine::NavigationLine(NavigationPath* path)
	: m_path(path)
{
	m_renderPasses.emplace_back(ImColor(0, 0, 0, 200), 0.9f);
	m_renderPasses.emplace_back(ImColor(52, 152, 219, 200), 0.5f);
	m_renderPasses.emplace_back(ImColor(241, 196, 15, 200), 0.5f);

	m_pathUpdated = path->PathUpdated.Connect(
		[this]() { if (nav::GetSettings().show_nav_path) Update(); });
}

NavigationLine::~NavigationLine()
{
	InvalidateDeviceObjects();
}

bool NavigationLine::CreateDeviceObjects()
{
	if (!m_visible)
		return true;

	auto shaderFile = LoadResource(IDR_VOLUMELINES_FX);
	if (shaderFile.empty())
		return false;

	ID3DXBuffer* errors = nullptr;

	HRESULT hr = D3DXCreateEffect(
		g_pDevice,
		shaderFile.data(),
		shaderFile.length(),
		nullptr,
		nullptr,
		0,
		nullptr,
		&m_effect,
		&errors);

	if (FAILED(hr))
	{
		if (errors)
		{
			DebugSpewAlways("Effect error: %s", errors->GetBufferPointer());

			errors->Release();
			errors = nullptr;
		}

		InvalidateDeviceObjects();
		return false;
	}

	D3DVERTEXELEMENT9 vertexElements[] =
	{
		{ 0,  0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
		{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,  0 },
		{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
		{ 0, 36,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
		{ 0, 40,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2 },
		D3DDECL_END()
	};

	hr = g_pDevice->CreateVertexDeclaration(vertexElements, &m_vDeclaration);
	if (FAILED(hr))
	{
		InvalidateDeviceObjects();
		return false;
	}

	m_loaded = true;
	return true;
}

void NavigationLine::InvalidateDeviceObjects()
{
	if (m_effect)
	{
		m_effect->Release();
		m_effect = nullptr;
	}

	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

	if (m_vDeclaration)
	{
		m_vDeclaration->Release();
		m_vDeclaration = nullptr;
	}

	m_loaded = false;
}

void NavigationLine::Render(RenderPhase phase)
{
	if (phase != Renderable::Render_Geometry)
		return;
	if (!m_loaded)
		return;
	if (m_needsUpdate)
		GenerateBuffers();
	if (!m_vertexBuffer)
		return;

	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	g_pDevice->SetTransform(D3DTS_WORLD, &world);

	D3DXMATRIX view, proj, mWVP, mWV;
	g_pDevice->GetTransform(D3DTS_VIEW, &view);
	g_pDevice->GetTransform(D3DTS_PROJECTION, &proj);

	mWV = world * view;
	mWVP = world * view * proj;

	// We will not be using a viewing transformation, so the view matrix will be identity.
	m_effect->SetMatrix("mWV", &mWV);
	m_effect->SetMatrix("mWVP", &mWVP);

	DWORD depthTest = 0, depthFunc = 0;
	g_pDevice->GetRenderState(D3DRS_ZENABLE, &depthTest);
	g_pDevice->GetRenderState(D3DRS_ZFUNC, &depthFunc);

	g_pDevice->SetVertexDeclaration(m_vDeclaration);
	g_pDevice->SetStreamSource(0, m_vertexBuffer, 0, sizeof(TVertex));

	UINT passes = 0;
	m_effect->Begin(&passes, 0);

	for (int iPass = 0; iPass < passes; iPass++)
	{

		RenderStyle style;
		if (iPass < m_renderPasses.size())
			style = m_renderPasses[iPass];

		if (!style.enabled)
			continue;

		m_effect->BeginPass(iPass);

		m_effect->SetVector("lineColor", (D3DXVECTOR4*)&style.render_color);
		m_effect->SetFloat("lineWidth", style.width);
		m_effect->CommitChanges();

		// render
		for (auto& cmd : m_commands)
		{
			g_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,
				cmd.StartVertex, cmd.PrimitiveCount);
		}

		m_effect->EndPass();
	}

	m_effect->End();

	g_pDevice->SetRenderState(D3DRS_ZENABLE, depthTest);
	g_pDevice->SetRenderState(D3DRS_ZFUNC, depthFunc);
}

void NavigationLine::GenerateBuffers()
{
	if (!m_path->GetRoute())
		return;

	auto& route = *m_path->GetRoute();
	int size = (int)route.size();

	if (size == m_lastSize)
	{
		// check if the path actually changed.
	}

	if (size <= 0)
		return;

	int bufferLength = (size - 1) * 4;

	if (!m_vertexBuffer || size > m_lastSize)
	{
		if (m_vertexBuffer)
			m_vertexBuffer->Release();

		HRESULT hr = g_pDevice->CreateVertexBuffer(bufferLength * sizeof(TVertex),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, TVertex::FVF, D3DPOOL_DEFAULT,
			&m_vertexBuffer, nullptr);

		if (FAILED(hr))
		{
			m_vertexBuffer = nullptr;
			return;
		}
	}

	m_lastSize = size;
	TVertex* vertexDest = nullptr;

	if (m_vertexBuffer->Lock(0, bufferLength * sizeof(TVertex), (void**)&vertexDest, D3DLOCK_DISCARD) < 0)
		return;

	// get the path as if it were a series of 3d points
	D3DXVECTOR3* pt = (D3DXVECTOR3*)&route.begin()->pos();

	int index = 0;
	m_commands.resize(size - 1);

	for (int i = 0; i < size - 1; ++i, ++pt)
	{
		// this point
		D3DXVECTOR3 v0;
		v0.x = pt[0].z;
		v0.y = pt[0].x;
		v0.z = pt[0].y + 1;

		// next point
		D3DXVECTOR3 v1;
		v1.x = pt[1].z;
		v1.y = pt[1].x;
		v1.z = pt[1].y + 1;


		// following point
		D3DXVECTOR3 nextPos;
		int nextIdx = i < size - 2 ? 2 : 1;
		nextPos.x = pt[nextIdx].z;
		nextPos.y = pt[nextIdx].x;
		nextPos.z = pt[nextIdx].y + 1;

		// previous point
		D3DXVECTOR3 prevPos;
		int prevIdx = i > 0 ? -1 : 0;
		prevPos.x = pt[prevIdx].z;
		prevPos.y = pt[prevIdx].x;
		prevPos.z = pt[prevIdx].y + 1;

		vertexDest[index + 0].pos = v0;
		vertexDest[index + 0].otherPos = v1;
		vertexDest[index + 0].thickness = -m_thickness;
		vertexDest[index + 0].adjPos = prevPos;
		vertexDest[index + 0].adjHint = prevIdx;

		vertexDest[index + 1].pos = v1;
		vertexDest[index + 1].otherPos = v0;
		vertexDest[index + 1].thickness = m_thickness;
		vertexDest[index + 1].adjPos = nextPos;
		vertexDest[index + 1].adjHint = nextIdx - 1;

		vertexDest[index + 2].pos = v0;
		vertexDest[index + 2].otherPos = v1;
		vertexDest[index + 2].thickness = m_thickness;
		vertexDest[index + 2].adjPos = prevPos;
		vertexDest[index + 2].adjHint = prevIdx;

		vertexDest[index + 3].pos = v1;
		vertexDest[index + 3].otherPos = v0;
		vertexDest[index + 3].thickness = -m_thickness;
		vertexDest[index + 3].adjPos = nextPos;
		vertexDest[index + 3].adjHint = nextIdx - 1;

		m_commands[i].StartVertex = index;
		m_commands[i].PrimitiveCount = 2; // 2 triangles in a strip
		index += 4;
	}

	m_vertexBuffer->Unlock();
	m_needsUpdate = false;
}

void NavigationLine::SetThickness(float thickness)
{
	if (thickness != m_thickness)
	{
		m_thickness = thickness;
		Update();
	}
}

void NavigationLine::SetVisible(bool visible)
{
	if (m_visible != visible)
	{
		m_visible = visible;

		if (m_visible) {
			m_needsUpdate = true;
			CreateDeviceObjects();
		}
		else
			InvalidateDeviceObjects();
	}
}

#if DEBUG_NAVIGATION_LINES
void NavigationLine::RenderUI()
{
	ImGui::PushID(this);
	if (ImGui::CollapsingHeader("Navigation Line Debug"))
	{
		int size = m_renderPasses.size();
		if (ImGui::InputInt("Render Passes", &size))
			m_renderPasses.resize(size);

		for (int i = 0; i < m_renderPasses.size(); ++i)
		{
			ImGui::PushID(i);
			auto& config = m_renderPasses[i];
			ImGui::Checkbox("Enabled", &config.enabled);
			ImGui::ColorEdit4("Color", (float*)&config.render_color,
				ImGuiColorEditFlags_RGB);
			ImGui::InputFloat("Width", &config.width);

			ImGui::PopID();
		}
	}

	ImGui::PopID();
}
#endif

//----------------------------------------------------------------------------
