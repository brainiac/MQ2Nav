#pragma once

#include "eqg_resource.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace eqg {

struct SDagWLDData;

struct SFrameTransform // TODO: Turn into mat4x4?
{
	glm::quat rotation;
	glm::vec3 pivot;
	float     scale;
};

struct STrack
{
	std::string_view tag;
	float            speed;
	bool             reverse;
	bool             interpolate;
	uint32_t         sleepTime;
	uint32_t         numFrames;
	std::vector<SFrameTransform> frameTransforms;
	bool             attachedToModel;
};

template <typename T>
struct AnimKey
{
	float time;
	T     value;
};

class AnimationTrack
{
public:
	AnimationTrack(std::string_view name,
		uint32_t numFrames,
		std::vector<AnimKey<glm::vec3>>&& scaleKeys,
		std::vector<AnimKey<glm::quat>>&& rotateKeys,
		std::vector<AnimKey<glm::vec3>>&& translationKeys)
		: m_name(name)
		, m_numFrames(numFrames)
		, m_scaleKeys(std::move(scaleKeys))
		, m_rotateKeys(std::move(rotateKeys))
		, m_translationKey(std::move(translationKeys))
	{
	}

	std::string_view GetName() const { return m_name; }

protected:
	std::string m_name;
	uint32_t m_numFrames;

	std::vector<AnimKey<glm::vec3>> m_scaleKeys;
	std::vector<AnimKey<glm::quat>> m_rotateKeys;
	std::vector<AnimKey<glm::vec3>> m_translationKey;
};
using AnimationTrackPtr = std::shared_ptr<AnimationTrack>;

class Animation : public Resource
{
public:
	Animation();
	~Animation() override;

	static ResourceType GetStaticResourceType() { return ResourceType::Animation; }

	std::string_view GetTag() const override { return m_tag; }

	bool InitFromWLDData(std::string_view animTag, const std::vector<std::shared_ptr<STrack>>& tracks, int frameSkip);
	bool InitFromBoneData(std::string_view animTag, const std::vector<SDagWLDData>& dags);

protected:
	virtual bool RegisterAnimationSRTKeys(std::string_view name,
		uint32_t numFrames,
		std::vector<AnimKey<glm::vec3>>&& scaleKeys,
		std::vector<AnimKey<glm::quat>>&& rotateKeys,
		std::vector<AnimKey<glm::vec3>>&& translationKeys);

private:
	std::string       m_tag;
	bool              m_looping = false;
	uint32_t          m_numTracks = 0;

	std::unordered_map<std::string_view, AnimationTrackPtr> m_tracks;
};

} // namespace eqg
