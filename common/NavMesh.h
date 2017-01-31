//
// NavMesh.h
//

#pragma once

#include "NavMeshData.h"
#include "Signal.h"

#include <string>

class dtNavMesh;
class dtNavMeshQuery;

namespace nav {


class NavMesh
{
public:
	NavMesh();
	~NavMesh();

	// update the current zone. This will trigger a reload of the navmesh file if
	// the zone has changed, if autoload is enabled.
	void SetZoneName(const std::string& zoneShortName);
	std::string GetZoneName() const { return m_zoneName; }

	// sets the navmesh directory. Navmesh files will be loaded from this directory.
	void SetNavMeshDirectory(const std::string& folderName);
	std::string GetNavMeshDirectory() const { return m_navMeshDirectory; }

	// turns autoload on or off. A navmesh will be loaded when the zone changes if
	// autoload is set to true.
	void SetAutoLoad(bool autoLoad);
	bool GetAutoLoad() const { return m_autoLoad; }

	// turns autoreload on or off. A navmesh will be reloaded when the file changes
	// if this is set to true.
	void SetAutoReload(bool autoReload);
	bool GetAutoReload() const { return m_autoReload; }

	// returns true if a navmesh is currently loaded.
	bool IsNavMeshLoaded() const { return m_navMesh != nullptr; }

	// get the current nav mesh
	std::shared_ptr<dtNavMesh> GetNavMesh() const { return m_navMesh; }

	// get the nav mesh query object

	// returns the name of the file that the navmesh was loaded from
	std::string GetDataFileName() const { return m_loadedDataFile; }

	// try to reload the navmesh for the current zone. Returns true if the navmesh
	// successfully loads.
	bool LoadNavMeshFromFile();

	// unload all existing data and clean up all state
	void ResetNavMesh();

	//------------------------------------------------------------------------
	// marked areas and volumes

	// TODO

	//------------------------------------------------------------------------
	// events

	// TODO
	

private:
	enum struct LoadResult { Success, Corrupt, VersionMismatch };
	LoadResult LoadMeshFromFile();

private:
	std::string m_navMeshDirectory;
	std::string m_zoneName;
	bool m_autoLoad = false;
	bool m_autoReload = false;

	std::shared_ptr<dtNavMesh> m_navMesh;
	std::shared_ptr<dtNavMeshQuery> m_navMeshQuery;
	std::string m_loadedDataFile;
};


} // namespace nav
