#pragma once

#include "common/NavMeshData.h"

#include <taskflow/taskflow.hpp>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

class dtNavMesh;
class Editor;
class NavMesh;
class NavMeshBuilder;
class RecastContext;
class Scene;
class ZoneCollisionMesh;
class ZoneProject;
class ZoneRenderManager;
class ZoneResourceManager;
struct ConvexVolume;
struct rcChunkyTriMesh;

enum class TaskPhase
{
	LoadZoneData,
	BuildCollisionMesh,
	LoadNavMesh,
	Done,
};

struct ProgressState
{
	std::optional<TaskPhase> phase;
	std::optional<bool> display;
	std::optional<std::string> text;
	std::optional<float> value;

	bool Combine(const ProgressState& other)
	{
		bool changed = false;
		if (other.display.has_value() && other.display != display)
		{
			display = other.display;
			changed = true;
		}
		if (other.text.has_value() && other.text != text)
		{
			text = other.text;
			changed = true;
		}
		if (other.value.has_value() && other.value != value)
		{
			value = other.value;
			changed = true;
		}

		if (other.phase.has_value() && other.phase != phase)
		{
			phase = other.phase;
			changed = true;
		}

		return changed;
	}
};

struct ResultState
{
	TaskPhase phase;
	bool failed;
	std::string message;
};

class NavMeshProject : public std::enable_shared_from_this<NavMeshProject>
{
	friend ZoneProject;
	friend NavMeshBuilder;

public:
	NavMeshProject(Editor* editor, std::string_view zoneShortName);
	~NavMeshProject();

	void InitWithProject(const std::shared_ptr<ZoneProject>& zoneProject);

	void OnUpdate(float timeStep);

	std::shared_ptr<dtNavMesh> GetDetourNavMesh() const;
	std::shared_ptr<NavMesh> GetNavMesh() const { return m_navMesh; }
	std::shared_ptr<ZoneProject> GetZoneProject() const { return m_zoneProj.lock(); }
	std::shared_ptr<NavMeshBuilder> GetBuilder() const { return m_builder; }

	bool IsLoaded() const;
	bool IsBuilding() const;
	bool IsReady() const { return IsLoaded() && !IsBuilding(); }
	void CancelBuild(bool wait = true);

	void ResetNavMesh();
	bool LoadNavMesh(bool allowMissing = false);
	bool SaveNavMesh();

	// this is our mutable config. It will be saved to the mesh when we build it.
	NavMeshConfig& GetNavMeshConfig() { return m_navMeshConfig; }
	const NavMeshConfig& GetNavMeshConfig() const { return m_navMeshConfig; }
	void ResetNavMeshConfig();

	void RemoveTile(const glm::vec3& pos, int layer = 0);
	void RemoveTileAt(int tx, int ty, int layer = 0);
	void RemoveAllTiles();

private:
	mutable std::mutex m_mutex;
	using ConvexVolumePtr = std::unique_ptr<ConvexVolume>;

	Editor*                         m_editor;
	std::weak_ptr<ZoneProject>      m_zoneProj;
	std::shared_ptr<NavMeshBuilder> m_builder;
	std::shared_ptr<NavMesh>        m_navMesh;
	std::string                     m_shortName;
	std::vector<ConvexVolumePtr>    m_volumes;
	NavMeshConfig                   m_navMeshConfig;
};

class ZoneProject : public std::enable_shared_from_this<ZoneProject>
{
public:
	ZoneProject(Editor* editor, const std::string& name);
	~ZoneProject();

	void OnUpdate(float timeStep);
	void OnShutdown();

	std::shared_ptr<ZoneRenderManager> GetRenderManager() const { return m_renderManager; }
	ZoneResourceManager* GetResourceManager() const { return m_resourceMgr.get(); }

	std::shared_ptr<ZoneCollisionMesh> GetCollisionMesh() const { return m_collisionMesh; }

	// Zone
	const std::string& GetShortName() const { return m_zoneShortName; }
	const std::string& GetLongName() const { return m_zoneLongName; }
	const std::string& GetDisplayName() const { return m_displayName; }
	bool IsZoneLoading() const { return m_zoneLoading; }
	bool IsZoneLoaded() const { return m_zoneLoaded; }
	bool IsBusy() const;
	const glm::vec3& GetMeshBoundsMin() const { return m_meshBMin; }
	const glm::vec3& GetMeshBoundsMax() const { return m_meshBMax; }

	void LoadZone(bool loadNavMesh);

	// NavMesh
	std::shared_ptr<NavMeshProject> GetNavMeshProject() const { return m_navMeshProj; }
	std::shared_ptr<NavMesh> GetNavMesh() const { return m_navMeshProj ? m_navMeshProj->GetNavMesh() : nullptr; }
	bool IsNavMeshReady() const;

	void ResetNavMesh();
	bool LoadNavMesh(bool allowMissing = false);
	bool SaveNavMesh();

	void CancelTasks();
	void SetProgress(const ProgressState& progress);
	ProgressState GetProgress() const;

	// Utilities
	bool RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin);

private:
	bool LoadZoneData();  // Load zone data from EQ folder
	bool BuildTriangleMesh(); // Generate chunky triangle mesh

	void OnLoadZoneComplete(bool success, const ResultState& result);

	using ZoneContextCallback = std::function<void(bool success, ResultState result)>;
	tf::Taskflow BuildLoadZoneTaskflow(bool loadNavMesh, ZoneContextCallback callback);

private:
	mutable std::mutex m_mutex;
	Editor* m_editor;

	std::string      m_zoneShortName;         // short name of the currently loaded zone
	std::string      m_zoneLongName;          // long name of the currently loaded zone
	std::string      m_displayName;           // short/long name combined in display string
	std::atomic_bool m_zoneLoading = false;
	std::atomic_bool m_zoneDataLoaded = false; // zone data has been loaded
	std::atomic_bool m_zoneLoaded = false;    // zone load has completed
	std::atomic_bool m_busy = false;
	glm::vec3        m_meshBMin, m_meshBMax;  // bounds of the currently loaded zone
	ProgressState    m_progress;
	tf::Future<void> m_loadZoneTask;

	std::shared_ptr<Scene>               m_scene;
	std::shared_ptr<ZoneRenderManager>   m_renderManager;
	std::unique_ptr<ZoneResourceManager> m_resourceMgr;
	std::shared_ptr<ZoneCollisionMesh>   m_collisionMesh;

	// The currently selected nav mesh and all of its state.
	std::shared_ptr<NavMeshProject>      m_navMeshProj;
};
