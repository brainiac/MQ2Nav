#include "pch.h"
#include "eqg_light.h"

#include "eqg_resource_manager.h"

namespace eqg {

LightDefinition::LightDefinition()
	: Resource(GetStaticResourceType())
{
}

LightDefinition::~LightDefinition()
{
}

bool LightDefinition::Init(
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

//-------------------------------------------------------------------------------------------------


PointLight::PointLight(ResourceManager* resourceMgr,
	const std::shared_ptr<LightDefinition>& lightDef,
	const glm::vec3& pos,
	float radius)
	: m_resourceMgr(resourceMgr)
	, m_definition(lightDef)
	, m_position(pos)
	, m_radius(radius)
{
	m_resourceMgr->AddLight(this);
}

PointLight::~PointLight()
{
	m_resourceMgr->RemoveLight(this);
}


} // namespace eqg
