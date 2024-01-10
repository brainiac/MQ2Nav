//
// NavMesh.h
//

#pragma once

#include "common/Enum.h"
#include "common/NavMeshData.h"
#include "common/NavModule.h"

#include <DetourNavMesh.h>
#include <mq/base/Signal.h>

#include <array>
#include <map>
#include <string>
#include <unordered_map>

class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;
class Context;
struct OffMeshConnectionBuffer;

namespace nav {
	class NavMeshFile;
}

enum struct PersistedDataFields : uint32_t
{
	BuildSettings          = 0x0001,
	MeshTiles              = 0x0002,
	ConvexVolumes          = 0x0004,
	AreaTypes              = 0x0008,
	Connections            = 0x0010,

	None                   = 0x0000,
	All                    = 0xffff,
};

constexpr bool has_bitwise_operations(PersistedDataFields) { return true; }

//============================================================================

class NavMesh : public NavModule
{
public:
	NavMesh(const std::string& folderName = std::string(),
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
	// navmesh queries

	std::vector<float> GetHeights(const glm::vec3& pos);
	float GetClosestHeight(const glm::vec3& pos);

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
		ZoneMismatch,
		OutOfMemory,
	};

	LoadResult LoadNavMeshFile();

	// Load a specific file into this instance
	LoadResult LoadNavMeshFile(const std::string& filename);

	// save the currently loaded mesh to a file
	bool SaveNavMeshFile();

	// save the currently loaded mesh to another file
	bool SaveNavMeshFile(const std::string& filename,
		NavMeshHeaderVersion version = NavMeshHeaderVersion::Latest);

	void SetNavMeshBounds(const glm::vec3& min, const glm::vec3& max);
	void GetNavMeshBounds(glm::vec3& min, glm::vec3& max);

	const glm::vec3& GetNavMeshBoundsMin() const { return m_boundsMin; }
	const glm::vec3& GetNavMeshBoundsMax() const { return m_boundsMax; }

	NavMeshConfig& GetNavMeshConfig() { return m_config; }
	const NavMeshConfig& GetNavMeshConfig() const { return m_config; }

	std::vector<dtTileRef> GetTileRefsForPoint(const glm::vec3& pos);

	int GetHeaderVersion() const { return static_cast<int>(m_version); }

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

	ConvexVolume* AddConvexVolume(std::unique_ptr<ConvexVolume> volume);

	ConvexVolume* AddConvexVolume(const std::vector<glm::vec3>& verts, const std::string& name,
		float minh, float maxh, uint8_t areaType);

	ConvexVolume* GetConvexVolumeById(uint32_t id);
	void DeleteConvexVolumeById(uint32_t id);

	std::vector<dtTileRef> GetTilesIntersectingConvexVolume(uint32_t volumeId);

	void MoveConvexVolumeToIndex(uint32_t id, size_t index);

	//------------------------------------------------------------------------
	// off-mesh connections

	size_t GetConnectionCount() const { return m_connections.size(); }

	const OffMeshConnection* GetConnection(size_t index) const { return m_connections[index].get(); }
	OffMeshConnection* GetConnection(size_t index) { return m_connections[index].get(); }

	const std::vector<std::unique_ptr<OffMeshConnection>>& GetConnections() const { return m_connections; }

	// Create a connection. Takes ownership of the provided connection.
	OffMeshConnection* AddConnection(
		std::unique_ptr<OffMeshConnection> connection);

	OffMeshConnection* GetConnectionById(uint32_t id);
	void DeleteConnectionById(uint32_t id);

	std::vector<dtTileRef> GetTilesIntersectingConnection(uint32_t connectionId);

	std::shared_ptr<OffMeshConnectionBuffer> CreateOffMeshConnectionBuffer() const;

	//------------------------------------------------------------------------
	// events

	mq::Signal<> OnNavMeshChanged;

private:
	LoadResult LoadMesh(const char* filename);

	bool SaveMeshV4(const char* filename);
	bool SaveMeshV5(const char* filename);

	bool SaveMesh(const char* filename, NavMeshHeaderVersion version = NavMeshHeaderVersion::Latest);

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
	NavMeshHeaderVersion m_version = {};

	std::shared_ptr<dtNavMesh> m_navMesh;
	std::shared_ptr<dtNavMeshQuery> m_navMeshQuery;
	glm::vec3 m_boundsMin = { 0, 0, 0 };
	glm::vec3 m_boundsMax = { 0, 0, 0 };
	NavMeshConfig m_config;

	// volumes
	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;
	std::unordered_map<uint32_t, ConvexVolume*> m_volumesById;
	uint32_t m_nextVolumeId = 1;

	// connections
	std::vector<std::unique_ptr<OffMeshConnection>> m_connections;
	std::unordered_map<uint32_t, OffMeshConnection*> m_connectionsById;
	uint32_t m_nextConnectionId = 1;

	// area types
	std::vector<const PolyAreaType*> m_polyAreaList;
	std::array<PolyAreaType, (int)PolyArea::Last + 1> m_polyAreas;
};

//----------------------------------------------------------------------------

struct dtNavMeshCreateParams;

// buffer used to store raw data used for tile creation
struct OffMeshConnectionBuffer
{
	OffMeshConnectionBuffer(
		const NavMesh* navMesh,
		const std::vector<std::unique_ptr<OffMeshConnection>>& connections);

	std::vector<std::pair<glm::vec3, glm::vec3>> offMeshConVerts;
	std::vector<float> offMeshConRads;
	std::vector<uint8_t> offMeshConDirs;
	std::vector<uint8_t> offMeshConAreas;
	std::vector<uint16_t> offMeshConFlags;
	std::vector<uint32_t> offMeshConId;
	size_t offMeshConCount = 0;

	void UpdateNavMeshCreateParams(dtNavMeshCreateParams& params);
};

//============================================================================
