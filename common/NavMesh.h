//
// NavMesh.h
//

#pragma once

#include "common/Context.h"
#include "common/Enum.h"
#include "common/NavMeshData.h"
#include "common/NavModule.h"
#include "common/Signal.h"

#include <DetourNavMesh.h>

#include <array>
#include <map>
#include <string>
#include <unordered_map>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;
class Context;

namespace nav {
	class NavMeshFile;
}

enum struct PersistedDataFields : uint32_t
{
	BuildSettings          = 0x0001,
	MeshTiles              = 0x0002,
	ConvexVolumes          = 0x0004,
	AreaTypes              = 0x0008,

	None                   = 0x0000,
	All                    = 0xffff,
};

constexpr bool has_bitwise_operations(PersistedDataFields) { return true; }

//============================================================================

class NavMesh : public NavModule
{
public:
	NavMesh(Context* context, const std::string& folderName = std::string(),
		const std::string& zoneName = std::string());
	~NavMesh();

	NavMesh(const NavMesh&) = delete;
	NavMesh& operator=(const NavMesh&) = delete;

	// update the current zone. This will trigger a reload of the navmesh file if
	// the zone has changed, if autoload is enabled.
	void SetZoneName(const std::string& zoneShortName);
	std::string GetZoneName() const { return m_zoneName; }

	// sets the navmesh directory. Navmesh files will be loaded from this directory.
	std::string GetNavMeshDirectory() const { return m_navMeshDirectory; }
	void SetNavMeshDirectory(const std::string& dirname);

	// returns true if a navmesh is currently loaded.
	bool IsNavMeshLoaded() const { return m_navMesh != nullptr; }
	bool IsNavMeshLoadedFromDisk() const { return m_lastLoadResult == LoadResult::Success; };

	// get the current nav mesh
	std::shared_ptr<dtNavMesh> GetNavMesh() const { return m_navMesh; }

	// set the navmesh. This is primarily used for building a new mesh and should be
	// hidden away in the future to avoid the wierd usage requirements...
	void SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh, bool reset = true);

	// unload all existing data and clean up all state
	void ResetNavMesh();

	// get the nav mesh query object
	std::shared_ptr<dtNavMeshQuery> GetNavMeshQuery();

	// build area costs for filter
	void FillFilterAreaCosts(dtQueryFilter& filter);

	// returns the name of the file that the navmesh was loaded from
	std::string GetDataFileName() const { return m_dataFile; }

	bool ExportJson(const std::string& filename, PersistedDataFields fields);
	bool ImportJson(const std::string& filename, PersistedDataFields fields);

	//----------------------------------------------------------------------------
	// navmesh data

	// try to reload the navmesh for the current zone. Returns true if the navmesh
	// successfully loads.
	enum struct LoadResult {
		None,
		Success,
		MissingFile,
		Corrupt,
		VersionMismatch,
		ZoneMismatch
	};

	LoadResult LoadNavMeshFile();

	// save the currently loaded mesh to a file
	bool SaveNavMeshFile();

	void SetNavMeshBounds(const glm::vec3& min, const glm::vec3& max);
	void GetNavMeshBounds(glm::vec3& min, glm::vec3& max);

	const glm::vec3& GetNavMeshBoundsMin() const { return m_boundsMin; }
	const glm::vec3& GetNavMeshBoundsMax() const { return m_boundsMax; }

	NavMeshConfig& GetNavMeshConfig() { return m_config; }
	const NavMeshConfig& GetNavMeshConfig() const { return m_config; }

	//------------------------------------------------------------------------
	// area types

	using PolyAreaList = std::vector<const PolyAreaType*>;
	PolyAreaList& GetPolyAreas() { return m_polyAreaList; }
	const PolyAreaList& GetPolyAreas() const { return m_polyAreaList; }

	inline const PolyAreaType& GetPolyArea(uint8_t areaType) const
	{
		return m_polyAreas[areaType];
	}

	void UpdateArea(const PolyAreaType& area);
	void RemoveUserDefinedArea(uint8_t areaId);

	// returns 0 if there are no available areas
	uint8_t GetFirstUnusedUserDefinedArea() const;

	//----------------------------------------------------------------------------
	// convex values / marked areas

	size_t GetConvexVolumeCount() const { return m_volumes.size(); }

	const ConvexVolume* GetConvexVolume(size_t index) const { return m_volumes[index].get(); }

	ConvexVolume* GetConvexVolume(size_t index) { return m_volumes[index].get(); }

	const std::vector<std::unique_ptr<ConvexVolume>>& GetConvexVolumes() const { return m_volumes; }

	ConvexVolume* AddConvexVolume(const std::vector<glm::vec3>& verts, const std::string& name,
		float minh, float maxh, uint8_t areaType);

	ConvexVolume* GetConvexVolumeById(uint32_t id);
	void DeleteConvexVolumeById(uint32_t id);

	std::vector<dtTileRef> GetTilesIntersectingConvexVolume(uint32_t volumeId);

	void MoveConvexVolumeToIndex(uint32_t id, size_t index);

	//------------------------------------------------------------------------
	// events

	Signal<> OnNavMeshChanged;
	
private:
	LoadResult LoadMesh(const char* filename);
	bool SaveMesh(const char* filename);

	void UpdateDataFile();
	void InitializeAreas();

	void ResetSavedData(PersistedDataFields fields = PersistedDataFields::All);

	void LoadFromProto(const nav::NavMeshFile& proto, PersistedDataFields fields);
	void SaveToProto(nav::NavMeshFile& proto, PersistedDataFields fields);

private:
	Context* m_ctx;
	std::string m_navMeshDirectory;
	std::string m_zoneName;
	std::string m_dataFile;
	LoadResult m_lastLoadResult = LoadResult::None;

	std::shared_ptr<dtNavMesh> m_navMesh;
	std::shared_ptr<dtNavMeshQuery> m_navMeshQuery;
	glm::vec3 m_boundsMin, m_boundsMax;
	NavMeshConfig m_config;

	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;
	std::unordered_map<uint32_t, ConvexVolume*> m_volumesById;
	uint32_t m_nextVolumeId = 1;

	std::vector<const PolyAreaType*> m_polyAreaList;
	std::array<PolyAreaType, (int)PolyArea::Last + 1> m_polyAreas;
};

//============================================================================
