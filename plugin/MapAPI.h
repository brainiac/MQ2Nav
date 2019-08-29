//
// MapAPI.h
//

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace eqlib {
	struct MAPLINE;
}

namespace nav {

class MapItem
{
public:
	MapItem() = default;
	virtual ~MapItem() = default;

	void SetColor(uint32_t argbcolor);
	uint32_t GetColor() const { return m_color; }

	void SetLayer(int layer);
	int GetLayer() const { return m_layer; }

	virtual void Clear();

	// Create a new line and return a shared pointer to it. The line will
	// automatically be destroyed via DeleteLine.
	static std::shared_ptr<eqlib::MAPLINE> CreateSegment();

protected:
	std::vector<std::shared_ptr<eqlib::MAPLINE>> m_lineSegments;
	uint32_t m_color = 0xff00ff00; // green
	int m_layer = 3; // layer 3
};

class MapLine : public MapItem
{
public:
	bool AddPoint(const glm::vec3& point);

	virtual void Clear() override;

private:
	glm::vec3 m_lastPos;
	bool m_hasLastPos = false;
};

class MapCircle : public MapItem
{
public:
	MapCircle() = default;
	MapCircle(const glm::vec3& position, float radius);

	void SetCircle(const glm::vec3& position, float radius);

	virtual void Clear() override;

private:
	void UpdateCircle();

	glm::vec3 m_position;
	float m_radius = 0;
	bool m_created = false;
};

} // namespace nav
