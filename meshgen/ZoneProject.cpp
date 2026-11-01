
#include "pch.h"
#include "ZoneProject.h"

#include "meshgen/Application.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/Editor.h"
#include "meshgen/NavMeshBuilder.h"
#include "meshgen/Scene.h"
#include "meshgen/ZoneCollisionMesh.h"
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

	m_scene = std::make_shared<Scene>(m_zoneShortName);

	m_renderManager->SetRegistry(&m_scene->GetRegistry());

	std::string eqPath = g_config.GetEverquestPath();
	std::string outputPath = g_config.GetOutputPath();

	m_resourceMgr = std::make_unique<ZoneResourceManager>(m_zoneShortName, eqPath, outputPath, m_scene);

	m_collisionMesh = std::make_shared<ZoneCollisionMesh>();

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

	// Should this go in here or in render?
	if (m_navMeshProj->GetNavMesh()->SendEventIfDirty())
		m_renderManager->GetNavMeshRender()->SetDirty();
}

void ZoneProject::OnShutdown()
{
	CancelTasks();
	m_renderManager->DestroyObjects();

	m_scene->Clear();
}

void ZoneProject::Render()
{
	m_renderManager->Render();
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
			auto phase = TaskPhase::BuildCollisionMesh;

			// Start the progress
			sharedThis->SetProgress({
				.phase = phase,
				.display = true,
				.text = fmt::format("Generating collision mesh..."),
				.value = 0.33f
			});

			// Do the work
			if (sharedThis->BuildCollisionMesh())
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
				return 1;

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
	// Get the long name from the application config
	m_zoneLongName = g_config.GetLongNameForShortName(m_zoneShortName);
	m_displayName = fmt::format("{} ({})", m_zoneLongName, m_zoneShortName);

	m_resourceMgr->SetLoadSocials(false);

	if (!m_resourceMgr->Load())
	{
		SPDLOG_ERROR("LoadZone: Failed to load '{}'", m_zoneShortName);
		return false;
	}

	m_resourceMgr->BuildScene(*m_scene);

	m_renderManager->SetConstantAmbientColor(
		m_resourceMgr->GetResourceManager()->GetConstantAmbientColor());

	m_zoneDataLoaded = true;
	return true;
}

bool ZoneProject::RebuildCollisionMesh()
{
	if (IsBusy() || !m_zoneDataLoaded)
		return false;

	if (!BuildCollisionMesh())
		return false;

	m_collisionMeshDirty = false;
	return true;
}

bool ZoneProject::BuildCollisionMesh()
{
	assert(m_zoneDataLoaded);

	m_collisionMesh->clear();

	if (g_config.GetUseMaxExtents())
	{
		auto iter = MaxZoneExtents.find(m_zoneShortName);
		if (iter != MaxZoneExtents.end())
		{
			m_collisionMesh->setMaxExtents(iter->second);
		}
	}

	if (m_resourceMgr->BuildCollisionMesh(*m_collisionMesh))
	{
		AddMeshPatchesToCollisionMesh();

		// The resource manager finalized the mesh before returning, which was before the
		// patches went in. Without this their triangles sit in the vertex list but never
		// reach the chunky tri mesh, which is the only thing the builder reads - so the
		// patches would quietly do nothing on every full rebuild.
		m_collisionMesh->finalize();

		m_meshBMin = m_collisionMesh->m_boundsMin;
		m_meshBMax = m_collisionMesh->m_boundsMax;

		return true;
	}

	return false;
}

// Mesh patches are user authored geometry, added after the zone's own so that a patch can
// lie over the surface it is repairing. They are ordinary triangles from here on: Recast
// voxelizes them like anything else, which is the point - the generator gets a clean
// surface to work from rather than having its output corrected afterwards.
// Triangulated as a fan, so the ring has to be convex to come out right. The tool only
// produces quads; a ring built by hand is the user's responsibility.
//
// Winding decides whether a triangle is walkable: rcMarkWalkableTriangles takes the face
// normal and rejects anything not pointing up. A patch is authored by clicking, so the ring
// comes out either way round depending on which side was clicked first - hence the flip
// rather than trusting the caller.
static void AddUpwardTriangle(ZoneCollisionMesh& collisionMesh,
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	if (glm::cross(b - a, c - a).y < 0.0f)
		collisionMesh.addTriangle(a, c, b);
	else
		collisionMesh.addTriangle(a, b, c);
}

// Box patches are solid: the top face is what gets walked on, and the sides and bottom are
// there so the box reads as an obstacle from every other direction rather than a surface
// that can be passed through from below.
static void AddBoxTriangles(ZoneCollisionMesh& collisionMesh, const glm::vec3& c0, const glm::vec3& c1)
{
	const glm::vec3 lo = glm::min(c0, c1);
	const glm::vec3 hi = glm::max(c0, c1);

	const glm::vec3 v[8] = {
		{ lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z },
		{ lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z },
	};

	// Each face as two triangles, wound outwards. Only the top face matters for walkability,
	// but the others still have to be rasterized or the box would be hollow to the voxelizer.
	static const int faces[12][3] = {
		{ 4, 5, 6 }, { 4, 6, 7 },   // top      (+y)
		{ 0, 2, 1 }, { 0, 3, 2 },   // bottom   (-y)
		{ 0, 1, 5 }, { 0, 5, 4 },   // -z
		{ 3, 7, 6 }, { 3, 6, 2 },   // +z
		{ 0, 4, 7 }, { 0, 7, 3 },   // -x
		{ 1, 2, 6 }, { 1, 6, 5 },   // +x
	};

	for (const auto& f : faces)
		collisionMesh.addTriangle(v[f[0]], v[f[1]], v[f[2]]);
}

// Strips are pairs of vertices along a run, each consecutive pair of pairs making a quad.
// Every quad is triangulated on its own, so unlike a fan the run does not have to be planar.
static void AddStripTriangles(ZoneCollisionMesh& collisionMesh, const std::vector<glm::vec3>& verts)
{
	for (size_t i = 0; i + 3 < verts.size(); i += 2)
	{
		AddUpwardTriangle(collisionMesh, verts[i], verts[i + 1], verts[i + 3]);
		AddUpwardTriangle(collisionMesh, verts[i], verts[i + 3], verts[i + 2]);
	}
}

static void AddPatchTriangles(ZoneCollisionMesh& collisionMesh, const MeshPatch& patch)
{
	switch (patch.type)
	{
	case MeshPatchType::Box:
		if (patch.verts.size() >= 2)
			AddBoxTriangles(collisionMesh, patch.verts[0], patch.verts[1]);
		break;

	case MeshPatchType::Strip:
		AddStripTriangles(collisionMesh, patch.verts);
		break;

	default:
		for (size_t i = 1; i + 1 < patch.verts.size(); ++i)
			AddUpwardTriangle(collisionMesh, patch.verts[0], patch.verts[i], patch.verts[i + 1]);
		break;
	}
}

// A box needs only its two corners, a strip needs at least two pairs, a surface a ring.
static bool IsPatchUsable(const MeshPatch& patch)
{
	switch (patch.type)
	{
	case MeshPatchType::Box:   return patch.verts.size() >= 2;
	case MeshPatchType::Strip: return patch.verts.size() >= 4 && patch.verts.size() % 2 == 0;
	default:                   return patch.verts.size() >= 3;
	}
}

void ZoneProject::AddMeshPatchesToCollisionMesh()
{
	auto navMesh = GetNavMesh();
	if (!navMesh)
		return;

	for (const auto& patch : navMesh->GetMeshPatches())
	{
		if (IsPatchUsable(*patch))
			AddPatchTriangles(*m_collisionMesh, *patch);
	}
}

// Adds a single patch to the collision mesh already in memory. Rebuilding tiles regenerates
// them from that mesh rather than from the zone data, so a patch that is only recorded in
// the navmesh has no effect until its geometry is in there too - and regenerating the whole
// collision mesh for one patch takes seconds on a large zone.
bool ZoneProject::AddMeshPatchGeometry(const MeshPatch& patch)
{
	if (!m_collisionMesh || !IsPatchUsable(patch))
		return false;

	AddPatchTriangles(*m_collisionMesh, patch);

	// Rebuilds the bounds and the chunky triangle mesh, which is what the navmesh builder
	// actually reads.
	return m_collisionMesh->finalize();
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

bool ZoneProject::RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin)
{
	return m_collisionMesh && m_collisionMesh->RaycastMesh(src, dest, tMin);
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
