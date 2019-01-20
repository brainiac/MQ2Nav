//
// MapAPI.cpp
//

#include "pch.h"
#include "MapAPI.h"

#include <MQ2Plugin.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

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

//============================================================================

std::shared_ptr<MAPLINE> nav::MapItem::CreateSegment()
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

void MapItem::SetColor(uint32_t argbcolor)
{
	if (m_color != argbcolor)
	{
		m_color = argbcolor;

		for (auto& line : m_lineSegments)
			line->Color.ARGB = argbcolor;
	}
}

void MapItem::SetLayer(int layer)
{
	if (layer >= 0 && layer <= 3 && layer != m_layer)
	{
		m_layer = layer;

		for (auto& line : m_lineSegments)
			line->Layer = layer;
	}
}

void MapItem::Clear()
{
	m_lineSegments.clear();
}

//----------------------------------------------------------------------------

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
	MapItem::Clear();
	m_hasLastPos = false;
}

//----------------------------------------------------------------------------

const int MAPCIRCLE_ANGLE_SIZE = 10;

MapCircle::MapCircle(const glm::vec3& position, float radius)
	: m_position(position)
	, m_radius(radius)
{
	UpdateCircle();
}

void MapCircle::SetCircle(const glm::vec3& position, float radius)
{
	if (m_position != position
		|| m_radius != radius
		|| !m_created)
	{
		m_position = position;
		m_radius = radius;

		UpdateCircle();
	}
}

void MapCircle::Clear()
{
	MapItem::Clear();
	m_created = false;
}

void MapCircle::UpdateCircle()
{
	int count = 360 / MAPCIRCLE_ANGLE_SIZE;
	float angle = 0;
	m_created = true;

	if (m_lineSegments.size() != count)
	{
		m_lineSegments.clear();

		for (int i = 0; i < count; ++i)
			m_lineSegments.push_back(CreateSegment());
	}

	for (int i = 0; i < count; ++i, angle += MAPCIRCLE_ANGLE_SIZE)
	{
		std::shared_ptr<MAPLINE> segment = m_lineSegments[i];

		segment->Color.ARGB = m_color;
		segment->Layer = m_layer;

		segment->Start = {
			-m_position.x + (glm::cos(glm::radians(angle)) * m_radius),
			-m_position.y + (glm::sin(glm::radians(angle)) * m_radius),
			m_position.z
		};
		segment->End = {
			-m_position.x + (glm::cos(glm::radians(angle + MAPCIRCLE_ANGLE_SIZE)) * m_radius),
			-m_position.y + (glm::sin(glm::radians(angle + MAPCIRCLE_ANGLE_SIZE)) * m_radius),
			m_position.z
		};

		m_lineSegments.push_back(segment);
	}
}

//============================================================================

} // namespace nav
