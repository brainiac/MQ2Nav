
#include "pch.h"
#include "eqg_terrain.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_material.h"
#include "eqg_resource_manager.h"
#include "eqg_structs.h"
#include "log_internal.h"

namespace eqg {

//=================================================================================================

AreaEnvironment TypeToEnvironment(uint32_t type)
{
	AreaEnvironment env;

	switch (type & 0xff)
	{
	case 0: break;
	case 1: env.type = AreaEnvironment::UnderWater; break;
	case 2: env.type = AreaEnvironment::UnderSlime; break;
	case 3: env.type = AreaEnvironment::UnderLava; break;
	case 4: env.type = AreaEnvironment::UnderIceWater; break;
	case 5: env.type = AreaEnvironment::UnderWater; break;
	case 6: env.type = AreaEnvironment::UnderSlime; break;
	case 7: env.type = AreaEnvironment::UnderLava; break;
	case 8: env.type = AreaEnvironment::UnderIceWater; break;
	case 9: env.type = AreaEnvironment::UnderWater2; break;
	case 10: env.type = AreaEnvironment::UnderWater3; break;
	default: break;
	}

	if (type & 0x80000000)
		env.flags |= AreaEnvironment::Teleport;
	if (type & 0x40000000)
		env.flags |= AreaEnvironment::Kill;
	if (type & 0x20000000)
		env.flags |= AreaEnvironment::Mover;
	if (type & 0x10000000)
		env.flags |= AreaEnvironment::Slippery;
	if (type & 0x0C000000)
		env.flags |= AreaEnvironment::Pain;

	return env;
}

AreaEnvironment ParseAreaTag(std::string_view areaTag, uint32_t type = 0)
{
	if (areaTag.length() < 4)
		return {};

	AreaEnvironment inEnv = TypeToEnvironment(type);

	if (areaTag[0] == 'A')
	{
		if (areaTag.starts_with("AWT"))
		{
			return { AreaEnvironment::UnderWater, inEnv.flags };
		}

		if (areaTag.starts_with("ALV"))
		{
			return { AreaEnvironment::UnderLava, inEnv.flags };
		}

		if (areaTag.starts_with("AVW"))
		{
			return { AreaEnvironment::UnderIceWater, inEnv.flags };
		}

		if (areaTag.starts_with("APK"))
		{
			return inEnv | AreaEnvironment::Kill;
		}

		if (areaTag.starts_with("ATP"))
		{
			// Old style teleport. No coordinates, just index.
			AreaEnvironment env = inEnv | AreaEnvironment::TeleportIndex;
			env.teleportIndex = (int16_t)str_to_int(areaTag.substr(4), -1);

			return env;
		}

		if (areaTag.starts_with("ASL"))
		{
			return inEnv | AreaEnvironment::Slippery;
		}

		if (areaTag.starts_with("AFG"))
		{
			return { AreaEnvironment::Fog, inEnv.flags };
		}

		if (areaTag.starts_with("APV"))
		{
			return { AreaEnvironment::Portal, inEnv.flags };
		}
	}
	else
	{
		AreaEnvironment env = inEnv;

		if (areaTag.starts_with("DR"))
		{
			env.type = AreaEnvironment::Type_None;
		}
		else if (areaTag.starts_with("WT"))
		{
			env.type = AreaEnvironment::UnderWater;
		}
		else if (areaTag.starts_with("LA"))
		{
			env.type = AreaEnvironment::UnderLava;
		}
		else if (areaTag.starts_with("SL"))
		{
			env.type = AreaEnvironment::UnderSlime;
		}
		else if (areaTag.starts_with("VW"))
		{
			env.type = AreaEnvironment::UnderIceWater;
		}
		else if (areaTag.starts_with("W2"))
		{
			env.type = AreaEnvironment::UnderWater2;
		}
		else if (areaTag.starts_with("W3"))
		{
			env.type = AreaEnvironment::UnderWater3;
		}

		if (areaTag[2] == 'P')
		{
			env.flags |= AreaEnvironment::Kill;
		}

		if (areaTag.size() >= 5)
		{
			if (areaTag[3] == 'T' && areaTag[4] == 'P')
			{
				env.flags |= AreaEnvironment::Teleport; // Additional info is added elsewhere
			}
		}

		if (areaTag.size() >= 32)
		{
			if (areaTag[31] == 'M')
			{
				env.flags |= AreaEnvironment::Mover; // An unused area type?
			}
		}

		if (areaTag.size() >= 33)
		{
			if (areaTag[32] == 'S')
			{
				env.flags |= AreaEnvironment::Slippery;
			}
		}

		if (areaTag.size() >= 34)
		{
			if (areaTag[33] == 'P' || areaTag[33] == 'F')
			{
				env.flags |= AreaEnvironment::Pain; // An unused area type?
			}
		}

		return env;
	}

	return {};
}

AreaEnvironment::AreaEnvironment(std::string_view areaTag, uint32_t type)
{
	*this = ParseAreaTag(areaTag, type);
}


bool ParseAreaTeleportTag(std::string_view areaTag, AreaTeleport& teleport)
{
	if (areaTag.length() < 17)
		return false;

	teleport.tag = std::string(areaTag);
	teleport.teleportIndex = 0;

	std::string_view zoneId = areaTag.substr(5, 5); // [5-19)
	teleport.zoneId = str_to_int(zoneId, 0);

	std::string_view x = areaTag.substr(10, 6); // [10-16)
	if (areaTag.length() < 31 || teleport.zoneId == 255)
	{
		teleport.teleportIndex = str_to_int(x, 0);
		return true;
	}
	teleport.position.x = str_to_float(x, 0);

	std::string_view y = areaTag.substr(16, 6); // [16-22)
	teleport.position.y = str_to_float(y, 0);

	std::string_view z = areaTag.substr(22, 6); // [22-28)
	teleport.position.z = str_to_float(z, 0);

	std::string_view heading = areaTag.substr(28, 3); // [28-31)
	teleport.heading = str_to_float(heading, 0)/* * EQ_TO_RAD*/;

	return true;
}

TerrainArea::TerrainArea(const std::string_view& name, const glm::vec3& position,
	const glm::vec3& orientation, const glm::vec3& scale, uint32_t type)
	: name(name)
	, environment(name, type)
	, position(position)
	, orientation(orientation)
	, scale(scale)
{
	transform = glm::scale(glm::identity<glm::mat4x4>(), this->scale);
	transform = glm::translate(transform, this->position);
	transform *= glm::mat4_cast(glm::quat{ this->orientation });

	//area->transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), area->position), glm::vec3(area->extents));
	//area->transform *= glm::mat4_cast(glm::quat{ area->orientation });
}

//=================================================================================================

Terrain::Terrain()
{
}

Terrain::~Terrain()
{
}

bool Terrain::InitFromWLDData(const STerrainWLDData& wldData)
{
	uint32_t numFaces = 0;
	uint32_t numVertices = 0;
	uint32_t faceOffset = 0;
	uint32_t vertexOffset = 0;

	// Count the total number of vertices and faces.
	for (const SRegionWLDData& region : wldData.regions)
	{
		if (region.regionSprite)
		{
			numFaces += static_cast<uint32_t>(region.regionSprite->faces.size());
			numVertices += static_cast<uint32_t>(region.regionSprite->vertices.size());
		}
		else
		{
			//EQG_LOG_WARN("Found region '{}' with no sprite instance!", region.tag);
		}
	}

	m_vertices.resize(numVertices);
	m_uvs.resize(numVertices);
	m_normals.resize(numVertices);
	m_rgbColors.resize(numVertices);
	m_rgbTints.resize(numVertices);
	m_faces.resize(numFaces);

	m_numWLDRegions = (uint32_t)wldData.regions.size();

	//
	uint32_t numWLDAreas = (uint32_t)wldData.areas.size();
	m_wldAreas.resize(numWLDAreas);
	m_wldAreaIndices.resize(m_numWLDRegions);

	for (uint32_t areaNum = 0; areaNum < numWLDAreas; ++areaNum)
	{
		const SAreaWLDData& wldArea = wldData.areas[areaNum];
		if (wldArea.areaNum >= m_wldAreas.size())
		{
			EQG_LOG_WARN("Area num {} is out of range (size: {})",
				wldArea.areaNum, m_wldAreas.size());
			continue;
		}
		SArea& area = m_wldAreas[wldArea.areaNum];

		area.tag = wldArea.tag;
		area.userData = wldArea.userData;
		area.regionNumbers.resize(wldArea.numRegions);
		memcpy(area.regionNumbers.data(), wldArea.regions, sizeof(uint32_t) * wldArea.numRegions);

		for (uint32_t region : area.regionNumbers)
		{
			m_wldAreaIndices[region] = areaNum;
		}
	}

	uint32_t numWLDRegions = (uint32_t)wldData.regions.size();

	for (uint32_t regionNum = 0; regionNum < numWLDRegions; ++regionNum)
	{
		// Read the region number from the tag string and add this region's center to that area.
		const SRegionWLDData& wldRegion = wldData.regions[regionNum];
		if (wldRegion.tag.size() > 1)
		{
			int regionId = str_to_int(wldRegion.tag.substr(1), -1);
			if (regionId == -1)
			{
				EQG_LOG_WARN("Bad region tag {}", wldRegion.tag);
			}

			if (regionId >= 0 && regionId < (int)numWLDRegions && !m_wldAreas.empty())
			{
				uint32_t areaId = m_wldAreaIndices[regionId];
				if (areaId < m_wldAreas.size())
				{
					m_wldAreas[areaId].centers.push_back(wldRegion.sphereCenter);
				}
			}
		}

		if (SDMSpriteDef2WLDData* pDMSpriteDef2 = wldRegion.regionSprite.get())
		{
			if (!m_materialPalette && pDMSpriteDef2->materialPalette)
			{
				m_materialPalette = pDMSpriteDef2->materialPalette;
			}

			const glm::vec3 centerOffset = pDMSpriteDef2->centerOffset;
			const float scaleFactor = pDMSpriteDef2->vertexScaleFactor;

			for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
			{
				glm::vec3 newVert = centerOffset + glm::vec3(pDMSpriteDef2->vertices[vert]) * scaleFactor;
				m_vertices[vertexOffset + vert] = newVert;

				m_aabb.enclose(newVert);
			}

			if (pDMSpriteDef2->uvs.index() == 0)
			{
				std::span<glm::vec2>& uvs = std::get<0>(pDMSpriteDef2->uvs);

				if (!uvs.empty())
				{
					memcpy(m_uvs.data() + vertexOffset, uvs.data(), uvs.size_bytes());
				}
				else
				{
					memset(m_uvs.data() + vertexOffset, 0, sizeof(glm::vec2) * pDMSpriteDef2->vertices.size());
				}
			}
			else
			{
				std::span<glm::i16vec2>& uvs = std::get<1>(pDMSpriteDef2->uvs);

				if (!uvs.empty())
				{
					for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
					{
						m_uvs[vertexOffset + vert] = glm::vec2(uvs[vert]) * S3D_UV_TO_FLOAT;
					}
				}
				else
				{
					memset(m_uvs.data() + vertexOffset, 0, sizeof(glm::vec2) * pDMSpriteDef2->vertices.size());
				}
			}

			if (pDMSpriteDef2->vertexNormals.empty())
			{
				memset(m_normals.data() + vertexOffset, 0, sizeof(glm::vec3) * pDMSpriteDef2->vertices.size());
			}
			else
			{
				for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
				{
					m_normals[vertexOffset + vert] = glm::vec3(pDMSpriteDef2->vertexNormals[vert]) * S3D_NORM_TO_FLOAT;
				}
			}

			if (pDMSpriteDef2->rgbData.empty())
			{
				for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
				{
					m_rgbColors[vertexOffset + vert] = 0xff1f1f1f;
				}
			}
			else
			{
				for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
				{
					m_rgbColors[vertexOffset + vert] = pDMSpriteDef2->rgbData[vert];
				}
			}

			for (size_t vert = 0; vert < pDMSpriteDef2->vertices.size(); ++vert)
			{
				m_rgbTints[vertexOffset + vert] = 0xff808080;
			}

			// Initialize faces and materials
			WLD_MATERIALGROUP* faceGroups = pDMSpriteDef2->faceMaterialGroups.data();
			uint32_t numMaterials = pDMSpriteDef2->materialPalette->GetNumMaterials();

			uint32_t curGroup = 0;
			uint32_t groupSize = faceGroups[curGroup].group_size;
			int16_t materialIndex = faceGroups[curGroup].material_index;
			if (materialIndex > (int)numMaterials)
				materialIndex = -1;

			uint32_t groupStart = 0;
			Material* pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
				? pDMSpriteDef2->materialPalette->GetMaterial(materialIndex) : nullptr;

			for (uint32_t face = 0; face < static_cast<uint32_t>(pDMSpriteDef2->faces.size()); ++face)
			{
				if (face > groupStart + groupSize)
				{
					// Increment to next group
					if (curGroup < pDMSpriteDef2->faceMaterialGroups.size() - 1)
						++curGroup;

					groupSize = faceGroups[curGroup].group_size;
					materialIndex = faceGroups[curGroup].material_index;
					if (materialIndex > (int)numMaterials)
						materialIndex = -1;

					groupStart = face;

					pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
						? pDMSpriteDef2->materialPalette->GetMaterial(materialIndex) : nullptr;
				}

				SFace& outFace = m_faces[faceOffset + face];
				WLD_DMFACE2& inFace = pDMSpriteDef2->faces[face];

				outFace.indices = glm::uvec3{ inFace.indices[2], inFace.indices[1], inFace.indices[0] } + vertexOffset;
				outFace.materialIndex = materialIndex;
				outFace.flags = 0
					| (inFace.flags & S3D_FACEFLAG_PASSABLE ? EQG_FACEFLAG_PASSABLE : 0)
					| (!pMaterial || pMaterial->IsTransparent() ? EQG_FACEFLAG_TRANSPARENT : 0);
			}

			vertexOffset += static_cast<uint32_t>(pDMSpriteDef2->vertices.size());
			faceOffset += static_cast<uint32_t>(pDMSpriteDef2->faces.size());
		}
	}

	// Assign areas and teleports
	if (!m_wldAreas.empty())
	{
		m_wldAreaEnvironments.resize(m_numWLDRegions);

		for (const SArea& area : m_wldAreas)
		{
			std::string_view areaTag = !area.userData.empty() ? area.userData : area.tag;

			AreaEnvironment env = areaTag;
			if (env.flags & AreaEnvironment::Teleport)
			{
				AreaTeleport teleport;
				if (ParseAreaTeleportTag(areaTag, teleport))
				{
					if (teleport.teleportIndex != 0)
					{
						env.flags |= AreaEnvironment::TeleportIndex;
						env.flags &= (AreaEnvironment::Flags)~AreaEnvironment::Teleport;
					}
					else if (teleport.zoneId == 255)
					{
						env.flags |= AreaEnvironment::TeleportIndex;
						env.flags &= (AreaEnvironment::Flags)~AreaEnvironment::Teleport;
					}
					else
					{
						teleport.teleportIndex = (int)m_teleports.size();
						m_teleports.push_back(teleport);
					}

					env.teleportIndex = (int16_t)teleport.teleportIndex;
				}
				else
				{
					EQG_LOG_WARN("Failed to parse teleport tag: {}", areaTag);
				}
			}

			for (uint32_t regionNum : area.regionNumbers)
			{
				m_wldAreaEnvironments[regionNum] = env;
			}
		}
	}

	m_wldBspTree = wldData.worldTree;
	m_constantAmbientColor = wldData.constantAmbientColor;

	return true;
}

bool Terrain::InitFromEQGData(
	const std::span<SEQMVertex>& vertices,
	const std::span<SEQMFace>& faces,
	const std::span<uint32_t>& RGBs,
	const MaterialPalettePtr& materialPalette)
{
	m_fromEQG = true;
	m_materialPalette = materialPalette;

	m_vertices.resize(m_vertexOffset + vertices.size());
	m_uvs.resize(m_vertexOffset + vertices.size());
	m_uvs2.resize(m_vertexOffset + vertices.size());
	m_normals.resize(m_vertexOffset + vertices.size());
	m_rgbColors.resize(m_vertexOffset + vertices.size());
	m_rgbTints.resize(m_vertexOffset + vertices.size());
	m_faces.resize(m_faceOffset + faces.size());

	for (uint32_t index = 0; index < vertices.size(); ++index)
	{
		SEQMVertex& inVertex = vertices[index];

		m_vertices[m_vertexOffset + index] = inVertex.pos;
		m_normals[m_vertexOffset + index] = inVertex.normal;
		m_uvs[m_vertexOffset + index] = inVertex.uv;
		m_uvs2[m_vertexOffset + index] = inVertex.uv2;

		if (!RGBs.empty())
		{
			m_rgbColors[m_vertexOffset + index] = RGBs[index];
		}
		else
		{
			m_rgbColors[m_vertexOffset + index] = 0x001f1f1f;
		}

		m_rgbTints[m_vertexOffset + index] = inVertex.color;
		m_aabb.enclose(inVertex.pos);
	}

	for (uint32_t index = 0; index < faces.size(); ++index)
	{
		SFace& outFace = m_faces[m_faceOffset + index];
		SEQMFace& inFace = faces[index];

		outFace.flags = inFace.flags & 0xffff;
		outFace.materialIndex = static_cast<uint16_t>(inFace.material);
		outFace.indices = { inFace.vertices[0], inFace.vertices[1], inFace.vertices[2] };
	}

	m_vertexOffset = static_cast<uint32_t>(m_vertices.size());
	m_faceOffset = static_cast<uint32_t>(m_faces.size());

	// TOOD: Calculate tangents and binormals
	
	return true;
}

void Terrain::InitAreasFromEQGData(const std::span<SZONArea>& zonAreas, const char* stringPool)
{
	for (const SZONArea& area : zonAreas)
	{
		auto newArea = std::make_shared<TerrainArea>(stringPool + area.name,
			area.center, area.orientation, area.extents);

		m_areas.push_back(std::move(newArea));
	}
}

} // namespace eqg
