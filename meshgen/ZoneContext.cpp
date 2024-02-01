
#include "pch.h"
#include "ZoneContext.h"

#include "meshgen/ApplicationConfig.h"
#include "meshgen/ZoneResourceManager.h"

ZoneContext::ZoneContext(std::string_view zoneShortName)
	: m_zoneShortName(zoneShortName)
{
	// Get the long name from the application config
	m_zoneLongName = g_config.GetLongNameForShortName(m_zoneShortName);

	m_resourceMgr = std::make_unique<ZoneResourceManager>(m_zoneShortName);
}

ZoneContext::~ZoneContext()
{
}
