#include "pch.h"
#include "eqg_light.h"

namespace eqg {

LightDefinition::LightDefinition()
	: Resource(GetStaticResourceType())
{
}

LightDefinition::~LightDefinition()
{
}

bool LightDefinition::InitFromWLDData(
	std::string_view tag,
	uint32_t frameCount,
	float* framesIntensity,
	glm::vec3* framesColor,
	int currentFrame,
	int updateInterval,
	bool skipFrames)
{
	m_tag = tag;
	m_currentFrame = currentFrame;
	m_updateInterval = updateInterval;
	m_skipFrames = skipFrames;
	
	m_framesIntensity.resize(frameCount);
	memcpy(m_framesIntensity.data(), framesIntensity, frameCount * sizeof(float));

	if (framesColor)
	{
		m_framesColor.resize(frameCount);
		memcpy(m_framesColor.data(), framesColor, frameCount * sizeof(glm::vec3));
	}

	return true;
}

} // namespace eqg
