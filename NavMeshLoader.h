//
// MeshLoader.h
//

#pragma once

#include "MQ2Plugin.h"
#include "Signal.h"

#include <chrono>
#include <string>
#include <memory>

class dtNavMesh;
class MQ2NavigationPlugin;

class NavMeshLoader
{
public:
	NavMeshLoader() {}

	// called from OnPulse, will do actions on specific intervals
	void Process();

	// update the current zone. This will trigger a reload of the navmesh file if
	// the zone has changed, if autoload is enabled
	void SetZoneId(DWORD zoneId);
	DWORD GetZoneId() const { return m_zoneId; }

	// turns autoload on or off. A navmesh will be loaded when zoning if autoload
	// is true.
	void SetAutoLoad(bool autoLoad);
	bool GetAutoLoad() const { return m_autoLoad; }

	// turns autoreload on or off. A navmesh will be reloaded when the file changes
	// if this is true.
	void SetAutoReload(bool autoReload);
	bool GetAutoReload() const { return m_autoReload; }

	// returns if a navmesh is currently loaded or not.
	bool IsNavMeshLoaded() const { return m_mesh != nullptr; }

	// returns the name of the file that the navmesh was loaded from.
	std::string GetDataFileName() const { return m_loadedDataFile; }
	
	// try to reload the navmesh for the current zone. Returns true if the
	// navmesh successfully loads
	bool LoadNavMesh();

	// unload all existing data and clean up all state
	void Reset();

	// get the currently loaded navmesh
	inline dtNavMesh* GetNavMesh() const { return m_mesh.get(); }

	Signal<dtNavMesh*> OnNavMeshChanged;

private:
	enum LoadResult { SUCCESS, CORRUPT, VERSION_MISMATCH };

	LoadResult LoadZoneMeshData(const std::shared_ptr<char>& data, DWORD length);

	std::string GetMeshDirectory() const;

private:
	std::unique_ptr<dtNavMesh> m_mesh;

	std::string m_zoneShortName;
	DWORD m_zoneId = (DWORD)-1;

	bool m_autoLoad = true;
	std::string m_loadedDataFile;
	int m_loadedTiles = 0;

	// auto reloading
	bool m_autoReload = true;
	FILETIME m_fileTime = { 0, 0 };

	typedef std::chrono::high_resolution_clock clock;
	clock::time_point m_lastUpdate = clock::now();
};
