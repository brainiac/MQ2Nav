#pragma once

#include <string_view>
#include <vector>

struct ConvexVolume;
class MapGeometryLoader;
class RecastContext;
class ZoneResourceManager;
struct rcChunkyTriMesh;

class ZoneContext
{
public:
	ZoneContext(std::string_view zoneShortName);
	~ZoneContext();

	void SetAutoloadMesh(bool autoload) { m_autoLoadMesh = autoload; }
	bool GetAutoloadMesh() const { return m_autoLoadMesh; }

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

	// Utilities
	bool RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin);

	//
	// Load/Build routines
	//

	// Load zone data from EQ folder
	bool LoadZone();

	// Generate chunky triangle mesh
	bool BuildTriangleMesh();

private:
	std::unique_ptr<ZoneResourceManager> m_resourceMgr;
	std::unique_ptr<RecastContext>       m_rcContext;
	std::unique_ptr<MapGeometryLoader>   m_loader;
	std::unique_ptr<rcChunkyTriMesh>     m_chunkyMesh;
	std::string m_zoneShortName;         // short name of the currently loaded zone
	std::string m_zoneLongName;          // long name of the currently loaded zone
	std::string m_displayName;           // short/long name combined in display string
	glm::vec3   m_meshBMin, m_meshBMax;  // bounds of the currently loaded zone
	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;
	bool m_zoneLoaded = false;
	bool m_autoLoadMesh = false;
};
