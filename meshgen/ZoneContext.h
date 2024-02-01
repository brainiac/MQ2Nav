#pragma once

#include <string_view>

class ZoneResourceManager;

class ZoneContext
{
public:
	ZoneContext(std::string_view zoneShortName);
	~ZoneContext();

	const std::string& GetShortName() const { return m_zoneShortName; }
	const std::string& GetLongName() const { return m_zoneLongName; }

private:
	// short name of the currently loaded zone
	std::string m_zoneShortName;

	// long name of the currently loaded zone
	std::string m_zoneLongName;

	std::unique_ptr<ZoneResourceManager> m_resourceMgr;
};
