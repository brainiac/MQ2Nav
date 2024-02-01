
#pragma once

class ZoneResourceManager
{
public:
	ZoneResourceManager(const std::string& shortName);
	~ZoneResourceManager();

private:
	std::string m_shortName;
};
