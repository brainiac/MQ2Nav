
#pragma once

#include <glm/glm.hpp>

namespace eqg {

glm::mat4 CreateRotateMatrix(float rx, float ry, float rz);
glm::mat4 CreateTranslateMatrix(float tx, float ty, float tz);
glm::mat4 CreateScaleMatrix(float sx, float sy, float sz);

class OrientedBoundingBox
{
public:
	OrientedBoundingBox() {}
	OrientedBoundingBox(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec3 extents);
	~OrientedBoundingBox() {}

	bool ContainsPoint(glm::vec3 p) const;

	glm::mat4& GetTransformation() { return transformation; }
	glm::mat4& GetInvertedTransformation() { return inverted_transformation; }

private:
	float min_x = 0.0f, max_x = 0.0f;
	float min_y = 0.0f, max_y = 0.0f;
	float min_z = 0.0f, max_z = 0.0f;
	glm::mat4 transformation;
	glm::mat4 inverted_transformation;
};

} // namespace eqg
