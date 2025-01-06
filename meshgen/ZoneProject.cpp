
#include "pch.h"
#include "ZoneProject.h"

#include "meshgen/Application.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/Editor.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshBuilder.h"
#include "meshgen/ZoneRenderManager.h"
#include "meshgen/ZoneResourceManager.h"
#include "common/Formatters.h"
#include "common/NavMeshData.h"
#include "Recast.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <taskflow/taskflow.hpp>


NavMeshProject::NavMeshProject(Editor* editor, std::string_view shortName)
	: m_editor(editor)
	, m_shortName(shortName)
{
	m_navMesh = std::make_shared<NavMesh>(g_config.GetOutputPath(), m_shortName);
}

NavMeshProject::~NavMeshProject()
{
}

void NavMeshProject::InitWithProject(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_zoneProj = zoneProject;
	m_navMesh->SetNavMeshBounds(zoneProject->GetMeshBoundsMin(), zoneProject->GetMeshBoundsMax());
	m_builder = std::make_shared<NavMeshBuilder>(shared_from_this());
}

void NavMeshProject::OnUpdate(float timeStep)
{
	if (m_builder)
	{
		m_builder->Update();
	}
}

std::shared_ptr<dtNavMesh> NavMeshProject::GetDetourNavMesh() const
{
	return m_navMesh ? m_navMesh->GetNavMesh() : nullptr;
}

void NavMeshProject::ResetNavMesh()
{
	std::unique_lock lock(m_mutex);

	m_navMesh->ResetNavMesh();
}

bool NavMeshProject::LoadNavMesh(bool allowMissing)
{
	if (IsBuilding())
		return false;

	// Make sure that the path is up-to-date
	m_navMesh->SetNavMeshDirectory(g_config.GetOutputPath());

	auto result = m_navMesh->LoadNavMeshFile();

	// Missing file is OK if we allow it.
	if (result != NavMesh::LoadResult::Success && (!allowMissing || result != NavMesh::LoadResult::MissingFile))
	{
		std::string failedReason;

		switch (result)
		{
		case NavMesh::LoadResult::MissingFile:
			failedReason = "No navmesh file found";
			break;
		case NavMesh::LoadResult::Corrupt:
			failedReason = "Navmesh file is corrupt";
			break;
		case NavMesh::LoadResult::VersionMismatch:
			failedReason = "Navmesh file is not compatible with this version";
			break;
		case NavMesh::LoadResult::ZoneMismatch:
			failedReason = "Navmesh file is for the wrong zone";
			break;
		case NavMesh::LoadResult::OutOfMemory:
			failedReason = "Ran out of memory!";
			break;
		default:
			failedReason = fmt::format("Failed to load navmesh ({})", static_cast<int>(result));
			break;
		}

		SPDLOG_ERROR("Failed to load navmesh: {}", failedReason);

		m_editor->ShowNotificationDialog("Failed To Open Navmesh", failedReason);
		return false;
	}

	m_navMeshConfig = m_navMesh->GetNavMeshConfig();

	return true;
}

bool NavMeshProject::SaveNavMesh()
{
	if (IsBuilding() || !IsLoaded())
	{
		SPDLOG_DEBUG("Cannot save navmesh right now");
		return false;
	}

	// Make sure that the path is up-to-date
	m_navMesh->SetNavMeshDirectory(g_config.GetOutputPath());

	// Save config to navmesh? Or did we already do that when we built it?
	bool success = m_navMesh->SaveNavMeshFile();

	if (success)
	{
		SPDLOG_INFO("Navmesh saved: {}", m_navMesh->GetDataFileName());
	}
	else
	{
		SPDLOG_ERROR("Failed to save navmesh: {}", m_navMesh->GetDataFileName());
	}

	return success;
}

bool NavMeshProject::IsLoaded() const
{
	return m_navMesh->IsNavMeshLoaded();
}

bool NavMeshProject::IsBuilding() const
{
	return m_builder && m_builder->IsBuildingTiles();
}

void NavMeshProject::ResetNavMeshConfig()
{
	m_navMeshConfig = NavMeshConfig{};
}

void NavMeshProject::CancelBuild(bool wait)
{
	if (m_builder && m_builder->IsBuildingTiles())
		m_builder->CancelBuild(wait);
}

void NavMeshProject::RemoveTile(const glm::vec3& pos, int layer)
{
	SPDLOG_DEBUG("Remove tile: {}", pos);

	glm::ivec2 tilePos = m_navMesh->GetTilePos(pos);
	RemoveTileAt(tilePos.x, tilePos.y, layer);
}

void NavMeshProject::RemoveTileAt(int tx, int ty, int layer)
{
	SPDLOG_DEBUG("Remove tile at: x={} y={} layer={}", tx, ty, layer);

	m_navMesh->RemoveTileAt(tx, ty, layer);
}

void NavMeshProject::RemoveAllTiles()
{
	SPDLOG_DEBUG("Remove all tiles");

	m_navMesh->RemoveAllTiles();
}

//========================================================================

ZoneProject::ZoneProject(Editor* editor, const std::string& name)
	: m_editor(editor)
	, m_zoneShortName(name)
	, m_displayName(name)
{
	m_renderManager = std::make_unique<ZoneRenderManager>(this);
	m_renderManager->InitShared();
	m_renderManager->GetNavMeshRender()->SetFlags(
		ZoneNavMeshRender::DRAW_TILES | ZoneNavMeshRender::DRAW_TILE_BOUNDARIES | ZoneNavMeshRender::DRAW_CLOSED_LIST
	);

	m_resourceMgr = std::make_unique<ZoneResourceManager>(m_zoneShortName);

	// Start with an empty navmesh project
	m_navMeshProj = std::make_shared<NavMeshProject>(editor, name);
}

ZoneProject::~ZoneProject()
{
	OnShutdown();
}

void ZoneProject::OnUpdate(float timeStep)
{
	m_navMeshProj->OnUpdate(timeStep);
}

void ZoneProject::OnShutdown()
{
	CancelTasks();
	m_renderManager->DestroyObjects();
}

void ZoneProject::LoadZone(bool loadNavMesh)
{
	if (m_loadZoneTask.valid())
		return;

	auto shared_this = shared_from_this();
	m_navMeshProj->InitWithProject(shared_this);

	tf::Taskflow loadFlow = BuildLoadZoneTaskflow(loadNavMesh,
		[shared_this](bool success, ResultState result)
		{
			Application::Get().GetBackgroundTaskManager().PostToMainThread(
				[shared_this, success, result = std::move(result)]()
				{
					shared_this->OnLoadZoneComplete(success, result);
				});
		});

	// Log the taskflow for debugging
	//std::stringstream ss;
	//taskflow.dump(ss);
	//bx::debugPrintf("Taskflow: %s", ss.str().c_str());

	m_loadZoneTask = Application::Get().GetBackgroundTaskManager().RunTask(std::move(loadFlow));
}

void ZoneProject::OnLoadZoneComplete(bool success, const ResultState& result)
{
	if (success)
	{
		m_zoneLoaded.store(true);

		m_editor->OnProjectLoaded(shared_from_this());
		m_renderManager->OnNavMeshChanged(m_navMeshProj);
		m_renderManager->Rebuild();
	}
	else
	{
		if (result.phase == TaskPhase::LoadNavMesh)
		{
			m_editor->ShowNotificationDialog("Failed to load navmesh", result.message);
		}
		else
		{
			m_editor->ShowNotificationDialog("Failed to load zone", result.message);
		}
	}
}

tf::Taskflow ZoneProject::BuildLoadZoneTaskflow(bool loadNavMesh, ZoneContextCallback callback)
{
	tf::Taskflow taskflow;
	taskflow.name("loadZone");

	auto sharedThis = shared_from_this();

	std::shared_ptr<ResultState> resultState = std::make_shared<ResultState>();
	*resultState = {
		.phase = TaskPhase::LoadZoneData,
		.failed = false,
		.message = ""
	};

	// Task to handle failure at any stage.
	auto failureTask = taskflow.emplace([sharedThis, resultState, callback]()
		{
			sharedThis->SetProgress({ .display = false });
			sharedThis->m_zoneLoading.store(false);

			callback(false, *resultState);
		}).name("failure");

	// Task to handle success at the end.
	auto successTask = taskflow.emplace([sharedThis, resultState, callback]()
		{
			sharedThis->SetProgress({ .display = false });
			sharedThis->m_zoneLoading.store(false);

			callback(true, *resultState);
		}).name("success");

	// Task to load the zone data from the game files
	auto loadZoneTask = taskflow.emplace([sharedThis, resultState]() -> int
		{
			sharedThis->m_zoneLoading.store(true);
			auto phase = TaskPhase::LoadZoneData;

			// Start the progress
			sharedThis->SetProgress({
				.phase = phase,
				.display = true,
				.text = fmt::format("Loading {}...", sharedThis->GetShortName()),
				.value = 0.0f
			});

			// Do the work
			if (sharedThis->LoadZoneData())
			{
				return 1; // proceed to next step
			}

			// Set up result info and stop
			resultState->failed = true;
			resultState->phase = phase;
			resultState->message = fmt::format("Failed to load zone: '{}'", sharedThis->GetShortName());
			return 0;

		}).name("loadZone");

	auto buildZoneScene = taskflow.emplace([sharedThis, resultState]()
		{
			SPDLOG_INFO("TODO: Build scene");
			return 1;
		}).name("buildScene");

	loadZoneTask.precede(failureTask, buildZoneScene);

	// Task to build the triangle mesh / perform spatial partitioning
	auto buildPartitioning = taskflow.emplace([sharedThis, resultState]()
		{
			auto phase = TaskPhase::BuildTriangleMesh;

			// Start the progress
			sharedThis->SetProgress({
				.phase = phase,
				.display = true,
				.text = fmt::format("Generating triangle mesh..."),
				.value = 0.33f
			});

			// Do the work
			if (sharedThis->BuildTriangleMesh())
			{
				// If load succeeded, we can submit the zone context at this time.
				// TODO: Submit zone

				return 1; // proceed to next step
			}

			// Set up result info and stop
			resultState->failed = true;
			resultState->phase = phase;
			resultState->message = fmt::format("Failed to build triangle mesh: '{}'", sharedThis->GetShortName());
			return 0;

		}).name("buildPartitioning");

	buildZoneScene.precede(failureTask, buildPartitioning);

	// Task to load navmesh
	auto loadNavMeshTask = taskflow.emplace([sharedThis, resultState, loadNavMesh]()
		{
			if (!loadNavMesh)
				return 0;

			auto phase = TaskPhase::LoadNavMesh;

			// Start the progress
			sharedThis->SetProgress({
				.phase = phase,
				.display = true,
				.text = fmt::format("Loading navmesh...", sharedThis->GetShortName()),
				.value = 0.9f
			});

			// Do the work.
			if (sharedThis->GetNavMeshProject()->LoadNavMesh(true))
			{
				return 1; // proceed to next step
			}

			// Set up result info and stop
			resultState->failed = true;
			resultState->phase = phase;
			resultState->message = fmt::format("Failed to load navmesh: '{}'", sharedThis->GetShortName());
			return 0;

		}).name("loadNavMesh");

	buildPartitioning.precede(failureTask, loadNavMeshTask);
	loadNavMeshTask.precede(failureTask, successTask);

	return taskflow;
}

bool ZoneProject::LoadZoneData()
{
	std::string eqPath = g_config.GetEverquestPath();
	std::string outputPath = g_config.GetOutputPath();

	m_loader = std::make_unique<MapGeometryLoader>(m_zoneShortName, eqPath, outputPath);

	if (g_config.GetUseMaxExtents())
	{
		auto iter = MaxZoneExtents.find(m_zoneShortName);
		if (iter != MaxZoneExtents.end())
		{
			m_loader->SetMaxExtents(iter->second);
		}
	}

	// Get the long name from the application config
	m_zoneLongName = g_config.GetLongNameForShortName(m_zoneShortName);
	m_displayName = fmt::format("{} ({})", m_zoneLongName, m_zoneShortName);

	if (!m_loader->Load())
	{
		SPDLOG_ERROR("LoadZone: Failed to load '{}'", m_zoneShortName);
		return false;
	}

	if (!m_loader->IsLoaded())
	{
		SPDLOG_ERROR("LoadZone: Zone loaded but had no geometry: '{}'", m_zoneShortName);
		return false;
	}

	m_meshBMin = m_loader->GetCollisionMesh().m_boundsMin;
	m_meshBMax = m_loader->GetCollisionMesh().m_boundsMax;

	m_zoneDataLoaded = true;
	return true;
}

bool ZoneProject::BuildTriangleMesh()
{
	assert(m_loader != nullptr);
	assert(m_zoneDataLoaded);

	// Construct the partitioned triangle mesh
	m_chunkyMesh = std::make_unique<rcChunkyTriMesh>();

	if (!rcCreateChunkyTriMesh(
		m_loader->GetCollisionMesh().getVerts(),
		m_loader->GetCollisionMesh().getTris(),
		m_loader->GetCollisionMesh().getTriCount(),
		256,
		m_chunkyMesh.get()))
	{
		SPDLOG_ERROR("LoadGeometry: Failed to build chunky triangle mesh");
		return false;
	}

	return true;
}


void ZoneProject::ResetNavMesh()
{
	if (IsBusy() || !IsNavMeshReady())
		return;

	m_navMeshProj->ResetNavMesh();
}

bool ZoneProject::LoadNavMesh(bool allowMissing)
{
	if (IsBusy())
		return false;

	return m_navMeshProj->LoadNavMesh(allowMissing);
}

bool ZoneProject::SaveNavMesh()
{
	if (IsBusy() || !IsNavMeshReady())
		return false;

	return m_navMeshProj->SaveNavMesh();
}

bool ZoneProject::IsNavMeshReady() const
{
	return m_navMeshProj != nullptr
		&& m_navMeshProj->IsLoaded()
		&& !m_navMeshProj->IsBuilding();
}

bool ZoneProject::IsBusy() const
{
	return m_zoneLoading || (m_navMeshProj->IsLoaded() && m_navMeshProj->IsBuilding());
}

void ZoneProject::CancelTasks()
{
	if (m_loadZoneTask.valid())
	{
		m_loadZoneTask.cancel();
		m_loadZoneTask.get();
	}

	if (m_navMeshProj)
	{
		m_navMeshProj->CancelBuild(true);
	}
}

//========================================================================
// RaycastMesh
//========================================================================

static bool intersectSegmentTriangle(const glm::vec3& sp, const glm::vec3& sq,
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
{
	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 qp = sp - sq;

	// Compute triangle normal. Can be precalculated or cached if
	// intersecting multiple segments against the same triangle
	glm::vec3 norm = glm::cross(ab, ac);

	// Compute denominator d. If d <= 0, segment is parallel to or points
	// away from triangle, so exit early
	float d = glm::dot(qp, norm);
	if (d <= 0.0f) return false;

	// Compute intersection t value of pq with plane of triangle. A ray
	// intersects iff 0 <= t. Segment intersects iff 0 <= t <= 1. Delay
	// dividing by d until intersection has been found to pierce triangle
	glm::vec3 ap = sp - a;
	t = glm::dot(ap, norm);
	if (t < 0.0f) return false;
	if (t > d) return false; // For segment; exclude this code line for a ray test

	// Compute barycentric coordinate components and test if within bounds
	glm::vec3 e = glm::cross(qp, ap);
	float v = glm::dot(ac, e);
	if (v < 0.0f || v > d) return false;
	float w = -glm::dot(ab, e);
	if (w < 0.0f || v + w > d) return false;

	// Segment/ray intersects triangle. Perform delayed division
	t /= d;

	return true;
}

static bool isectSegAABB(const glm::vec3& sp, const glm::vec3& sq,
	const glm::vec3& amin, const glm::vec3& amax, float& tmin, float& tmax)
{
	static constexpr float EPS = 1e-6f;

	glm::vec3 d = sq - sp;
	tmin = 0.0;
	tmax = 1.0f;

	for (int i = 0; i < 3; i++)
	{
		if (fabsf(d[i]) < EPS)
		{
			if (sp[i] < amin[i] || sp[i] > amax[i])
				return false;
		}
		else
		{
			const float ood = 1.0f / d[i];
			float t1 = (amin[i] - sp[i]) * ood;
			float t2 = (amax[i] - sp[i]) * ood;
			if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
			tmin = std::max(t1, tmin);
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}

	return true;
}

bool ZoneProject::RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin)
{
	glm::vec3 dir = dest - src;

	// Prune hit ray.
	float btmin, btmax;
	if (!isectSegAABB(src, dest, m_meshBMin, m_meshBMax, btmin, btmax))
		return false;

	glm::vec2 pq = dest.xz() - src.xz();

	glm::vec2 p, q;
	p.x = src.x + pq.x * btmin;
	p.y = src.z + pq.y * btmin;
	q.x = src.x + pq.x * btmax;
	q.y = src.z + pq.y * btmax;

	int cid[512];
	const int ncid = rcGetChunksOverlappingSegment(m_chunkyMesh.get(), glm::value_ptr(p), glm::value_ptr(q), cid, 512);
	if (!ncid)
		return false;

	tMin = 1.0f;
	bool hit = false;
	const glm::vec3* verts = reinterpret_cast<const glm::vec3*>(m_loader->GetCollisionMesh().getVerts());

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = m_chunkyMesh->nodes[cid[i]];
		const int* tris = &m_chunkyMesh->tris[node.i * 3];
		const int ntris = node.n;

		for (int j = 0; j < ntris * 3; j += 3)
		{
			float t = 1;
			if (intersectSegmentTriangle(src, dest, verts[tris[j]], verts[tris[j + 1]], verts[tris[j + 2]], t))
			{
				tMin = std::min(t, tMin);
				hit = true;
			}
		}
	}

	return hit;
}

void ZoneProject::SetProgress(const ProgressState& progress)
{
	std::unique_lock lock(m_mutex);

	if (m_progress.Combine(progress))
	{
		//SPDLOG_INFO("SetProgressState display={} text={} progress={:.0f}%",
		//	m_progress.display.value_or(false),
		//	m_progress.text.value_or(std::string()),
		//	m_progress.value.value_or(0.0f) * 100);
	}
}

ProgressState ZoneProject::GetProgress() const
{
	std::unique_lock lock(m_mutex);
	return m_progress;
}
