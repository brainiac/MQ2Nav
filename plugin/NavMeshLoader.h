//
// MeshLoader.h
//

#pragma once

#include "common/NavMesh.h"
#include "common/NavModule.h"
#include "common/Signal.h"

#include <mq/Plugin.h>

#include <chrono>
#include <string>
#include <memory>

class dtNavMesh;
class MQ2NavigationPlugin;

class NavMeshLoader : public NavModule
{
public:
	NavMeshLoader(NavMesh* mesh);

	virtual void Initialize() override {}
	virtual void Shutdown() override {}

	// will do actions on specific intervals
	virtual void OnPulse() override;
	virtual void SetGameState(int GameState) override;

	// update the current zone. This will trigger a reload of the navmesh file if
	// the zone has changed, if autoload is enabled
	virtual void SetZoneId(int zoneId) override;
	int GetZoneId() const { return m_zoneId; }

	// turns autoload on or off. A navmesh will be loaded when zoning if autoload
	// is true.
	void SetAutoLoad(bool autoLoad);
	bool GetAutoLoad() const { return m_autoLoad; }

	// turns autoreload on or off. A navmesh will be reloaded when the file changes
	// if this is true.
	void SetAutoReload(bool autoReload);
	bool GetAutoReload() const { return m_autoReload; }

	// try to reload the navmesh for the current zone. Returns true if the
	// navmesh successfully loads
	bool LoadNavMesh();

private:
	NavMesh* m_navMesh = nullptr;

	std::string m_zoneShortName;
	int m_zoneId = -1;

	bool m_autoLoad = true;

	// auto reloading
	bool m_autoReload = true;
	FILETIME m_fileTime = { 0, 0 };

	using clock = std::chrono::high_resolution_clock;
	clock::time_point m_lastUpdate = clock::now();
};
