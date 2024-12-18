#pragma once

#include <taskflow/taskflow.hpp>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

class Editor;
class NavMesh;
struct ConvexVolume;
class MapGeometryLoader;
class RecastContext;
class ZoneResourceManager;
struct rcChunkyTriMesh;

struct ProgressState
{
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

		return changed;
	}
};

enum class LoadingPhase
{
	LoadZone,
	BuildTriangleMesh,
	LoadNavMesh,
	BuildNavMesh,
};

struct ResultState
{
	LoadingPhase phase;
	bool success;
	std::string message;
};

class ZoneContext : public std::enable_shared_from_this<ZoneContext>
{
public:
	ZoneContext(Editor* editor, std::string_view zoneShortName);
	~ZoneContext();

	// Accessors
	const std::string& GetShortName() const { return m_zoneShortName; }
	const std::string& GetLongName() const { return m_zoneLongName; }
	const std::string& GetDisplayName() const { return m_displayName; }
	bool IsZoneLoaded() const { return m_zoneLoaded; }

	const glm::vec3& GetMeshBoundsMin() const { return m_meshBMin; }
	const glm::vec3& GetMeshBoundsMax() const { return m_meshBMax; }

	MapGeometryLoader* GetMeshLoader() { return m_loader.get(); }
	const MapGeometryLoader* GetMeshLoader() const { return m_loader.get(); }
	const rcChunkyTriMesh* GetChunkyMesh() { return m_chunkyMesh.get(); }
	std::shared_ptr<NavMesh> GetNavMesh() { return m_navMesh; }

	RecastContext& GetRcContext() { return *m_rcContext; }
	const RecastContext& GetRcContext() const { return *m_rcContext; }

	// Navmesh related
	bool IsNavMeshLoaded() const;// m_navMesh->IsNavMeshLoaded
	bool IsBuildingNavMesh() const;

	void ResetNavMesh(); // m_navMesh->ResetNavMesh

	// Utilities
	bool RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin);

	//
	// Load/Build routines
	//

	using ZoneContextCallback = std::function<void(
		LoadingPhase phase,
		bool result
	)>;

	tf::Taskflow BuildInitialTaskflow(
		bool loadNavMesh,
		ZoneContextCallback&& callback);

	// Load zone data from EQ folder
	bool LoadZone();

	// Load navmesh for this zone
	bool LoadNavMesh();

	// Save navmesh for this zone
	void SaveNavMesh();

	// Generate chunky triangle mesh
	bool BuildTriangleMesh();

	void SetProgress(const ProgressState& progress);
	ProgressState GetProgress() const;

	void SetResultState(const ResultState& state);
	ResultState GetResultState() const;

private:
	mutable std::mutex m_mutex;

	Editor*                              m_editor;
	std::unique_ptr<ZoneResourceManager> m_resourceMgr;
	std::unique_ptr<RecastContext>       m_rcContext;
	std::unique_ptr<MapGeometryLoader>   m_loader;
	std::unique_ptr<rcChunkyTriMesh>     m_chunkyMesh;
	std::shared_ptr<NavMesh>             m_navMesh;
	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;

	std::string m_zoneShortName;         // short name of the currently loaded zone
	std::string m_zoneLongName;          // long name of the currently loaded zone
	std::string m_displayName;           // short/long name combined in display string
	glm::vec3   m_meshBMin, m_meshBMax;  // bounds of the currently loaded zone
	bool        m_zoneLoaded = false;
	std::atomic_bool m_loading = false;
	std::atomic_bool m_buildingNavmesh = false;
	ProgressState m_progress;
	ResultState m_resultState;
};
