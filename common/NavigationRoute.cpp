//
// NavigationRoute.cpp
//

#include "NavigationRoute.h"

#include <algorithm>

void NavigationBuffer::Resize(uint32_t size)
{
	if (size > reserved)
	{
		reserved = size;
		polys = std::make_unique<dtPolyRef[]>(size);
		flags = std::make_unique<uint8_t[]>(reserved);
		points = std::make_unique<glm::vec3[]>(reserved);
	}
}

void NavigationBuffer::Reset()
{
	reserved = 0;
	count = 0;
	polys.reset();
	flags.reset();
	points.reset();
}

float NavigationRoute::GetPathTraversalDistance() const
{
	float result = 0.f;

	if (size() > 0)
	{
		auto last = begin();

		for (auto iter = std::next(begin()); iter != end(); ++iter)
		{
			result += glm::distance(last->pos(), iter->pos());
			last = iter;
		}
	}

	return result;
}
