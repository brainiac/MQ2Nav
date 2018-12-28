//
// MapAPI.cpp
//

#include "MapAPI.h"

#include <MQ2Plugin.h>
#include <glm/glm.hpp>

namespace nav {

//============================================================================

static PMQPLUGIN FindMQ2MapPlugin()
{
	PMQPLUGIN pPlugin = pPlugins;
	while (pPlugin)
	{
		if (!_stricmp("MQ2Map", pPlugin->szFilename))
		{
			return pPlugin;
		}

		pPlugin = pPlugin->pNext;
	}

	return nullptr;
}

static void DeleteLineSegment(PMAPLINE pLine)
{
	using DeleteLineFunc = void(__cdecl*)(MAPLINE*);

	if (PMQPLUGIN pPlugin = FindMQ2MapPlugin())
	{
		if (DeleteLineFunc deleteLine = (DeleteLineFunc)GetProcAddress(pPlugin->hModule, "MQ2MapDeleteLine"))
		{
			deleteLine(pLine);
		}
	}
}

std::shared_ptr<MAPLINE> nav::MapLine::CreateSegment()
{
	using AddMapLineFunc = PMAPLINE(__cdecl*)();

	if (PMQPLUGIN pPlugin = FindMQ2MapPlugin())
	{
		if (AddMapLineFunc addMapLine = (AddMapLineFunc)GetProcAddress(pPlugin->hModule, "MQ2MapAddLine"))
		{
			if (MAPLINE* mapLine = addMapLine())
			{
				std::shared_ptr<MAPLINE> ptr = std::shared_ptr<MAPLINE>(
					mapLine, [](PMAPLINE line) { DeleteLineSegment(line); });
				return ptr;
			}
		}
	}

	return nullptr;
}

MapLine::MapLine()
{
}

MapLine::~MapLine()
{
}

void MapLine::SetColor(uint32_t argbcolor)
{
	if (m_color != argbcolor)
	{
		m_color = argbcolor;

		for (auto& line : m_lineSegments)
			line->Color.ARGB = argbcolor;
	}
}

void MapLine::SetLayer(int layer)
{
	if (layer >= 0 && layer <= 3 && layer != m_layer)
	{
		m_layer = layer;

		for (auto& line : m_lineSegments)
			line->Layer = layer;
	}
}

bool MapLine::AddPoint(const glm::vec3& point)
{
	if (!m_hasLastPos)
	{
		m_lastPos = point;
		m_hasLastPos = true;
		return true;
	}

	if (auto segment = CreateSegment())
	{
		glm::vec3 p1 = m_lastPos.xzy;
		glm::vec3 p2 = point.xzy;

		segment->Color.ARGB = m_color;
		segment->Layer = m_layer;
		segment->Start = { -p1.x, -p1.y, p1.z };
		segment->End = { -p2.x, -p2.y, p2.z };
		m_lastPos = point;
		m_lineSegments.push_back(segment);

		return true;
	}

	return false;
}

void MapLine::Clear()
{
	m_lineSegments.clear();
	m_hasLastPos = false;
}

//============================================================================

} // namespace nav
