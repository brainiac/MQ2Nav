#pragma once

#include "eqg_resource.h"

namespace eqg {

class LightDefinition : public eqg::Resource
{
public:
	LightDefinition();
	~LightDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::LightDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	uint32_t GetNumFrames() const { return (uint32_t)m_framesIntensity.size(); }

	float GetIntensity(uint32_t frame) const { return m_framesIntensity[frame]; }

	bool HasColors() const { return !m_framesColor.empty(); }
	const glm::vec3& GetColor(uint32_t frame) const
	{
		if (frame < (uint32_t)m_framesColor.size())
			return m_framesColor[frame];

		return glm::vec3(1.0f);
	}

	int GetCurrentFrame() const { return m_currentFrame; }
	int GetUpdateInterval() const { return m_updateInterval; }

	bool InitFromWLDData(
		std::string_view tag,
		uint32_t frameCount,
		float* framesIntensity,
		glm::vec3* framesColor,
		int currentFrame,
		int updateInterval,
		bool skipFrames
	);

private:
	std::string m_tag;

	std::vector<float> m_framesIntensity;
	std::vector<glm::vec3> m_framesColor;
	int m_currentFrame = 0;
	int m_updateInterval = 0;
	bool m_skipFrames = false;
};

// Instance of a point light
class PointLight
{
public:
	PointLight(const std::shared_ptr<LightDefinition>& lightDef, const glm::vec3& pos, float radius)
		: m_lightDefinition(lightDef)
		, m_position(pos)
		, m_radius(radius)
	{
	}

	std::shared_ptr<LightDefinition> m_lightDefinition;
	glm::vec3 m_position;
	float m_radius;
};

} // namespace eqg
