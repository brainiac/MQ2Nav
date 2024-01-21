#pragma once

#include <bgfx/bgfx.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <bimg/bimg.h>

namespace bx { struct FileReaderI; }

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

	bool Initialize();
	void Shutdown();

	bx::AllocatorI* GetAllocator() const;
	bx::FileReaderI* GetFileReader() const { return m_fileReader.get(); }

	void Load(std::string_view filePath, uint32_t* outSize = nullptr);

	void* LoadMemory(std::string_view filePath, uint32_t* outSize = nullptr);

	bgfx::TextureHandle LoadTexture(
		std::string_view filePath,
		uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
		bgfx::TextureInfo* info = nullptr,
		bimg::Orientation::Enum* orientation = nullptr);

	bimg::ImageContainer* LoadImage(std::string_view filePath, bgfx::TextureFormat::Enum dstFormat);

	bgfx::ProgramHandle GetProgramHandle(std::string_view programName);

	void Unload(void* ptr);

private:
	bool LoadShaders();
	void FreeShaders();

	std::unique_ptr<bx::FileReaderI> m_fileReader;

	struct ShaderProgram
	{
		const char* name;
		bgfx::ProgramHandle handle;
	};
	std::vector<ShaderProgram> m_shaders;

};

extern ResourceManager* g_resourceMgr;
