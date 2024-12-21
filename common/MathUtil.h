#pragma once

#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>


template <typename T, glm::qualifier Q>
glm::vec<3, T, Q> to_eq_coord(const glm::vec<3, T, Q>& world)
{
	return world.zxy();
}

template <typename T, glm::qualifier Q>
glm::vec<3, T, Q> from_eq_coord(const glm::vec<3, T, Q>& eq)
{
	return eq.yzx();
}
