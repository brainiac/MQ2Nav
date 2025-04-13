#include "pch.h"
#include "eqg_animation.h"

#include <algorithm>

#include "log_internal.h"
#include "wld_types.h"

namespace eqg {

Animation::Animation()
	: Resource(ResourceType::Animation)
{
}

Animation::~Animation()
{
}

static bool IsLoopingAnimation(std::string_view animTag)
{
	if (animTag.size() < 3)
		return false;

	if (animTag[0] == 'L')
	{
		switch (animTag[2])
		{
		case '3':
		case '4':
		case '8':
			break;

		default:
			return true;
		}
	}
	else if (animTag[0] == 'P')
	{
		switch (animTag[2])
		{
		case '2':
		case '3':
		case '4':
		case '5':
			break;

		default: return true;
		}
	}

	return false;
}

bool Animation::InitFromWLDData(std::string_view animTag, const std::vector<std::shared_ptr<STrack>>& tracks, int frameSkip)
{
	m_tag = animTag;
	m_looping = IsLoopingAnimation(animTag);
	m_numTracks = tracks.size();

	uint32_t maxFrames = 1;
	for (const auto& track : tracks)
	{
		maxFrames = std::max(maxFrames, track->numFrames);
	}

	if (m_looping)
	{
		++maxFrames;
	}

	for (const auto& track : tracks)
	{
		// Build an AnimationTrack for each track.

		uint32_t numFrames = track->numFrames;
		int framesToSkip = 0;

		if (numFrames > 11)
		{
			numFrames = 2 + (numFrames - 2) / (1 + frameSkip);
			framesToSkip = frameSkip;
		}
		uint32_t oldFrameIndex = 0;
		uint32_t extraFrames = m_looping ? 1 : 0;

		std::vector<AnimKey<glm::vec3>> scaleKeys(numFrames + extraFrames);
		std::vector<AnimKey<glm::vec3>> translateKeys(numFrames + extraFrames);
		std::vector<AnimKey<glm::quat>> rotateKeys(numFrames + extraFrames);
		glm::quat previousRotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);

		for (uint32_t frameIndex = 0; frameIndex < numFrames; ++frameIndex, oldFrameIndex += 1 + framesToSkip)
		{
			if (oldFrameIndex >= track->numFrames)
			{
				oldFrameIndex = track->numFrames - 1;
			}

			float time = oldFrameIndex * (track->sleepTime * track->speed);

			scaleKeys[frameIndex].time = time;
			scaleKeys[frameIndex].value = glm::vec3(track->frameTransforms[oldFrameIndex].scale);

			translateKeys[frameIndex].time = time;
			translateKeys[frameIndex].value = track->frameTransforms[oldFrameIndex].pivot;

			rotateKeys[frameIndex].time = time;
			glm::quat& rotation = track->frameTransforms[oldFrameIndex].rotation;

			float sign = 1.0f;
			if (frameIndex != 0)
			{
				// Check to see if we need to flip the rotation around for the shortest path.
				if (glm::length2((rotation * -1.0f) - previousRotation) < glm::length2(rotation - previousRotation))
				{
					sign = -1.0f;
				}
			}

			rotateKeys[frameIndex].value = rotation * sign;
			previousRotation = rotateKeys[frameIndex].value;
		}

		if (m_looping)
		{
			float time = scaleKeys[numFrames - 1].time + track->sleepTime * track->speed;

			scaleKeys[numFrames].time = time;
			scaleKeys[numFrames].value = scaleKeys[0].value;

			translateKeys[numFrames].time = time;
			translateKeys[numFrames].value = translateKeys[0].value;

			rotateKeys[numFrames].time = time;
			glm::quat& rotation = rotateKeys[0].value;

			float sign = 1.0f;
			// Check to see if we need to flip the rotation around for the shortest path.
			if (glm::length2((rotation * -1.0f) - previousRotation) < glm::length2(rotation - previousRotation))
			{
				sign = -1.0f;
			}

			rotateKeys[numFrames].value = rotation * sign;
			numFrames += 1;
		}

		RegisterAnimationSRTKeys(track->tag.substr(m_tag.length()), numFrames, std::move(scaleKeys), std::move(rotateKeys), std::move(translateKeys));
	}

	return true;
}

bool Animation::InitFromBoneData(std::string_view animTag, const std::vector<SDagWLDData>& dags)
{
	m_tag = animTag;
	m_numTracks = static_cast<uint32_t>(dags.size());

	uint32_t maxFrames = 1;
	for (uint32_t index = 0; index < m_numTracks; ++index)
	{
		maxFrames = std::max(dags[index].track->numFrames, maxFrames);
	}

	++maxFrames;

	for (uint32_t index = 0; index < m_numTracks; ++index)
	{
		auto pTrack = dags[index].track;
		uint32_t numFrames = pTrack->numFrames;

		std::vector<AnimKey<glm::vec3>> scaleKeys(maxFrames);
		std::vector<AnimKey<glm::vec3>> translateKeys(maxFrames);
		std::vector<AnimKey<glm::quat>> rotateKeys(maxFrames);
		glm::quat previousRotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);

		for (uint32_t frameIndex = 0; frameIndex < numFrames; ++frameIndex)
		{
			float time = frameIndex * pTrack->sleepTime * pTrack->speed;

			scaleKeys[frameIndex].time = time;
			scaleKeys[frameIndex].value = glm::vec3(pTrack->frameTransforms[frameIndex].scale);
			translateKeys[frameIndex].time = time;
			translateKeys[frameIndex].value = pTrack->frameTransforms[frameIndex].pivot;
			rotateKeys[frameIndex].time = time;
			rotateKeys[frameIndex].value = pTrack->frameTransforms[frameIndex].rotation;
			glm::quat& rotation = pTrack->frameTransforms[frameIndex].rotation;

			float sign = 1.0f;
			if (frameIndex != 0)
			{
				// Check to see if we need to flip the rotation around for the shortest path.
				if (glm::length2((rotation * -1.0f) - previousRotation) < glm::length2(rotation - previousRotation))
				{
					sign = -1.0f;
				}
			}

			rotateKeys[frameIndex].value = rotation * sign;
			previousRotation = rotateKeys[frameIndex].value;
		}

		if (numFrames != 1)
		{
			float time = numFrames * pTrack->sleepTime * pTrack->speed;

			scaleKeys[numFrames].time = time;
			scaleKeys[numFrames].value = scaleKeys[0].value;
			translateKeys[numFrames].time = time;
			translateKeys[numFrames].value = translateKeys[0].value;
			rotateKeys[numFrames].time = time;
			glm::quat& rotation = rotateKeys[0].value;

			float sign = 1.0f;
			// Check to see if we need to flip the rotation around for the shortest path.
			if (glm::length2((rotation * -1.0f) - previousRotation) < glm::length2(rotation - previousRotation))
			{
				sign = -1.0f;
			}

			rotateKeys[numFrames].value = rotation * sign;

			++numFrames;
		}

		RegisterAnimationSRTKeys(pTrack->tag, numFrames, std::move(scaleKeys), std::move(rotateKeys), std::move(translateKeys));
	}

	return true;
}

bool Animation::RegisterAnimationSRTKeys(
	std::string_view name,
	uint32_t numFrames,
	std::vector<AnimKey<glm::vec3>>&& scaleKeys,
	std::vector<AnimKey<glm::quat>>&& rotateKeys,
	std::vector<AnimKey<glm::vec3>>&& translationKeys)
{
	if (m_tracks.contains(name))
		return false;

	auto animTrack = std::make_shared<AnimationTrack>(name, numFrames, std::move(scaleKeys), std::move(rotateKeys), std::move(translationKeys));
	m_tracks[animTrack->GetName()] = animTrack;
	return true;
}

} // namespace eqg
