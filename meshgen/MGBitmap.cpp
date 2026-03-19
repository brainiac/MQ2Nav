//
// MGBitmap.cpp
//

#include "pch.h"
#include "MGBitmap.h"

#include "bimg/decode.h"
#include "spdlog/spdlog.h"

extern bx::AllocatorI* g_allocator;

static void ImageReleaseCallback(void* ptr, void* userData)
{
	(void)ptr;
	bimg::ImageContainer* imageContainer = static_cast<bimg::ImageContainer*>(userData);
	bimg::imageFree(imageContainer);
}

MGBitmap::MGBitmap()
{
}

MGBitmap::~MGBitmap()
{
	if (bgfx::isValid(m_textureHandle))
	{
		bgfx::destroy(m_textureHandle);
		m_textureHandle = BGFX_INVALID_HANDLE;
	}
}

void MGBitmap::SetRawData(std::unique_ptr<char[]> rawData, size_t rawDataSize)
{
	m_rawData = std::move(rawData);
	m_rawDataSize = rawDataSize;
}

bool MGBitmap::LoadTexture()
{
	if (!m_rawData || m_rawDataSize == 0)
	{
		SPDLOG_ERROR("MGBitmap::LoadTexture: No raw data available for {}", GetFileName());
		return false;
	}

	// Parse the image data using bimg
	bimg::ImageContainer* imageContainer = bimg::imageParse(
		g_allocator,
		m_rawData.get(),
		static_cast<uint32_t>(m_rawDataSize));

	if (!imageContainer)
	{
		SPDLOG_ERROR("MGBitmap::LoadTexture: Failed to parse image data for {}", GetFileName());
		return false;
	}

	// Create bgfx memory reference
	const bgfx::Memory* mem = bgfx::makeRef(
		imageContainer->m_data,
		imageContainer->m_size,
		ImageReleaseCallback,
		imageContainer);

	uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE;

	if (imageContainer->m_cubeMap)
	{
		m_textureHandle = bgfx::createTextureCube(
			static_cast<uint16_t>(imageContainer->m_width),
			1 < imageContainer->m_numMips,
			imageContainer->m_numLayers,
			static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
			flags,
			mem);
	}
	else if (1 < imageContainer->m_depth)
	{
		m_textureHandle = bgfx::createTexture3D(
			static_cast<uint16_t>(imageContainer->m_width),
			static_cast<uint16_t>(imageContainer->m_height),
			static_cast<uint16_t>(imageContainer->m_depth),
			1 < imageContainer->m_numMips,
			static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
			flags,
			mem);
	}
	else if (bgfx::isTextureValid(
		0,
		false,
		imageContainer->m_numLayers,
		static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
		flags))
	{
		m_textureHandle = bgfx::createTexture2D(
			static_cast<uint16_t>(imageContainer->m_width),
			static_cast<uint16_t>(imageContainer->m_height),
			1 < imageContainer->m_numMips,
			imageContainer->m_numLayers,
			static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
			flags,
			mem);
	}

	if (bgfx::isValid(m_textureHandle))
	{
		bgfx::setName(m_textureHandle, GetFileName().c_str(), static_cast<int32_t>(GetFileName().length()));
		SPDLOG_DEBUG("MGBitmap::LoadTexture: Created texture for {} ({}x{})",
			GetFileName(), imageContainer->m_width, imageContainer->m_height);
		return true;
	}

	SPDLOG_ERROR("MGBitmap::LoadTexture: Failed to create bgfx texture for {}", GetFileName());
	return false;
}
