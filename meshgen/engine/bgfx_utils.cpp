/*
 * Copyright 2011-2023 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "bgfx_utils.h"
#include "ResourceManager.h"

#include <vector>
#include <string>

#include <bgfx/bgfx.h>
#include <bx/readerwriter.h>
#include <meshoptimizer.h>

#include <bimg/decode.h>
#include <bx/debug.h>

#define DBG_STRINGIZE(_x) DBG_STRINGIZE_(_x)
#define DBG_STRINGIZE_(_x) #_x
#define DBG_FILE_LINE_LITERAL "" __FILE__ "(" DBG_STRINGIZE(__LINE__) "): "
#define DBG(_format, ...) bx::debugPrintf(DBG_FILE_LINE_LITERAL "" _format "\n", ##__VA_ARGS__)

Group::Group()
{
	reset();
}

void Group::reset()
{
	m_vbh.idx = bgfx::kInvalidHandle;
	m_ibh.idx = bgfx::kInvalidHandle;
	m_numVertices = 0;
	m_vertices = NULL;
	m_numIndices = 0;
	m_indices = NULL;
	m_prims.clear();
}

namespace bgfx
{
	int32_t read(bx::ReaderI* _reader, bgfx::VertexLayout& _layout, bx::Error* _err);
}

void Mesh::load(bx::ReaderSeekerI* _reader, bool _ramcopy)
{
	constexpr uint32_t kChunkVertexBuffer           = BX_MAKEFOURCC('V', 'B', ' ', 0x1);
	constexpr uint32_t kChunkVertexBufferCompressed = BX_MAKEFOURCC('V', 'B', 'C', 0x0);
	constexpr uint32_t kChunkIndexBuffer            = BX_MAKEFOURCC('I', 'B', ' ', 0x0);
	constexpr uint32_t kChunkIndexBufferCompressed  = BX_MAKEFOURCC('I', 'B', 'C', 0x1);
	constexpr uint32_t kChunkPrimitive              = BX_MAKEFOURCC('P', 'R', 'I', 0x0);

	using namespace bx;
	using namespace bgfx;

	Group group;

	bx::AllocatorI* allocator = g_resourceMgr->GetAllocator();

	uint32_t chunk;
	bx::Error err;
	while (4 == bx::read(_reader, chunk, &err)
	   &&  err.isOk() )
	{
		switch (chunk)
		{
			case kChunkVertexBuffer:
			{
				read(_reader, group.m_sphere, &err);
				read(_reader, group.m_aabb, &err);
				read(_reader, group.m_obb, &err);

				read(_reader, m_layout, &err);

				uint16_t stride = m_layout.getStride();

				read(_reader, group.m_numVertices, &err);
				const bgfx::Memory* mem = bgfx::alloc(group.m_numVertices*stride);
				read(_reader, mem->data, mem->size, &err);

				if (_ramcopy)
				{
					group.m_vertices = (uint8_t*)bx::alloc(allocator, group.m_numVertices*stride);
					bx::memCopy(group.m_vertices, mem->data, mem->size);
				}

				group.m_vbh = bgfx::createVertexBuffer(mem, m_layout);
			}
				break;

			case kChunkVertexBufferCompressed:
			{
				read(_reader, group.m_sphere, &err);
				read(_reader, group.m_aabb, &err);
				read(_reader, group.m_obb, &err);

				read(_reader, m_layout, &err);

				uint16_t stride = m_layout.getStride();

				read(_reader, group.m_numVertices, &err);

				const bgfx::Memory* mem = bgfx::alloc(group.m_numVertices*stride);

				uint32_t compressedSize;
				bx::read(_reader, compressedSize, &err);

				void* compressedVertices = bx::alloc(allocator, compressedSize);
				bx::read(_reader, compressedVertices, compressedSize, &err);

				meshopt_decodeVertexBuffer(mem->data, group.m_numVertices, stride, (uint8_t*)compressedVertices, compressedSize);

				bx::free(allocator, compressedVertices);

				if (_ramcopy)
				{
					group.m_vertices = (uint8_t*)bx::alloc(allocator, group.m_numVertices*stride);
					bx::memCopy(group.m_vertices, mem->data, mem->size);
				}

				group.m_vbh = bgfx::createVertexBuffer(mem, m_layout);
			}
				break;

			case kChunkIndexBuffer:
			{
				read(_reader, group.m_numIndices, &err);

				const bgfx::Memory* mem = bgfx::alloc(group.m_numIndices*2);
				read(_reader, mem->data, mem->size, &err);

				if (_ramcopy)
				{
					group.m_indices = (uint16_t*)bx::alloc(allocator, group.m_numIndices*2);
					bx::memCopy(group.m_indices, mem->data, mem->size);
				}

				group.m_ibh = bgfx::createIndexBuffer(mem);
			}
				break;

			case kChunkIndexBufferCompressed:
			{
				bx::read(_reader, group.m_numIndices, &err);

				const bgfx::Memory* mem = bgfx::alloc(group.m_numIndices*2);

				uint32_t compressedSize;
				bx::read(_reader, compressedSize, &err);

				void* compressedIndices = bx::alloc(allocator, compressedSize);

				bx::read(_reader, compressedIndices, compressedSize, &err);

				meshopt_decodeIndexBuffer(mem->data, group.m_numIndices, 2, (uint8_t*)compressedIndices, compressedSize);

				bx::free(allocator, compressedIndices);

				if (_ramcopy)
				{
					group.m_indices = (uint16_t*)bx::alloc(allocator, group.m_numIndices*2);
					bx::memCopy(group.m_indices, mem->data, mem->size);
				}

				group.m_ibh = bgfx::createIndexBuffer(mem);
			}
				break;

			case kChunkPrimitive:
			{
				uint16_t len;
				read(_reader, len, &err);

				std::string material;
				material.resize(len);
				read(_reader, const_cast<char*>(material.c_str() ), len, &err);

				uint16_t num;
				read(_reader, num, &err);

				for (uint32_t ii = 0; ii < num; ++ii)
				{
					read(_reader, len, &err);

					std::string name;
					name.resize(len);
					read(_reader, const_cast<char*>(name.c_str() ), len, &err);

					Primitive prim;
					read(_reader, prim.m_startIndex, &err);
					read(_reader, prim.m_numIndices, &err);
					read(_reader, prim.m_startVertex, &err);
					read(_reader, prim.m_numVertices, &err);
					read(_reader, prim.m_sphere, &err);
					read(_reader, prim.m_aabb, &err);
					read(_reader, prim.m_obb, &err);

					group.m_prims.push_back(prim);
				}

				m_groups.push_back(group);
				group.reset();
			}
				break;

			default:
				DBG("%08x at %d", chunk, bx::skip(_reader, 0) );
				break;
		}
	}
}

void Mesh::unload()
{
	bx::AllocatorI* allocator = g_resourceMgr->GetAllocator();

	for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
	{
		const Group& group = *it;
		bgfx::destroy(group.m_vbh);

		if (bgfx::isValid(group.m_ibh) )
		{
			bgfx::destroy(group.m_ibh);
		}

		if (NULL != group.m_vertices)
		{
			bx::free(allocator, group.m_vertices);
		}

		if (NULL != group.m_indices)
		{
			bx::free(allocator, group.m_indices);
		}
	}
	m_groups.clear();
}

void Mesh::submit(bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state) const
{
	if (BGFX_STATE_MASK == _state)
	{
		_state = 0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
			| BGFX_STATE_WRITE_Z
			| BGFX_STATE_DEPTH_TEST_LESS
			| BGFX_STATE_CULL_CCW
			| BGFX_STATE_MSAA
			;
	}

	bgfx::setTransform(_mtx);
	bgfx::setState(_state);

	for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
	{
		const Group& group = *it;

		bgfx::setIndexBuffer(group.m_ibh);
		bgfx::setVertexBuffer(0, group.m_vbh);
		bgfx::submit(
			  _id
			, _program
			, 0
			, BGFX_DISCARD_INDEX_BUFFER
			| BGFX_DISCARD_VERTEX_STREAMS
			);
	}

	bgfx::discard();
}

void Mesh::submit(const MeshState*const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices) const
{
	uint32_t cached = bgfx::setTransform(_mtx, _numMatrices);

	for (uint32_t pass = 0; pass < _numPasses; ++pass)
	{
		bgfx::setTransform(cached, _numMatrices);

		const MeshState& state = *_state[pass];
		bgfx::setState(state.m_state);

		for (uint8_t tex = 0; tex < state.m_numTextures; ++tex)
		{
			const MeshState::Texture& texture = state.m_textures[tex];
			bgfx::setTexture(
				  texture.m_stage
				, texture.m_sampler
				, texture.m_texture
				, texture.m_flags
				);
		}

		for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
		{
			const Group& group = *it;

			bgfx::setIndexBuffer(group.m_ibh);
			bgfx::setVertexBuffer(0, group.m_vbh);
			bgfx::submit(
				  state.m_viewId
				, state.m_program
				, 0
				, BGFX_DISCARD_INDEX_BUFFER
				| BGFX_DISCARD_VERTEX_STREAMS
				);
		}

		bgfx::discard(0
			| BGFX_DISCARD_BINDINGS
			| BGFX_DISCARD_STATE
			| BGFX_DISCARD_TRANSFORM
			);
	}

	bgfx::discard();
}

Mesh* meshLoad(bx::ReaderSeekerI* _reader, bool _ramcopy)
{
	Mesh* mesh = new Mesh;
	mesh->load(_reader, _ramcopy);
	return mesh;
}

Mesh* meshLoad(const char* _filePath, bool _ramcopy)
{
	bx::FileReaderI* reader = g_resourceMgr->GetFileReader();
	if (bx::open(reader, _filePath) )
	{
		Mesh* mesh = meshLoad(reader, _ramcopy);
		bx::close(reader);
		return mesh;
	}

	return NULL;
}

void meshUnload(Mesh* _mesh)
{
	_mesh->unload();
	delete _mesh;
}

MeshState* meshStateCreate()
{
	MeshState* state = (MeshState*)bx::alloc(g_resourceMgr->GetAllocator(), sizeof(MeshState));
	return state;
}

void meshStateDestroy(MeshState* _meshState)
{
	bx::free(g_resourceMgr->GetAllocator(), _meshState);
}

void meshSubmit(const Mesh* _mesh, bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state)
{
	_mesh->submit(_id, _program, _mtx, _state);
}

void meshSubmit(const Mesh* _mesh, const MeshState*const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices)
{
	_mesh->submit(_state, _numPasses, _mtx, _numMatrices);
}
