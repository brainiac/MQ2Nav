
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

AreaEnvironment ParseAreaTag(std::string_view areaTag)
{
	if (areaTag.length() < 4)
		return {};

	if (areaTag[0] == 'A')
	{
		if (areaTag.starts_with("AWT"))
		{
			return AreaEnvironment::UnderWater;
		}

		if (areaTag.starts_with("ALV"))
		{
			return AreaEnvironment::UnderLava;
		}

		if (areaTag.starts_with("AVW"))
		{
			return AreaEnvironment::UnderIceWater;
		}

		if (areaTag.starts_with("APK"))
		{
			return AreaEnvironment::Kill;
		}

		if (areaTag.starts_with("ATP"))
		{
			// Old style teleport. No coordinates, just index.
			AreaEnvironment env = AreaEnvironment::TeleportIndex;
			env.teleportIndex = (int16_t)str_to_int(areaTag.substr(3), -1);

			return env;
		}

		if (areaTag.starts_with("ASL"))
		{
			return AreaEnvironment::Slippery;
		}
	}
	else
	{
		AreaEnvironment env;

		if (areaTag.starts_with("DR"))
		{
			env = AreaEnvironment::Type_None;
		}
		else if (areaTag.starts_with("WT"))
		{
			env = AreaEnvironment::UnderWater;
		}
		else if (areaTag.starts_with("LA"))
		{
			env = AreaEnvironment::UnderLava;
		}
		else if (areaTag.starts_with("SL"))
		{
			env = AreaEnvironment::UnderSlime;
		}
		else if (areaTag.starts_with("VW"))
		{
			env = AreaEnvironment::UnderIceWater;
		}
		else if (areaTag.starts_with("W2"))
		{
			env = AreaEnvironment::UnderWater2;
		}
		else if (areaTag.starts_with("W3"))
		{
			env = AreaEnvironment::UnderWater3;
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
				// An unused area type.
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
				// An unused area type
			}
		}

		return env;
	}

	return {};
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

//=================================================================================================

Terrain::Terrain()
	: m_bbMin(std::numeric_limits<float>::max())
	, m_bbMax(std::numeric_limits<float>::min())
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
			numFaces += region.regionSprite->numFaces;
			numVertices += region.regionSprite->numVertices;
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

		if (SDMSpriteDef2WLDData* pSprite = wldRegion.regionSprite.get())
		{
			if (!m_materialPalette && pSprite->materialPalette)
			{
				m_materialPalette = pSprite->materialPalette;
			}

			// Copy over vertices and indices
			for (uint32_t i = 0; i < pSprite->numVertices; ++i)
			{
				glm::vec3& outVertex = m_vertices[vertexOffset + i];
				glm::i16vec3* inVertex = pSprite->vertices + i;

				outVertex = glm::vec3(inVertex->x, inVertex->y, inVertex->z) * pSprite->vertexScaleFactor + pSprite->centerOffset;

				if (pSprite->numUVs)
				{
					if (pSprite->uvsOldForm)
					{
						glm::vec2& outUV = m_uvs[vertexOffset + i];
						glm::u16vec2* inUV = pSprite->uvsOldForm + i;

						outUV = glm::vec2(inUV->x, inUV->y) * S3D_UV_TO_FLOAT;
					}
					else
					{
						glm::vec2& outUV = m_uvs[vertexOffset + i];
						glm::vec2* inUV = pSprite->uvs + i;

						outUV = *inUV;
					}
				}
				else
				{
					m_uvs[vertexOffset + i] = glm::vec2(0.0f, 0.0f);
				}

				if (pSprite->numVertexNormals)
				{
					glm::vec3& outNormal = m_normals[vertexOffset + i];
					glm::u8vec3* inNormal = pSprite->vertexNormals + i;

					outNormal = glm::vec3(inNormal->x, inNormal->y, inNormal->z) * S3D_NORM_TO_FLOAT;
				}
				else
				{
					m_normals[vertexOffset + i] = glm::vec3(0.0f, 0.0f, 0.0f);
				}

				if (pSprite->numRGBs)
				{
					m_rgbColors[vertexOffset + i] = pSprite->rgbData[i];
				}
				else
				{
					m_rgbColors[vertexOffset + i] = 0xff1f1f1f;
				}

				m_rgbTints[vertexOffset + i] = 0xff808080;

				m_bbMin = glm::min(m_bbMin, outVertex);
				m_bbMax = glm::max(m_bbMax, outVertex);
			}

			// Initialize faces and materials
			WLD_MATERIALGROUP* faceGroups = pSprite->faceMaterialGroups;
			uint32_t numMaterials = pSprite->materialPalette->GetNumMaterials();

			uint32_t curGroup = 0;
			uint32_t groupSize = faceGroups[curGroup].group_size;
			int16_t materialIndex = faceGroups[curGroup].material_index;
			if (materialIndex > (int)numMaterials)
				materialIndex = -1;

			uint32_t groupStart = 0;
			Material* pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
				? pSprite->materialPalette->GetMaterial(materialIndex) : nullptr;

			for (uint32_t face = 0; face < pSprite->numFaces; ++face)
			{
				if (face > groupStart + groupSize)
				{
					// Increment to next group
					if (curGroup < pSprite->numFaceMaterialGroups - 1)
						++curGroup;

					groupSize = faceGroups[curGroup].group_size;
					materialIndex = faceGroups[curGroup].material_index;
					if (materialIndex > (int)numMaterials)
						materialIndex = -1;

					groupStart = face;

					pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
						? pSprite->materialPalette->GetMaterial(materialIndex) : nullptr;
				}

				SFace& outFace = m_faces[faceOffset + face];
				WLD_DMFACE2* inFace = pSprite->faces + face;

				outFace.indices = glm::uvec3{ inFace->indices[0], inFace->indices[1], inFace->indices[2] } + faceOffset;
				outFace.materialIndex = materialIndex;
				outFace.flags = 0
					| (inFace->flags & S3D_FACEFLAG_PASSABLE ? EQG_FACEFLAG_PASSABLE : 0)
					| (!pMaterial || pMaterial->IsTransparent() ? EQG_FACEFLAG_TRANSPARENT : 0);
			}

			vertexOffset += pSprite->numVertices;
			faceOffset += pSprite->numFaces;
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

} // namespace eqg
