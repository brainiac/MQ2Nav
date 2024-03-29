#ifndef EQEMU_COMMON_EQG_INVIS_WALL_H
#define EQEMU_COMMON_EQG_INVIS_WALL_H

#include <string>
#include <vector>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

#include <glm/glm.hpp>

namespace EQEmu
{

namespace EQG
{

class InvisWall
{
public:
	InvisWall() { }
	~InvisWall() { }

	void SetName(std::string n) { name = n; }

	std::string &GetName() { return name; }
	std::vector<glm::vec3> &GetVerts() { return verts; }
private:
	std::string name;
	std::vector<glm::vec3> verts;
};

}

}

#endif
