/*
 * Copyright 2011-2023 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#pragma once

#include <bx/bounds.h>
#include <bx/pixelformat.h>
#include <bx/file.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>

#include <vector>

void utilsInit();

void utilsShutdown();

namespace utils
{
	extern bx::AllocatorI* g_allocator;

	bx::AllocatorI* getAllocator();
	bx::FileReaderI* getFileReader();
	bx::FileWriterI* getFileWriter();

	void setCurrentDir(const char* _dir);
}


///
void* load(const char* _filePath, uint32_t* _size = NULL);

///
void unload(void* _ptr);

///
bgfx::ShaderHandle loadShader(const char* _name);

///
bgfx::ProgramHandle loadProgram(const char* _vsName, const char* _fsName);

///
bgfx::TextureHandle loadTexture(const char* _name, uint64_t _flags = BGFX_TEXTURE_NONE|BGFX_SAMPLER_NONE, uint8_t _skip = 0, bgfx::TextureInfo* _info = NULL, bimg::Orientation::Enum* _orientation = NULL);

///
bimg::ImageContainer* imageLoad(const char* _filePath, bgfx::TextureFormat::Enum _dstFormat);

///
void calcTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices);

///
inline uint32_t encodeNormalRgba8(float _x, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f)
{
	const float src[] =
	{
		_x * 0.5f + 0.5f,
		_y * 0.5f + 0.5f,
		_z * 0.5f + 0.5f,
		_w * 0.5f + 0.5f,
	};
	uint32_t dst;
	bx::packRgba8(&dst, src);
	return dst;
}

///
struct MeshState
{
	struct Texture
	{
		uint32_t            m_flags;
		bgfx::UniformHandle m_sampler;
		bgfx::TextureHandle m_texture;
		uint8_t             m_stage;
	};

	Texture             m_textures[4];
	uint64_t            m_state;
	bgfx::ProgramHandle m_program;
	uint8_t             m_numTextures;
	bgfx::ViewId        m_viewId;
};

struct Primitive
{
	uint32_t   m_startIndex;
	uint32_t   m_numIndices;
	uint32_t   m_startVertex;
	uint32_t   m_numVertices;

	bx::Sphere m_sphere;
	bx::Aabb   m_aabb;
	bx::Obb    m_obb;
};

using PrimitiveArray = std::vector<Primitive>;

struct Group
{
	Group();
	void reset();

	bgfx::VertexBufferHandle     m_vbh;
	bgfx::IndexBufferHandle      m_ibh;
	uint16_t                     m_numVertices;
	uint8_t*                     m_vertices;
	uint32_t                     m_numIndices;
	uint16_t*                    m_indices;
	bx::Sphere                   m_sphere;
	bx::Aabb                     m_aabb;
	bx::Obb                      m_obb;
	PrimitiveArray               m_prims;
};
using GroupArray = std::vector<Group>;

struct Mesh
{
	void load(bx::ReaderSeekerI* _reader, bool _ramcopy);
	void unload();
	void submit(bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state) const;
	void submit(const MeshState*const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices) const;

	bgfx::VertexLayout m_layout;
	GroupArray m_groups;
};

///
Mesh* meshLoad(const char* _filePath, bool _ramcopy = false);

///
void meshUnload(Mesh* _mesh);

///
MeshState* meshStateCreate();

///
void meshStateDestroy(MeshState* _meshState);

///
void meshSubmit(const Mesh* _mesh, bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state = BGFX_STATE_MASK);

///
void meshSubmit(const Mesh* _mesh, const MeshState*const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices = 1);

inline bool checkAvailTransientBuffers(uint32_t numVertices, const bgfx::VertexLayout& layout, uint32_t numIndices)
{
	return numVertices == bgfx::getAvailTransientVertexBuffer(numVertices, layout)
		&& (0 == numIndices || numIndices == bgfx::getAvailTransientIndexBuffer(numIndices));
}
