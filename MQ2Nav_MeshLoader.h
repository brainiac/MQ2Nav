//
// MQ2Nav_MeshLoader.h
//

#pragma once

#include <string>

class MQ2NavigationPlugin;

class MeshLoader
{
	MeshLoader(MQ2NavigationPlugin* plugin)
		: m_plugin(plugin)
	{}

	void SetZoneId(int zoneId);

	void ReloadMesh();

private:
	MQ2NavigationPlugin* m_plugin;

	std::string m_zoneShortName;
	int m_zoneId;
};