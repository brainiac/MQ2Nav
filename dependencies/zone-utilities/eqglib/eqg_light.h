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

	bool Init(
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
using LightDefinitionPtr = std::shared_ptr<LightDefinition>;

// Instance of a point light
class PointLight
{
public:
	PointLight(ResourceManager* resourceMgr,
		const LightDefinitionPtr& lightDef, const glm::vec3& pos, float radius);
	virtual ~PointLight();

	float GetRadius() const { return m_radius; }
	const glm::vec3& GetPosition() const { return m_position; }
	const LightDefinitionPtr& GetDefinition() const { return m_definition; }

	void SetDynamic(bool dynamic) { m_dynamic = dynamic; }
	bool IsDynamic() const { return m_dynamic; }

private:
	ResourceManager*         m_resourceMgr;
	LightDefinitionPtr       m_definition;
	glm::vec3                m_position;
	float                    m_radius;
	bool                     m_dynamic = false;
};

} // namespace eqg
