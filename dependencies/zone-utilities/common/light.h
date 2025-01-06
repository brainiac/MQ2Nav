
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace EQEmu {

class Light
{
public:
	std::string tag;

	std::vector<glm::vec3> color;
	std::vector<float> intensity;
	bool skip_frames = false;
	int current_frame = 0;
	int update_interval = 1;

	glm::vec3 pos;
	float radius;
};

} // namespace EQEmu
