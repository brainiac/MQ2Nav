//
// RenderBatch.cpp
//

#include "pch.h"
#include "RenderBatch.h"

#include "bgfx/bgfx.h"

static bgfx::VertexLayout s_staticMeshVertexLayout;

void StaticMeshVertex::Init()
{
	s_staticMeshVertexLayout
		.begin()
			.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
			.add(bgfx::Attrib::Color1,    4, bgfx::AttribType::Uint8, true)
		.end();
}

bgfx::VertexLayout& StaticMeshVertex::GetLayout()
{
	return s_staticMeshVertexLayout;
}

static bgfx::VertexLayout s_meshInstanceDataLayout;

void MeshInstanceData::Init()
{
	s_meshInstanceDataLayout
		.begin()
			.add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		.end();
}

bgfx::VertexLayout& MeshInstanceData::GetLayout()
{
	return s_meshInstanceDataLayout;
}
