/*
 * Copyright 2011-2023 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#pragma once

#include <bx/bounds.h>
#include <bx/file.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>

#include <vector>

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
