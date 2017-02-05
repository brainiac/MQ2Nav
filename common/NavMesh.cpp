//
// NavMesh.cpp
//

#include "NavMesh.h"
#include "common/Enum.h"
#include "common/Utilities.h"
#include "common/proto/NavMeshFile.pb.h"

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <glm/gtc/type_ptr.hpp>

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include <fstream>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

//============================================================================

NavMesh::NavMesh(Context* context, const std::string& dataFolder, const std::string& zoneName)
	: m_ctx(context)
	, m_navMeshDirectory(dataFolder)
	, m_zoneName(zoneName)
{
	UpdateDataFile();
}

NavMesh::~NavMesh()
{
}

//----------------------------------------------------------------------------

void NavMesh::SetZoneName(const std::string& zoneShortName)
{
	if (m_zoneName == zoneShortName)
		return;

	ResetNavMesh();

	if (zoneShortName.empty() || zoneShortName == "UNKNOWN_ZONE")
	{
		m_zoneName.clear();
		m_dataFile.clear();
		m_ctx->Log(LogLevel::INFO, "Clearing current zone", m_zoneName.c_str());

		return;
	}

	m_zoneName = zoneShortName;

	UpdateDataFile();

	m_ctx->Log(LogLevel::INFO, "Setting navmesh zone to '%s'", m_zoneName.c_str());
}

void NavMesh::SetNavMeshDirectory(const std::string& dirname)
{
	if (m_navMeshDirectory != dirname)
	{
		m_navMeshDirectory = dirname;
		m_ctx->Log(LogLevel::DEBUG, "Navmesh data folder: ", m_navMeshDirectory.c_str());

		UpdateDataFile();
	}
}

void NavMesh::UpdateDataFile()
{
	if (!m_navMeshDirectory.empty() && !m_zoneName.empty())
	{
		fs::path meshpath = m_navMeshDirectory;
		meshpath /= m_zoneName + std::string(NAVMESH_FILE_EXTENSION);

		m_dataFile = meshpath.string();
	}
	else
	{
		m_dataFile.clear();
	}
}

void NavMesh::SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh, bool reset)
{
	if (navMesh == m_navMesh)
		return;

	m_navMesh = navMesh;
	m_navMeshQuery.reset();
	m_lastLoadResult = LoadResult::None;

	if (reset)
	{
		m_boundsMin = m_boundsMax = glm::vec3();
		m_config = NavMeshConfig{};
	}
}

void NavMesh::ResetNavMesh()
{
	SetNavMesh(nullptr, true);

	OnNavMeshChanged();
}

std::shared_ptr<dtNavMeshQuery> NavMesh::GetNavMeshQuery()
{
	if (!m_navMeshQuery)
	{
		if (m_navMesh)
		{
			auto query = std::shared_ptr<dtNavMeshQuery>(dtAllocNavMeshQuery(),
				[](dtNavMeshQuery* ptr) { dtFreeNavMeshQuery(ptr); });

			dtStatus status = query->init(m_navMesh.get(), NAVMESH_QUERY_MAX_NODES);
			if (dtStatusFailed(status))
			{
				m_ctx->Log(LogLevel::ERROR, "GetNavMeshQuery: Could not init detour navmesh query");
			}
			else
			{
				m_navMeshQuery = query;
			}
		}
	}

	return m_navMeshQuery;
}

void NavMesh::SetNavMeshBounds(const glm::vec3& min, const glm::vec3& max)
{
	m_boundsMin = min;
	m_boundsMax = max;
}

void NavMesh::GetNavMeshBounds(glm::vec3& min, glm::vec3& max)
{
	min = m_boundsMin;
	max = m_boundsMax;
}

//----------------------------------------------------------------------------

static void ToProto(nav::vector3& out_proto, const glm::vec3& v3)
{
	out_proto.set_x(v3.x);
	out_proto.set_y(v3.y);
	out_proto.set_z(v3.z);
}

static void FromProto(const nav::vector3& in_proto, glm::vec3& out_vec)
{
	out_vec = glm::vec3{ in_proto.x(), in_proto.y(), in_proto.z() };
}

inline glm::vec3 FromProto(const nav::vector3& in_proto)
{
	return glm::vec3{ in_proto.x(), in_proto.y(), in_proto.z() };
}

static void ToProto(nav::dtNavMeshParams& out_proto, const dtNavMeshParams* params)
{
	out_proto.set_tile_width(params->tileWidth);
	out_proto.set_tile_height(params->tileHeight);
	out_proto.set_max_tiles(params->maxTiles);
	out_proto.set_max_polys(params->maxPolys);
	ToProto(*out_proto.mutable_origin(), glm::make_vec3(params->orig));
}

static void FromProto(dtNavMeshParams& out_params, const nav::dtNavMeshParams& proto)
{
	out_params.tileWidth = proto.tile_width();
	out_params.tileHeight = proto.tile_height();
	out_params.maxTiles = proto.max_tiles();
	out_params.maxPolys = proto.max_polys();

	glm::vec3 orig = FromProto(proto.origin());
	dtVcopy(out_params.orig, glm::value_ptr(orig));
}

static void ToProto(google::protobuf::RepeatedPtrField<nav::NavMeshTile>& tiles, const dtNavMesh* mesh)
{
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize) continue;

		nav::NavMeshTile* ptile = tiles.Add();
		ptile->set_tile_ref(mesh->getTileRef(tile));
		ptile->set_tile_data(tile->data, tile->dataSize);
	}
}

static void ToProto(nav::BuildSettings& out_proto, const NavMeshConfig& config)
{
	out_proto.set_config_version(config.configVersion);
	out_proto.set_tile_size(config.tileSize);
	out_proto.set_cell_size(config.cellSize);
	out_proto.set_cell_height(config.cellHeight);
	out_proto.set_agent_height(config.agentHeight);
	out_proto.set_agent_radius(config.agentRadius);
	out_proto.set_agent_max_climb(config.agentMaxClimb);
	out_proto.set_agent_max_slope(config.agentMaxSlope);
	out_proto.set_region_min_size(config.regionMinSize);
	out_proto.set_region_merge_size(config.regionMergeSize);
	out_proto.set_edge_max_len(config.edgeMaxLen);
	out_proto.set_edge_max_error(config.edgeMaxError);
	out_proto.set_verts_per_poly(config.vertsPerPoly);
	out_proto.set_detail_sample_dist(config.detailSampleDist);
	out_proto.set_detail_sample_max_error(config.detailSampleMaxError);
	out_proto.set_partition_type(static_cast<int>(config.partitionType));
}

static void FromProto(const nav::BuildSettings& proto, NavMeshConfig& config)
{
	// compare the config version?
	config.configVersion = proto.config_version();

	config.tileSize = proto.tile_size();
	config.cellSize = proto.cell_size();
	config.cellHeight = proto.cell_height();
	config.agentHeight = proto.agent_height();
	config.agentRadius = proto.agent_radius();
	config.agentMaxClimb = proto.agent_max_climb();
	config.agentMaxSlope = proto.agent_max_slope();
	config.regionMinSize = proto.region_min_size();
	config.regionMergeSize = proto.region_merge_size();
	config.edgeMaxLen = proto.edge_max_len();
	config.edgeMaxError = proto.edge_max_error();
	config.vertsPerPoly = proto.verts_per_poly();
	config.detailSampleDist = proto.detail_sample_dist();
	config.detailSampleMaxError = proto.detail_sample_max_error();
	config.partitionType = static_cast<PartitionType>(proto.partition_type());
}

NavMesh::LoadResult NavMesh::LoadNavMeshFile()
{
	if (m_dataFile.empty())
	{
		return LoadResult::MissingFile;
	}

	m_lastLoadResult = LoadMesh(m_dataFile.c_str());
	OnNavMeshChanged();

	return m_lastLoadResult;
}

NavMesh::LoadResult NavMesh::LoadMesh(const char* filename)
{
	// cache the filename of the file we tried to load
	m_dataFile = filename;

	FILE* file = 0;
	errno_t err = fopen_s(&file, filename, "rb");
	if (err == ENOENT)
		return LoadResult::MissingFile;
	else if (!file)
		return LoadResult::Corrupt;

	scope_guard g = [file]() { fclose(file); };
	
	// read the whole file
	size_t filesize = 0;
	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	rewind(file);

	if (filesize <= sizeof(MeshFileHeader))
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: mesh file is not a valid mesh file");
		return LoadResult::Corrupt;
	}

	std::unique_ptr<char[]> buffer(new char[filesize]);
	size_t result = fread(buffer.get(), filesize, 1, file);
	if (result != 1)
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: failed to read contents of mesh file");
		return LoadResult::Corrupt;
	}

	char* data_ptr = buffer.get();
	size_t data_size = filesize;

	// read header
	MeshFileHeader* fileHeader = (MeshFileHeader*)data_ptr;
	data_ptr += sizeof(MeshFileHeader); data_size -= sizeof(MeshFileHeader);

	if (fileHeader->magic != NAVMESH_FILE_MAGIC)
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: mesh file is not a valid mesh file");
		return LoadResult::Corrupt;
	}

	if (fileHeader->version != NAVMESH_FILE_VERSION)
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: mesh file has an incompatible version number");
		return LoadResult::VersionMismatch;
	}

	bool compressed = +(fileHeader->flags & NavMeshFileFlags::COMPRESSED) != 0;
	nav::NavMeshFile file_proto;

	if (compressed)
	{
		std::vector<uint8_t> data;
		if (!DecompressMemory(data_ptr, data_size, data))
		{
			m_ctx->Log(LogLevel::ERROR, "loadMesh: failed to decompress mesh file");
			return LoadResult::Corrupt;
		}

		buffer.reset();

		if (!file_proto.ParseFromArray(&data[0], (int)data.size()))
		{
			m_ctx->Log(LogLevel::ERROR, "loadMesh: failed to parse mesh file");
			return LoadResult::Corrupt;
		}
	}
	else
	{
		if (!file_proto.ParseFromArray(data_ptr, data_size))
		{
			m_ctx->Log(LogLevel::ERROR, "loadMesh: failed to parse mesh file");
			return LoadResult::Corrupt;
		}
	}

	if (file_proto.zone_short_name() != m_zoneName)
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: zone name mismatch! mesh is for '%s'",
			file_proto.zone_short_name());
		return LoadResult::ZoneMismatch;
	}

	// read the tileset
	const nav::NavMeshTileSet& tileset = file_proto.tile_set();

	if (tileset.compatibility_version() == NAVMESH_TILE_COMPAT_VERSION)
	{
		dtNavMeshParams params;
		FromProto(params, tileset.mesh_params());

		std::shared_ptr<dtNavMesh> navMesh(dtAllocNavMesh(),
			[](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });

		dtStatus status = navMesh->init(&params);
		if (status == DT_SUCCESS)
		{
			// read the mesh tiles and add them to the navmesh one by one.
			for (const nav::NavMeshTile& tile : tileset.tiles())
			{
				dtTileRef ref = tile.tile_ref();
				const std::string& tiledata = tile.tile_data();

				if (ref == 0 || tiledata.length() == 0)
					continue;

				// allocate buffer for the data
				uint8_t* data = (uint8_t*)dtAlloc((int)tiledata.length(), DT_ALLOC_PERM);
				memcpy(data, &tiledata[0], tiledata.length());

				dtMeshHeader* tileheader = (dtMeshHeader*)data;

				dtStatus status = navMesh->addTile(data, (int)tiledata.length(), DT_TILE_FREE_DATA, ref, 0);
				if (status != DT_SUCCESS)
				{
					m_ctx->Log(LogLevel::WARNING, "Failed to read tile: %d, %d (%d) = %d",
						tileheader->x, tileheader->y, tileheader->layer, status);
				}
			}

			m_navMesh = std::move(navMesh);
		}
		else
		{
			m_ctx->Log(LogLevel::ERROR, "loadMesh: failed to initialize navmesh, will continue loading without tiles.");
		}
	}
	else
	{
		m_ctx->Log(LogLevel::ERROR, "loadMesh: navmesh has incompatible structure, will continue loading without tiles.");
	}

	m_navMeshQuery.reset();

	// load build settings
	FromProto(file_proto.build_settings(), m_config);

	m_boundsMin = FromProto(file_proto.build_settings().bounds_min());
	m_boundsMax = FromProto(file_proto.build_settings().bounds_max());

	return LoadResult::Success;
}

bool NavMesh::SaveNavMeshFile()
{
	if (m_dataFile.empty())
	{
		return false;
	}

	return SaveMesh(m_dataFile.c_str());
}

bool NavMesh::SaveMesh(const char* filename)
{
	if (!m_navMesh)
	{
		return false;
	}

	std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
	if (!outfile.is_open())
		return false;

	// todo: Configuration
	bool compress = true;

	// Build the NavMeshFile proto

	nav::NavMeshFile file_proto;
	file_proto.set_zone_short_name(m_zoneName);

	// save the navmesh data
	nav::NavMeshTileSet* tileset = file_proto.mutable_tile_set();
	tileset->set_compatibility_version(NAVMESH_TILE_COMPAT_VERSION);
	ToProto(*tileset->mutable_mesh_params(), m_navMesh->getParams());
	ToProto(*tileset->mutable_tiles(), m_navMesh.get());

	// save build settings
	ToProto(*file_proto.mutable_build_settings(), m_config);
	ToProto(*file_proto.mutable_build_settings()->mutable_bounds_min(), m_boundsMin);
	ToProto(*file_proto.mutable_build_settings()->mutable_bounds_max(), m_boundsMax);

	// todo: save convex volumes
	// todo: save offmesh connections
	// todo: save area definitions

	// Store header.
	MeshFileHeader header;
	header.magic = NAVMESH_FILE_MAGIC;
	header.version = NAVMESH_FILE_VERSION;
	header.flags = NavMeshFileFlags{};

	if (compress) header.flags |= NavMeshFileFlags::COMPRESSED;

	outfile.write((const char*)&header, sizeof(MeshFileHeader));

	if (compress)
	{
		std::string buffer;
		file_proto.SerializeToString(&buffer);

		std::vector<uint8_t> data;
		CompressMemory(&buffer[0], buffer.length(), data);

		outfile.write((const char*)&data[0], data.size());
	}
	else
	{
		file_proto.SerializeToOstream(&outfile);
	}

	outfile.close();
	return true;
}

//============================================================================
