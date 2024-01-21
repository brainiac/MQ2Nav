#include "pch.h"
#include "ResourceManager.h"


#include <mq/base/Common.h>

#include <bx/pixelformat.h>
#include <bx/file.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>

#include "shaders/imgui/fs_imgui.bin.h"
#include "shaders/imgui/vs_imgui.bin.h"
#include "shaders/imgui/fs_imgui_image.bin.h"
#include "shaders/imgui/vs_imgui_image.bin.h"
#include "shaders/navmesh/fs_inputgeom.bin.h"
#include "shaders/navmesh/vs_inputgeom.bin.h"
#include "shaders/navmesh/fs_meshtile.bin.h"
#include "shaders/navmesh/vs_meshtile.bin.h"
#include "shaders/lines/fs_lines.bin.h"
#include "shaders/lines/vs_lines.bin.h"
#include "shaders/points/fs_points.bin.h"
#include "shaders/points/vs_points.bin.h"

#define DBG_STRINGIZE(_x) DBG_STRINGIZE_(_x)
#define DBG_STRINGIZE_(_x) #_x
#define DBG_FILE_LINE_LITERAL "" __FILE__ "(" DBG_STRINGIZE(__LINE__) "): "
#define DBG(_format, ...) bx::debugPrintf(DBG_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__)

#define EMBEDDED_SHADER(name)                                              \
	{                                                                      \
		#name,                                                             \
		BX_CONCATENATE(fs_, BX_CONCATENATE(name, _dx11)),                  \
		BX_COUNTOF(BX_CONCATENATE(fs_, BX_CONCATENATE(name, _dx11))),      \
		BX_CONCATENATE(vs_, BX_CONCATENATE(name, _dx11)),                  \
		BX_COUNTOF(BX_CONCATENATE(vs_, BX_CONCATENATE(name, _dx11))),      \
	}

struct EmbeddedShaderInfo
{
	const char* name;

	const uint8_t* fragment_shader;
	uint32_t fragment_shader_size;
	const uint8_t* vertex_shader;
	uint32_t vertex_shader_size;
};

static EmbeddedShaderInfo s_embeddedShaders[] = {
	EMBEDDED_SHADER(imgui),
	EMBEDDED_SHADER(imgui_image),
	EMBEDDED_SHADER(inputgeom),
	EMBEDDED_SHADER(meshtile),
	EMBEDDED_SHADER(lines),
	EMBEDDED_SHADER(points),
};

//============================================================================

ResourceManager* g_resourceMgr = nullptr;

bx::AllocatorI* getDefaultAllocator()
{
	static bx::DefaultAllocator s_allocator;
	return &s_allocator;
}
bx::AllocatorI* g_allocator = getDefaultAllocator();

using String = bx::StringT<&g_allocator>;
static String s_currentDir;

class FileReader : public bx::FileReader
{
	using super = bx::FileReader;

public:
	bool open(const bx::FilePath& filePath_, bx::Error* err) override
	{
		String filePath(s_currentDir);

		filePath.append(filePath_);
		return super::open(filePath.getPtr(), err);
	}
};

static void* Load(bx::FileReaderI* reader, bx::AllocatorI* allocator, std::string_view filePath, uint32_t* outSize)
{
	bx::StringView filePathSv(filePath.data(), static_cast<int32_t>(filePath.length()));

	if (bx::open(reader, filePathSv))
	{
		uint32_t size = static_cast<uint32_t>(bx::getSize(reader));
		void* data = bx::alloc(allocator, size);
		bx::read(reader, data, size, bx::ErrorAssert{});
		bx::close(reader);

		if (outSize != nullptr)
		{
			*outSize = size;
		}
		return data;
	}

	DBG("Failed to open: %.*s.", filePath.length(), filePath.data());

	if (outSize != nullptr)
	{
		*outSize = 0;
	}

	return nullptr;
}

static void Unload(void* ptr)
{
	bx::free(g_allocator, ptr);
}

static const bgfx::Memory* LoadMemory(bx::FileReaderI* fileReader, std::string_view filePath)
{
	bx::StringView filePathSv(filePath.data(), static_cast<int32_t>(filePath.length()));

	if (bx::open(fileReader, filePathSv))
	{
		uint32_t size = static_cast<uint32_t>(bx::getSize(fileReader));
		const bgfx::Memory* mem = bgfx::alloc(size + 1);
		bx::read(fileReader, mem->data, size, bx::ErrorAssert{});
		bx::close(fileReader);

		mem->data[mem->size - 1] = '\0';
		return mem;
	}

	DBG("Failed to load: %.*s.", filePath.length(), filePath.data());
	return nullptr;
}

static void* LoadMemory(bx::FileReaderI* fileReader, bx::AllocatorI* allocator, std::string_view filePath, uint32_t* outSize)
{
	bx::StringView filePathSv(filePath.data(), static_cast<int32_t>(filePath.length()));

	if (bx::open(fileReader, filePathSv))
	{
		uint32_t size = static_cast<uint32_t>(bx::getSize(fileReader));
		void* data = bx::alloc(allocator, size);
		bx::read(fileReader, data, size, bx::ErrorAssert{});
		bx::close(fileReader);

		if (outSize != nullptr)
		{
			*outSize = size;
		}

		return data;
	}

	DBG("Failed to load %.*s.", filePath.length(), filePath.data());
	return nullptr;
}

static void ImageReleaseCallback(void* ptr, void* userData)
{
	UNUSED(ptr);

	bimg::ImageContainer* imageContainer = static_cast<bimg::ImageContainer*>(userData);
	bimg::imageFree(imageContainer);
}

static bgfx::TextureHandle LoadTexture(
	bx::FileReaderI* reader,
	std::string_view filePath,
	uint64_t flags,
	bgfx::TextureInfo* info,
	bimg::Orientation::Enum* orientation)
{
	bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;


	uint32_t size;
	if (void* data = Load(reader, g_allocator, filePath, &size); data != nullptr)
	{
		if (bimg::ImageContainer* imageContainer = bimg::imageParse(g_allocator, data, size); imageContainer != nullptr)
		{
			if (orientation != nullptr)
			{
				*orientation = imageContainer->m_orientation;
			}

			const bgfx::Memory* mem = bgfx::makeRef(imageContainer->m_data, imageContainer->m_size,
				ImageReleaseCallback, imageContainer);
			Unload(data);

			if (imageContainer->m_cubeMap)
			{
				handle = bgfx::createTextureCube(
					static_cast<uint16_t>(imageContainer->m_width),
					1 < imageContainer->m_numMips,
					imageContainer->m_numLayers,
					static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
					flags,
					mem);
			}
			else if (1 < imageContainer->m_depth)
			{
				handle = bgfx::createTexture3D(
					static_cast<uint16_t>(imageContainer->m_width),
					static_cast<uint16_t>(imageContainer->m_height),
					static_cast<uint16_t>(imageContainer->m_depth),
					1 < imageContainer->m_numMips,
					static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
					flags,
					mem);
			}
			else if (bgfx::isTextureValid(0, false, imageContainer->m_numLayers, static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format), flags))
			{
				handle = bgfx::createTexture2D(
					static_cast<uint16_t>(imageContainer->m_width),
					static_cast<uint16_t>(imageContainer->m_height),
					1 < imageContainer->m_numMips,
					imageContainer->m_numLayers,
					static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format),
					flags,
					mem);
			}

			if (bgfx::isValid(handle))
			{
				bgfx::setName(handle, filePath.data(), static_cast<int32_t>(filePath.length()));
			}

			if (info != nullptr)
			{
				bgfx::calcTextureSize(
					*info,
					static_cast<uint16_t>(imageContainer->m_width),
					static_cast<uint16_t>(imageContainer->m_height),
					static_cast<uint16_t>(imageContainer->m_depth),
					imageContainer->m_cubeMap,
					1 < imageContainer->m_numMips,
					imageContainer->m_numLayers,
					static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format));
			}
		}
	}

	return handle;
}

//============================================================================

ResourceManager::ResourceManager()
	: m_fileReader(std::make_unique<FileReader>())
{
	s_currentDir.set("");
}

ResourceManager::~ResourceManager()
{
}

bool ResourceManager::Initialize()
{
	if (!LoadShaders())
		return false;

	return true;
}

void ResourceManager::Shutdown()
{
	FreeShaders();
}

bx::AllocatorI* ResourceManager::GetAllocator() const
{
	return g_allocator;
}

void ResourceManager::Load(std::string_view filePath, uint32_t* outSize)
{
	::Load(m_fileReader.get(), g_allocator, filePath, outSize);
}

void ResourceManager::Unload(void* ptr)
{
	::Unload(ptr);
}

void* ResourceManager::LoadMemory(std::string_view filePath, uint32_t* outSize)
{
	return ::LoadMemory(m_fileReader.get(), g_allocator, filePath, outSize);
}

bgfx::TextureHandle ResourceManager::LoadTexture(std::string_view filePath, uint64_t flags, bgfx::TextureInfo* info,
	bimg::Orientation::Enum* orientation)
{
	return ::LoadTexture(m_fileReader.get(), filePath, flags, info, orientation);
}

bimg::ImageContainer* ResourceManager::LoadImage(std::string_view filePath, bgfx::TextureFormat::Enum dstFormat)
{
	uint32_t size = 0;
	void* data = ::LoadMemory(m_fileReader.get(), g_allocator, filePath, &size);

	return bimg::imageParse(g_allocator, data, size, static_cast<bimg::TextureFormat::Enum>(dstFormat));
}

bgfx::ProgramHandle ResourceManager::GetProgramHandle(std::string_view programName)
{
	for (const ShaderProgram& shaderEntry : m_shaders)
	{
		if (programName == shaderEntry.name)
			return shaderEntry.handle;
	}

	return BGFX_INVALID_HANDLE;
}

//============================================================================

static bgfx::ProgramHandle CreateEmbeddedProgram(const EmbeddedShaderInfo* esi)
{
	char shaderName[256] = { 0 };
	sprintf_s(shaderName, "vs_%s", esi->name);

	bgfx::ShaderHandle vertexShader = bgfx::createShader(bgfx::makeRef(esi->vertex_shader, esi->vertex_shader_size));
	if (bgfx::isValid(vertexShader))
	{
		bgfx::setName(vertexShader, shaderName);
	}
	else
	{
		DBG("Failed to create vertex shader: %s", shaderName);
		return BGFX_INVALID_HANDLE;
	}

	sprintf_s(shaderName, "fs_%s", esi->name);

	bgfx::ShaderHandle fragmentShader = bgfx::createShader(bgfx::makeRef(esi->fragment_shader, esi->fragment_shader_size));
	if (bgfx::isValid(fragmentShader))
	{
		bgfx::setName(fragmentShader, shaderName);
	}
	else
	{
		DBG("Failed to create fragment shader: %s", shaderName);
		bgfx::destroy(vertexShader);
		return BGFX_INVALID_HANDLE;
	}

	// Now create the final program.
	bgfx::ProgramHandle programHandle = bgfx::createProgram(vertexShader, fragmentShader, true);
	if (bgfx::isValid(programHandle))
	{
		return programHandle;
	}

	DBG("Failed to create program: %s", esi->name);

	bgfx::destroy(fragmentShader);
	bgfx::destroy(vertexShader);

	return BGFX_INVALID_HANDLE;
}

bool ResourceManager::LoadShaders()
{
	bool failed = false;
	for (const EmbeddedShaderInfo& esi : s_embeddedShaders)
	{
		bgfx::ProgramHandle programHandle = CreateEmbeddedProgram(&esi);
		if (bgfx::isValid(programHandle))
		{
			m_shaders.push_back(ShaderProgram{ .name = esi.name, .handle = programHandle });
		}
		else
		{
			failed = true;
			break;
		}
	}

	if (failed)
	{
		// We failed, so clear any shaders that we did create.
		FreeShaders();
	}

	return !failed;
}

void ResourceManager::FreeShaders()
{
	for (const ShaderProgram& shaderEntry : m_shaders)
	{
		bgfx::destroy(shaderEntry.handle);
	}

	m_shaders.clear();
}
