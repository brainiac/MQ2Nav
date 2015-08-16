//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "pch.h"
#include "MeshLoaderObj.h"
#include "map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>


rcMeshLoaderObj::rcMeshLoaderObj() :
	m_scale(1.0f),
	m_verts(0),
	m_tris(0),
	m_normals(0),
	m_vertCount(0),
	m_triCount(0)
{
}

rcMeshLoaderObj::~rcMeshLoaderObj()
{
	delete [] m_verts;
	delete [] m_normals;
	delete [] m_tris;
}
		
void rcMeshLoaderObj::addVertex(float x, float y, float z, int& cap)
{
	if (m_vertCount+1 > cap)
	{
		cap = !cap ? 8 : cap*2;
		float* nv = new float[cap*3];
		if (m_vertCount)
			memcpy(nv, m_verts, m_vertCount*3*sizeof(float));
		delete [] m_verts;
		m_verts = nv;
	}
	float* dst = &m_verts[m_vertCount*3];
	*dst++ = x*m_scale;
	*dst++ = y*m_scale;
	*dst++ = z*m_scale;
	m_vertCount++;
}

void rcMeshLoaderObj::addTriangle(int a, int b, int c, int& cap)
{
	if (m_triCount+1 > cap)
	{
		cap = !cap ? 8 : cap*2;
		int* nv = new int[cap*3];
		if (m_triCount)
			memcpy(nv, m_tris, m_triCount*3*sizeof(int));
		delete [] m_tris;
		m_tris = nv;
	}
	int* dst = &m_tris[m_triCount*3];
	*dst++ = a;
	*dst++ = b;
	*dst++ = c;
	m_triCount++;
}

bool rcMeshLoaderObj::load(const char* zoneShortName, const char* everquest_path)
{
	std::string filename = zoneShortName;
	Map map;

	// change the working directory to the everquest folder
	char orig_directory[_MAX_PATH] = { 0 };
	GetCurrentDirectoryA(_MAX_PATH, orig_directory);

	SetCurrentDirectoryA(everquest_path);

	int tcap = 0, vcap = 0;
	uint32_t counter = 0;

	if (map.Build(filename))
	{
		// load terrain geometry
		std::shared_ptr<EQEmu::EQG::Terrain> terrain = map.GetTerrain();
		if (terrain)
		{
			//const auto& tiles = terrain->GetTiles();

			//for (uint32_t i = 0; i < tiles.size(); ++i)
			//{
			//	auto& tile = tiles[i];

			//	float x = tile->GetX();
			//	float y = tile->GetY();

			//	if (tile->IsFlat())
			//	{
			//		float z = tile->GetFloats()[0];

			//		//addVertex(x, z, y, vcap);
			//	}
			//}
		}


		const auto& collide_indices = map.GetCollideIndices();

		for (uint32_t index = 0; index < collide_indices.size(); index += 3, counter += 3)
		{
			uint32_t vert_index1 = collide_indices[index];
			const glm::vec3& vert1 = map.GetCollideVert(vert_index1);
			addVertex(vert1.x, vert1.z, vert1.y, vcap);

			uint32_t vert_index2 = collide_indices[index + 2];
			const glm::vec3& vert2 = map.GetCollideVert(vert_index2);
			addVertex(vert2.x, vert2.z, vert2.y, vcap);

			uint32_t vert_index3 = collide_indices[index + 1];
			const glm::vec3& vert3 = map.GetCollideVert(vert_index3);
			addVertex(vert3.x, vert3.z, vert3.y, vcap);

			addTriangle(counter, counter + 2, counter + 1, tcap);
		}

		//const auto& non_collide_indices = map.GetNonCollideIndices();

		//for (uint32_t index = 0; index < non_collide_indices.size(); index += 3, counter += 3)
		//{
		//	uint32_t vert_index1 = non_collide_indices[index];
		//	const glm::vec3& vert1 = map.GetNonCollideVert(vert_index1);
		//	addVertex(vert1.x, vert1.z, vert1.y, vcap);

		//	uint32_t vert_index2 = non_collide_indices[index + 1];
		//	const glm::vec3& vert2 = map.GetNonCollideVert(vert_index2);
		//	addVertex(vert2.x, vert2.z, vert2.y, vcap);

		//	uint32_t vert_index3 = non_collide_indices[index + 2];
		//	const glm::vec3& vert3 = map.GetNonCollideVert(vert_index3);
		//	addVertex(vert3.x, vert3.z, vert3.y, vcap);

		//	addTriangle(counter, counter + 2, counter + 1, tcap);
		//}
	}
	else
	{
		SetCurrentDirectoryA(orig_directory);
		return false;
	}
	
	//message = "Calculating Surface Normals...";
	m_normals = new float[m_triCount*3];
	for (int i = 0; i < m_triCount*3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i]*3];
		const float* v1 = &m_verts[m_tris[i+1]*3];
		const float* v2 = &m_verts[m_tris[i+2]*3];
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}
		float* n = &m_normals[i];
		n[0] = e0[1]*e1[2] - e0[2]*e1[1];
		n[1] = e0[2]*e1[0] - e0[0]*e1[2];
		n[2] = e0[0]*e1[1] - e0[1]*e1[0];
		float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			d = 1.0f/d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}
	
	strncpy(m_filename, zoneShortName, sizeof(m_filename));
	m_filename[sizeof(m_filename)-1] = '\0';
	SetCurrentDirectoryA(orig_directory);

	return true;
}
