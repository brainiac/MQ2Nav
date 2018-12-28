//
// MapAPI.h
//

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace EQUIStructs {
	typedef struct _MAPLINE MAPLINE;
}

namespace nav {

class MapLine
{
public:
	MapLine();
	~MapLine();

	void SetColor(uint32_t argbcolor);
	uint32_t GetColor() const { return m_color; }

	void SetLayer(int layer);
	int GetLayer() const { return m_layer; }

	bool AddPoint(const glm::vec3& point);

	void Clear();

	// Create a new line and return a shared pointer to it. The line will
	// automatically be destroyed via DeleteLine.
	static std::shared_ptr<EQUIStructs::MAPLINE> CreateSegment();

private:
	std::vector<std::shared_ptr<EQUIStructs::MAPLINE>> m_lineSegments;
	uint32_t m_color = 0xff00ff00; // green
	int m_layer = 3; // layer 3

	glm::vec3 m_lastPos;
	bool m_hasLastPos = false;
};

} // namespace nav
