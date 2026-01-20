//
// MGBitmap.h
//

#pragma once

#include "eqglib/eqg_material.h"

#include <bgfx/bgfx.h>
#include <memory>

// Engine-specific bitmap subclass that stores bgfx texture data.
// Created by MGResourceManager::CreateBitmap() and populated via LoadTexture().
class MGBitmap : public eqg::Bitmap
{
public:
	MGBitmap();
	virtual ~MGBitmap() override;

	bgfx::TextureHandle GetTextureHandle() const { return m_textureHandle; }
	bool HasValidTexture() const { return bgfx::isValid(m_textureHandle); }

	// Load texture from raw data into GPU.
	virtual bool LoadTexture() override;

	virtual char* GetRawData() const override { return m_rawData.get(); }
	size_t GetRawDataSize() const { return m_rawDataSize; }
	virtual void SetRawData(std::unique_ptr<char[]> rawData, size_t rawDataSize) override;

private:
	bgfx::TextureHandle m_textureHandle = BGFX_INVALID_HANDLE;
	std::unique_ptr<char[]> m_rawData;
	size_t m_rawDataSize = 0;
};
