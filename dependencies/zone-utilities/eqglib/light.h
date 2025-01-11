
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace EQEmu {

// TODO: Split into LightDefinition, PointLight, etc.
class Light
{
public:
	std::string name;
	std::string tag;

	std::vector<glm::vec3> color;
	std::vector<float> intensity;
	bool skip_frames = false;
	int current_frame = 0;
	int update_interval = 1;

	glm::vec3 pos;
	float radius;

	glm::mat4x4 transform;
	bool static_light = true;
};

} // namespace EQEmu
