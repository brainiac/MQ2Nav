#pragma once

#include "eqg_geometry.h"
#include "eqg_structs.h"
#include "eqg_types_fwd.h"

namespace eqg {

//=================================================================================================

struct STerrainWLDData;

// Environment Types parsed from WLD and EQG areas
struct AreaEnvironment
{
	// Type of environment (water, lava, etc.)
	enum Type : uint8_t
	{
		Type_None,
		UnderWater,
		UnderSlime,
		UnderLava,
		UnderIceWater,
		UnderWater2,
		UnderWater3,
	};

	// Flags on the environment
	enum Flags : uint8_t
	{
		Flags_None = 0,
		Slippery = 0x04,
		Kill = 0x10,
		Teleport = 0x20,
		TeleportIndex = 0x40,
	};

	Type      type;
	Flags     flags;
	int16_t   teleportIndex;

	AreaEnvironment(std::string_view areaTag);
	AreaEnvironment() : type(Type_None), flags(Flags_None), teleportIndex(0) {}
	AreaEnvironment(Type env) : type(env), flags(Flags_None), teleportIndex(0) {}
	AreaEnvironment(Flags flags) : type(Type_None), flags(flags), teleportIndex(0) {}
	AreaEnvironment(Flags flags, uint16_t teleportIndex) : type(Type_None), flags(flags), teleportIndex(teleportIndex) {}
	AreaEnvironment(Type env, Flags flags) : type(env), flags(flags), teleportIndex(0) {}
	AreaEnvironment(Type env, Flags flags, uint16_t teleportIndex) : type(env), flags(flags), teleportIndex(teleportIndex) {}

	// Combines another environment with this one to produce a new environment with flags mixed
	AreaEnvironment operator|(AreaEnvironment other) const;
};
static_assert(sizeof(AreaEnvironment) == 4);

AreaEnvironment ParseAreaTag(std::string_view areaTag);

inline AreaEnvironment::AreaEnvironment(std::string_view areaTag)
{
	*this = ParseAreaTag(areaTag);
}

inline AreaEnvironment::Flags operator|(AreaEnvironment::Flags a, AreaEnvironment::Flags b) { return static_cast<AreaEnvironment::Flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
inline AreaEnvironment::Flags operator|=(AreaEnvironment::Flags& a, AreaEnvironment::Flags b) { return a = a | b; }
inline AreaEnvironment::Flags operator&(AreaEnvironment::Flags a, AreaEnvironment::Flags b) { return static_cast<AreaEnvironment::Flags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b)); }
inline AreaEnvironment::Flags operator&=(AreaEnvironment::Flags& a, AreaEnvironment::Flags b) { return a = a & b; }

inline AreaEnvironment AreaEnvironment::operator|(AreaEnvironment other) const
{
	// Other environment type replacees, but flags mix.
	return AreaEnvironment(other.type, flags | other.flags, other.teleportIndex ? other.teleportIndex : 0);
}

struct AreaTeleport
{
	std::string tag;
	int teleportIndex;

	glm::vec3 position;
	float heading;
	int zoneId;
};
bool ParseAreaTeleportTag(std::string_view areaTag, AreaTeleport& teleport);

struct SArea
{
	std::string tag;
	std::string userData;
	std::vector<uint32_t> regionNumbers;
	std::vector<glm::vec3> centers;
};

class Terrain
{
public:
	Terrain();
	virtual ~Terrain();

	bool InitFromWLDData(const STerrainWLDData& wldData);

private:
	std::vector<glm::vec3> m_vertices;
	std::vector<glm::vec2> m_uvs;
	std::vector<glm::vec3> m_normals;
	std::vector<uint32_t>  m_rgbColors;
	std::vector<uint32_t>  m_rgbTints;
	std::vector<SFace>     m_faces;
	glm::vec3              m_bbMin;
	glm::vec3              m_bbMax;
	std::shared_ptr<MaterialPalette> m_materialPalette;
	uint32_t               m_constantAmbientColor = 0;

	// WLD/S3D data
	uint32_t               m_numWLDRegions = 0;
	std::vector<SArea>     m_wldAreas;
	std::vector<uint32_t>  m_wldAreaIndices;
	std::vector<AreaEnvironment> m_wldAreaEnvironments;
	std::shared_ptr<SWorldTreeWLDData> m_wldBspTree;

	// Teleports
	std::vector<AreaTeleport> m_teleports;
};

} // namespace eqglib
