//
// NavMesh.cpp
//

#include "NavMesh.h"

#include "common/Enum.h"
#include "common/JsonProto.h"
#include "common/Utilities.h"
#include "common/proto/NavMeshFile.pb.h"

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/json_util.h>
#include <glm/gtc/type_ptr.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <spdlog/spdlog.h>

#include <DebugDraw.h>
#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>
#include <Recast.h>

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

//============================================================================

NavMesh::NavMesh(const std::string& dataFolder, const std::string& zoneName)
	: m_navMeshDirectory(dataFolder)
	, m_zoneName(zoneName)
{
	UpdateDataFile();
	InitializeAreas();
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

		SPDLOG_DEBUG("Clearing current zone: {}", m_zoneName);
		return;
	}

	m_zoneName = zoneShortName;

	UpdateDataFile();
	SPDLOG_DEBUG("Setting navmesh zone to: {}", m_zoneName);
}

void NavMesh::SetNavMeshDirectory(const std::string& dirname)
{
	if (m_navMeshDirectory != dirname)
	{
		m_navMeshDirectory = dirname;
		SPDLOG_DEBUG("Navmesh data folder: {}", m_navMeshDirectory);

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

	if (reset)
	{
		ResetSavedData();
	}

	m_navMesh = navMesh;
	m_navMeshQuery.reset();
	m_lastLoadResult = LoadResult::None;
}

void NavMesh::ResetSavedData(PersistedDataFields fields)
{
	if (+(fields & PersistedDataFields::BuildSettings))
	{
		m_boundsMin = m_boundsMax = glm::vec3();
		m_config = NavMeshConfig{};
	}

	if (+(fields & PersistedDataFields::MeshTiles))
	{
		m_navMesh.reset();
		m_navMeshQuery.reset();
	}

	if (+(fields & PersistedDataFields::AreaTypes))
	{
		InitializeAreas();
	}

	if (+(fields & PersistedDataFields::ConvexVolumes))
	{
		m_volumes.clear();
		m_volumesById.clear();
		m_nextVolumeId = 1;
	}

	if (+(fields & PersistedDataFields::Connections))
	{
		m_connections.clear();
		m_connectionsById.clear();
		m_nextConnectionId = 1;
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
				SPDLOG_ERROR("GetNavMeshQuery: Could not init detour nav mesh query");
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

static void ToProto(nav::color& out_proto, uint32_t color)
{
	out_proto.set_a((color >> 24) & 0xff);
	out_proto.set_r((color >> 16) & 0xff);
	out_proto.set_g((color >> 8) & 0xff);
	out_proto.set_b(color & 0xff);
}

inline uint32_t FromProto(const nav::color& proto)
{
	return ((proto.a() & 0xff) << 24)
		| ((proto.r() & 0xff) << 16)
		| ((proto.g() & 0xff) << 8)
		| (proto.b() & 0xff);
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

static void ToProto(nav::ConvexVolume& out_proto, const ConvexVolume& volume)
{
	out_proto.set_area_type(static_cast<uint32_t>(volume.areaType));
	out_proto.set_height_min(volume.hmin);
	out_proto.set_height_max(volume.hmax);
	out_proto.set_name(volume.name);
	out_proto.set_id(volume.id);

	for (const auto& vert : volume.verts)
	{
		nav::vector3* out_vert = out_proto.add_vertices();
		ToProto(*out_vert, vert);
	}
}

static std::unique_ptr<ConvexVolume> FromProto(const nav::ConvexVolume& proto)
{
	auto volume = std::make_unique<ConvexVolume>();
	
	volume->areaType = proto.area_type();
	volume->hmin = proto.height_min();
	volume->hmax = proto.height_max();
	volume->name = proto.name();
	volume->id = proto.id();

	for (const auto& vert : proto.vertices())
	{
		volume->verts.push_back(FromProto(vert));
	}

	return volume;
}

static void ToProto(nav::Connection& out_proto, const OffMeshConnection& connection)
{
	out_proto.set_id(connection.id);
	ToProto(*out_proto.mutable_pos_from(), connection.start);
	ToProto(*out_proto.mutable_pos_to(), connection.end);
	out_proto.set_type(static_cast<uint32_t>(connection.type));
	out_proto.set_area_type(connection.areaType);
	out_proto.set_one_way(!connection.bidirectional);
	out_proto.set_name(connection.name);
}

static std::unique_ptr<OffMeshConnection> FromProto(const nav::Connection& proto)
{
	auto connection = std::make_unique<OffMeshConnection>();

	connection->id = proto.id();
	connection->start = FromProto(proto.pos_from());
	connection->end = FromProto(proto.pos_to());
	connection->type = static_cast<ConnectionType>(proto.type());
	connection->areaType = proto.area_type();
	connection->name = proto.name();
	connection->bidirectional = !proto.one_way();

	return connection;
}

static void ToProto(nav::PolyAreaType& out_proto, const PolyAreaType& area)
{
	out_proto.set_id(area.id);
	out_proto.set_name(area.name);
	ToProto(*out_proto.mutable_color(), area.color);
	out_proto.set_flags(area.flags);
	out_proto.set_cost(area.cost);
}

static void FromProto(const nav::PolyAreaType& in_proto, PolyAreaType& area)
{
	area.id = static_cast<uint8_t>(in_proto.id());
	area.name = in_proto.name();
	area.color = FromProto(in_proto.color());
	area.flags = static_cast<uint16_t>(in_proto.flags());
	area.cost = in_proto.cost();
	area.valid = true;
}

void NavMesh::LoadFromProto(const nav::NavMeshFile& proto, PersistedDataFields fields)
{
	if (+(fields & PersistedDataFields::MeshTiles))
	{
		// read the tileset
		const nav::NavMeshTileSet& tileset = proto.tile_set();

		if (tileset.compatibility_version() == NAVMESH_TILE_COMPAT_VERSION)
		{
			dtNavMeshParams params;
			FromProto(params, tileset.mesh_params());

			std::shared_ptr<dtNavMesh> navMesh(dtAllocNavMesh(),
				[](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });

			// would prefer to have proper origin, but this can fix it up too.
			params.orig[0] = proto.build_settings().bounds_min().x();
			params.orig[1] = proto.build_settings().bounds_min().y();
			params.orig[2] = proto.build_settings().bounds_min().z();

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

					dtStatus status = navMesh->addTile(data, (int)tiledata.length(), DT_TILE_FREE_DATA, ref, nullptr);
					if (status != DT_SUCCESS)
					{
						SPDLOG_WARN("Failed to read tile: {}, {} ({}) = {}",
							tileheader->x, tileheader->y, tileheader->layer, status);
					}
				}

				m_navMesh = std::move(navMesh);
			}
			else
			{
				SPDLOG_ERROR("loadMesh: failed to initialize navmesh, will continue loading without tiles.");
			}
		}
		else
		{
			SPDLOG_ERROR("loadMesh: navmesh has incompatible structure, will continue loading without tiles.");
		}
	}

	if (+(fields & PersistedDataFields::BuildSettings))
	{
		// load build settings
		FromProto(proto.build_settings(), m_config);

		m_boundsMin = FromProto(proto.build_settings().bounds_min());
		m_boundsMax = FromProto(proto.build_settings().bounds_max());
	}

	if (+(fields & PersistedDataFields::AreaTypes))
	{
		// load areas
		for (const auto& proto_area : proto.areas())
		{
			PolyAreaType area;
			FromProto(proto_area, area);

			UpdateArea(area);
		}
	}

	if (+(fields & PersistedDataFields::ConvexVolumes))
	{
		// load convex volumes
		for (const auto& proto_volume : proto.convex_volumes())
		{
			std::unique_ptr<ConvexVolume> vol = FromProto(proto_volume);
			m_volumesById.emplace(vol->id, vol.get());
			m_volumes.push_back(std::move(vol));
		}

		// find the next volume id
		auto result =
			std::max_element(std::begin(m_volumes), std::end(m_volumes),
				[](const auto& l, const auto& r) { return l->id < r->id; });

		m_nextVolumeId = (result != std::end(m_volumes) ? (*result)->id : 0) + 1;
	}

	if (+(fields & PersistedDataFields::Connections))
	{
		// load connections
		for (const auto& proto_conn : proto.connections())
		{
			std::unique_ptr<OffMeshConnection> conn = FromProto(proto_conn);
			m_connectionsById.emplace(conn->id, conn.get());
			m_connections.push_back(std::move(conn));
		}

		// find the next connection id
		auto result =
			std::max_element(std::begin(m_connections), std::end(m_connections),
				[](const auto& l, const auto& r) { return l->id < r->id; });

		m_nextConnectionId = (result != std::end(m_connections) ? (*result)->id : 0) + 1;
	}
}

void NavMesh::SaveToProto(nav::NavMeshFile& proto, PersistedDataFields fields)
{
	if (+(fields & PersistedDataFields::BuildSettings))
	{
		// save build settings
		ToProto(*proto.mutable_build_settings(), m_config);
		ToProto(*proto.mutable_build_settings()->mutable_bounds_min(), m_boundsMin);
		ToProto(*proto.mutable_build_settings()->mutable_bounds_max(), m_boundsMax);
	}

	if (+(fields & PersistedDataFields::MeshTiles))
	{
		// save the navmesh data
		nav::NavMeshTileSet* tileset = proto.mutable_tile_set();
		tileset->set_compatibility_version(NAVMESH_TILE_COMPAT_VERSION);
		ToProto(*tileset->mutable_mesh_params(), m_navMesh->getParams());
		ToProto(*tileset->mutable_tiles(), m_navMesh.get());
	}

	if (+(fields & PersistedDataFields::ConvexVolumes))
	{
		// save convex volumes
		for (const auto& volume : m_volumes)
		{
			nav::ConvexVolume* proto_vol = proto.add_convex_volumes();
			ToProto(*proto_vol, *volume);
		}
	}

	if (+(fields & PersistedDataFields::AreaTypes))
	{
		// save area definitions
		for (const PolyAreaType* area : m_polyAreaList)
		{
			// only serialize user defined areas
			if (area->id == (uint8_t)PolyArea::Unwalkable)
				continue;

			if (!IsUserDefinedPolyArea(area->id))
			{
				if (area->id < DefaultPolyAreas.size()
					&& *area == DefaultPolyAreas[area->id])
				{
					continue;
				}
			}

			nav::PolyAreaType* proto_area = proto.add_areas();
			ToProto(*proto_area, *area);
		}
	}

	if (+(fields & PersistedDataFields::Connections))
	{
		// save connections
		for (const auto& conn : m_connections)
		{
			// don't save invalid connections
			if (!conn->valid)
				continue;

			nav::Connection* proto_conn = proto.add_connections();
			ToProto(*proto_conn, *conn);
		}
	}
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

NavMesh::LoadResult NavMesh::LoadNavMeshFile(const std::string& filename)
{
	m_lastLoadResult = LoadMesh(filename.c_str());
	OnNavMeshChanged();

	return m_lastLoadResult;
}

NavMesh::LoadResult NavMesh::LoadMesh(const char* filename)
{
	// cache the filename of the file we tried to load
	m_dataFile = filename;

	FILE* file = nullptr;
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
		SPDLOG_ERROR("loadMesh: mesh file is not a valid mesh file");
		return LoadResult::Corrupt;
	}

	std::unique_ptr<char[]> buffer(new char[filesize]);
	size_t result = fread(buffer.get(), filesize, 1, file);
	if (result != 1)
	{
		SPDLOG_ERROR("loadMesh: failed to read contents of mesh file");
		return LoadResult::Corrupt;
	}

	char* data_ptr = buffer.get();
	size_t data_size = filesize;

	// read header
	MeshFileHeader* fileHeader = (MeshFileHeader*)data_ptr;

	uint32_t uncompressedSize = 0;
	size_t headerSize = 0;
	uint16_t headerVersion = fileHeader->version;

	if (fileHeader->magic != NAVMESH_FILE_MAGIC)
	{
		SPDLOG_ERROR("loadMesh: mesh file is not a valid mesh file");
		return LoadResult::Corrupt;
	}

	if (headerVersion == (uint16_t)NavMeshHeaderVersion::Version4)
	{
		headerSize = sizeof(MeshFileHeader);
	}
	else if (headerVersion >= (uint16_t)NavMeshHeaderVersion::Version5)
	{
		MeshFileHeaderV5* fileHeaderV5 = (MeshFileHeaderV5*)data_ptr;

		headerSize = fileHeaderV5->headerSize;
		uncompressedSize = fileHeaderV5->uncompressedSize;
	}
	else
	{
		SPDLOG_ERROR("loadMesh: mesh file has an incompatible version number: {0}", headerVersion);
		return LoadResult::VersionMismatch;
	}

	data_ptr += headerSize; data_size -= headerSize;

	bool compressed = +(fileHeader->flags & NavMeshFileFlags::COMPRESSED) != 0;
	nav::NavMeshFile file_proto;

	if (compressed)
	{
		try
		{
			std::vector<uint8_t> data;
			if (!DecompressMemory(data_ptr, data_size, data, uncompressedSize))
			{
				SPDLOG_ERROR("loadMesh: failed to decompress mesh file");
				return LoadResult::Corrupt;
			}

			buffer.reset();

			if (!file_proto.ParseFromArray(&data[0], (int)data.size()))
			{
				SPDLOG_ERROR("loadMesh: failed to parse mesh file");
				return LoadResult::Corrupt;
			}
		}
		catch (const std::bad_alloc&)
		{
			return LoadResult::OutOfMemory;
		}
	}
	else
	{
		if (!file_proto.ParseFromArray(data_ptr, (int)data_size))
		{
			SPDLOG_ERROR("loadMesh: failed to parse mesh file");
			return LoadResult::Corrupt;
		}
	}

	if (m_zoneName.empty())
	{
		m_zoneName = file_proto.zone_short_name();
	}
	else if (file_proto.zone_short_name() != m_zoneName)
	{
		SPDLOG_ERROR("loadMesh: zone name mismatch! mesh is for '{}'", file_proto.zone_short_name());
		return LoadResult::ZoneMismatch;
	}

	m_version = static_cast<NavMeshHeaderVersion>(headerVersion);

	ResetSavedData(PersistedDataFields::All);
	LoadFromProto(file_proto, PersistedDataFields::All);

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

bool NavMesh::SaveNavMeshFile(const std::string& filename, NavMeshHeaderVersion version)
{
	return SaveMesh(filename.c_str(), version);
}

bool NavMesh::SaveMeshV4(const char* filename)
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

	SaveToProto(file_proto, PersistedDataFields::All);

	// Store header.
	MeshFileHeader header;
	header.magic = NAVMESH_FILE_MAGIC;
	header.version = (uint16_t)NavMeshHeaderVersion::Version4;
	header.flags = NavMeshFileFlags{};
	outfile.write((const char*)&header, sizeof(header));

	if (compress) header.flags |= NavMeshFileFlags::COMPRESSED;

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

	m_version = NavMeshHeaderVersion::Version4;
	outfile.close();
	return true;
}

bool NavMesh::SaveMeshV5(const char* filename)
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

	SaveToProto(file_proto, PersistedDataFields::All);

	// Store header.
	MeshFileHeaderV5 header;
	header.magic = NAVMESH_FILE_MAGIC;
	header.version = (uint16_t)NavMeshHeaderVersion::Version5;
	header.flags = NavMeshFileFlags{};
	header.headerSize = sizeof(MeshFileHeaderV5);
	header.uncompressedSize = 0;

	if (compress) header.flags |= NavMeshFileFlags::COMPRESSED;

	if (compress)
	{
		std::string buffer;
		file_proto.SerializeToString(&buffer);

		// save the uncompressed size and write out the header
		header.uncompressedSize = (uint32_t)buffer.length();
		outfile.write((const char*)&header, sizeof(header));

		std::vector<uint8_t> data;
		CompressMemory(&buffer[0], buffer.length(), data);

		outfile.write((const char*)&data[0], data.size());
	}
	else
	{
		outfile.write((const char*)&header, sizeof(header));
		file_proto.SerializeToOstream(&outfile);
	}

	m_version = NavMeshHeaderVersion::Version5;
	outfile.close();
	return true;
}

bool NavMesh::SaveMesh(const char* filename, NavMeshHeaderVersion version /*= NavMeshHeaderVersion::Latest*/)
{
	if (version == NavMeshHeaderVersion::Version4)
		return SaveMeshV4(filename);
	if (version == NavMeshHeaderVersion::Version5)
		return SaveMeshV5(filename);

	return false;
}

//----------------------------------------------------------------------------

ConvexVolume* NavMesh::AddConvexVolume(std::unique_ptr<ConvexVolume> volume)
{
	volume->id = m_nextVolumeId++;

	ConvexVolume* vol = volume.get();
	m_volumes.push_back(std::move(volume));
	m_volumesById.emplace(vol->id, vol);

	return vol;
}

ConvexVolume* NavMesh::AddConvexVolume(const std::vector<glm::vec3>& verts,
	const std::string& name, float minh, float maxh, uint8_t areaType)
{
	auto volume = std::make_unique<ConvexVolume>();
	volume->areaType = areaType;
	volume->hmax = maxh;
	volume->hmin = minh;
	volume->verts = verts;
	volume->name = name;

	return AddConvexVolume(std::move(volume));
}

void NavMesh::DeleteConvexVolumeById(uint32_t id)
{
	auto iter = std::find_if(m_volumes.begin(), m_volumes.end(),
		[id](const auto& ptr) { return ptr->id == id; });
	if (iter != m_volumes.end())
	{
		m_volumesById.erase((*iter)->id);
		m_volumes.erase(iter);
	}
}

ConvexVolume* NavMesh::GetConvexVolumeById(uint32_t id)
{
	auto iter = m_volumesById.find(id);
	if (iter != m_volumesById.end())
		return iter->second;

	return nullptr;
}

std::vector<dtTileRef> NavMesh::GetTilesIntersectingConvexVolume(uint32_t id)
{
	std::vector<dtTileRef> tiles;
	auto volume = GetConvexVolumeById(id);

	if (m_navMesh && volume && volume->verts.size() > 1)
	{
		glm::vec3 bmin = volume->verts[0], bmax = volume->verts[0];
		for (const auto& vert : volume->verts)
		{
			bmin = glm::min(bmin, vert);
			bmax = glm::max(bmax, vert);
		}

		int minx, miny, maxx, maxy;
		m_navMesh->calcTileLoc(glm::value_ptr(bmin), &minx, &miny);
		m_navMesh->calcTileLoc(glm::value_ptr(bmax), &maxx, &maxy);

		static const int MAX_NEIS = 32;
		const dtMeshTile* neis[MAX_NEIS];

		for (int y = miny; y <= maxy; ++y)
		{
			for (int x = minx; x <= maxx; ++x)
			{
				const int nneis = m_navMesh->getTilesAt(x, y, neis, MAX_NEIS);
				for (int j = 0; j < nneis; j++)
				{
					dtTileRef tileRef = m_navMesh->getTileRef(neis[j]);
					tiles.push_back(tileRef);
				}
			}
		}

		// sort the tiles and unique-ify them.
		std::sort(tiles.begin(), tiles.end());

		auto last = std::unique(tiles.begin(), tiles.end());
		tiles.erase(last, tiles.end());
	}

	return tiles;
}

void NavMesh::MoveConvexVolumeToIndex(uint32_t id, size_t toIndex)
{
	// get iterator and index of id
	auto fromIter = std::find_if(std::begin(m_volumes),
		std::end(m_volumes),
		[id](const auto& vol) { return vol->id == id; });
	size_t fromIndex = std::distance(std::begin(m_volumes), fromIter);

	auto toIter = std::next(std::begin(m_volumes), toIndex);

	if (toIndex > fromIndex)
	{
		std::rotate(fromIter, std::next(fromIter), std::next(toIter));
	}
	else
	{
		std::rotate(toIter, fromIter, std::next(fromIter));
	}
}

//----------------------------------------------------------------------------

OffMeshConnection* NavMesh::AddConnection(std::unique_ptr<OffMeshConnection> connection)
{
	connection->id = m_nextConnectionId++;

	OffMeshConnection* conn = connection.get();
	m_connections.push_back(std::move(connection));
	m_connectionsById.emplace(conn->id, conn);

	// todo: update buffer if one exists?

	return conn;
}

OffMeshConnection* NavMesh::GetConnectionById(uint32_t id)
{
	auto iter = m_connectionsById.find(id);
	if (iter != m_connectionsById.end())
		return iter->second;

	return nullptr;
}

void NavMesh::DeleteConnectionById(uint32_t id)
{
	auto iter = std::find_if(m_connections.begin(), m_connections.end(),
		[id](const auto& ptr) { return ptr->id == id; });
	if (iter != m_connections.end())
	{
		m_connectionsById.erase((*iter)->id);
		m_connections.erase(iter);
	}

	// todo: update buffer if one exists?
}

std::vector<dtTileRef> NavMesh::GetTilesIntersectingConnection(uint32_t connectionId)
{
	std::vector<dtTileRef> refs;

	OffMeshConnection* connection = GetConnectionById(connectionId);
	if (connection && connection->valid)
	{
		return GetTileRefsForPoint(connection->start);
	}

	return refs;
}

std::shared_ptr<OffMeshConnectionBuffer> NavMesh::CreateOffMeshConnectionBuffer() const
{
	// crude and simple. Could be optimized in the future to not regenerate
	// every time we need it. At least this is thread-safe...
	return std::make_shared<OffMeshConnectionBuffer>(this, m_connections);
}

//----------------------------------------------------------------------------

void NavMesh::InitializeAreas()
{
	m_polyAreaList.clear();

	// initialize the array
	for (uint8_t i = 0; i < m_polyAreas.size(); i++)
	{
		m_polyAreas[i] = PolyAreaType{ i, std::string(), duIntToCol(i, 255),
			+PolyFlags::Walk, 1.f, false, true };
	}

	for (const PolyAreaType& area : DefaultPolyAreas)
	{
		m_polyAreas[area.id] = area;
		if (area.selectable)
		{
			m_polyAreaList.push_back(&m_polyAreas[area.id]);
		}
	}
}

std::vector<dtTileRef> NavMesh::GetTileRefsForPoint(const glm::vec3& pos)
{
	std::vector<dtTileRef> refs;

	int tilex, tiley;
	m_navMesh->calcTileLoc(glm::value_ptr(pos), &tilex, &tiley);

	static const int MAX_NEIS = 32;
	const dtMeshTile* neis[MAX_NEIS];

	const int nneis = m_navMesh->getTilesAt(tilex, tiley, neis, MAX_NEIS);
	for (int j = 0; j < nneis; j++)
	{
		dtTileRef tileRef = m_navMesh->getTileRef(neis[j]);
		refs.push_back(tileRef);
	}

	return refs;
}

void NavMesh::UpdateArea(const PolyAreaType& areaType)
{
	// don't read in invalid ids
	if (areaType.id < m_polyAreas.size())
	{
		// simpler to just remove and append new poly area 
		auto iter = std::find_if(m_polyAreaList.begin(), m_polyAreaList.end(),
			[&areaType](const PolyAreaType* area)
		{
			return areaType.id == area->id; 
		});
		if (iter != m_polyAreaList.end())
			m_polyAreaList.erase(iter);

		if (IsUserDefinedPolyArea(areaType.id))
		{
			m_polyAreas[areaType.id] = areaType;
			m_polyAreas[areaType.id].valid = true;
			m_polyAreas[areaType.id].selectable = true;
		}
		else
		{
			// can only change color and cost
			m_polyAreas[areaType.id].color = areaType.color;
			m_polyAreas[areaType.id].cost = areaType.cost;
		}

		if (areaType.selectable)
		{
			m_polyAreaList.push_back(&m_polyAreas[areaType.id]);
		}

		std::sort(m_polyAreaList.begin(), m_polyAreaList.end(),
			[](const PolyAreaType* typeA, const PolyAreaType* typeB) { return typeA->id < typeB->id; });
	}
}

void NavMesh::RemoveUserDefinedArea(uint8_t areaId)
{
	if (!IsUserDefinedPolyArea(areaId))
		return;

	m_polyAreas[areaId].valid = false;

	auto iter = std::find_if(m_polyAreaList.begin(), m_polyAreaList.end(),
		[areaId](const PolyAreaType* area)
	{
		return areaId == area->id;
	});
	if (iter != m_polyAreaList.end())
		m_polyAreaList.erase(iter);

	m_polyAreas[areaId] = PolyAreaType{ areaId, std::string(), duIntToCol(areaId, 255),
		+PolyFlags::Walk, 1.f, false, true };

	std::sort(m_polyAreaList.begin(), m_polyAreaList.end(),
		[](const PolyAreaType* typeA, const PolyAreaType* typeB) { return typeA->id < typeB->id; });
}

uint8_t NavMesh::GetFirstUnusedUserDefinedArea() const
{
	for (uint8_t index = (uint8_t)PolyArea::UserDefinedFirst;
		index <= (uint8_t)PolyArea::UserDefinedLast; ++index)
	{
		if (!m_polyAreas[index].valid)
			return index;
	}

	return 0;
}

void NavMesh::FillFilterAreaCosts(dtQueryFilter& filter)
{
	for (const PolyAreaType& areaType : m_polyAreas)
	{
		if (areaType.valid)
		{
			filter.setAreaCost(areaType.id, areaType.cost);
		}
	}
}

bool NavMesh::ExportJson(const std::string& filename, PersistedDataFields fields)
{
	if (m_zoneName.empty())
		return false;

	nav::NavMeshFile proto;
	SaveToProto(proto, fields);

	google::protobuf::util::JsonPrintOptions options;
	options.add_whitespace = true;
	//options.always_print_primitive_fields = true;

	std::string jsonString;
	google::protobuf::util::Status status =
		google::protobuf::util::MessageToJsonString(proto, &jsonString, options);
	if (status.ok())
	{
		fs::path rootPath(filename);
		std::error_code returnedError;
		fs::create_directories(rootPath.parent_path(), returnedError);

		std::ofstream ofile(filename.c_str(), std::ios::trunc);
		if (ofile.is_open())
		{
			ofile << jsonString;
			return true;
		}
	}

	return false;
}

bool NavMesh::ImportJson(const std::string& filename, PersistedDataFields fields)
{
	if (m_zoneName.empty())
		return false;

	std::string contents;
	{
		std::ifstream infile(filename.c_str());
		if (!infile.is_open())
			return false;

		std::stringstream buffer;
		buffer << infile.rdbuf();
		contents = buffer.str();
	}
	
	google::protobuf::util::JsonParseOptions options;
	options.ignore_unknown_fields = true;

	nav::NavMeshFile proto;
	google::protobuf::util::Status status =
		google::protobuf::util::JsonStringToMessage(contents,
			&proto, options);
	if (!status.ok())
		return false;

	LoadFromProto(proto, fields);
	OnNavMeshChanged();

	return true;
}

std::vector<float> NavMesh::GetHeights(const glm::vec3& pos)
{
	std::vector<float> heights;

	const float center[3] = { pos.x, pos.y, pos.z };
	const float extents[3] = { 1.f, m_boundsMax.y - m_boundsMin.y, 1.f };
	dtQueryFilter filter;
	dtPolyRef polys[128];
	int polyCount;

	// we will still get success on 0 polys, but it won't matter
	auto query = GetNavMeshQuery();
	if (IsNavMeshLoaded()
		&& query
		&& !(query->queryPolygons(center, extents, &filter, polys, &polyCount, 128) & DT_FAILURE))
	{
		for (int idx_poly = 0; idx_poly < polyCount; ++idx_poly)
		{
			dtPolyRef poly = polys[idx_poly];
			float height;

			// this can fail with valid coords, so we need to make sure we are only adding valid heights.
			if (!dtStatusFailed(query->getPolyHeight(poly, center, &height)))
				heights.push_back(height);
		}
	}

	return heights;
}

float NavMesh::GetClosestHeight(const glm::vec3& pos)
{
	float height = pos.y;
	auto heights = GetHeights(pos);
	std::sort(heights.begin(), heights.end());

	auto height_it = std::lower_bound(heights.begin(), heights.end(), pos.y);
	if (height_it == heights.end() && height_it != heights.begin())
	{
		--height_it;
	}
	else if (height_it != heights.end()
		&& height_it != heights.begin()
		&& std::abs(*height_it - pos.y) > std::abs(*(height_it - 1) - pos.y))
	{
		--height_it;
	}

	return height_it == heights.end() ? pos.y : *height_it;
}

//============================================================================

OffMeshConnectionBuffer::OffMeshConnectionBuffer(
	const NavMesh* navMesh,
	const std::vector<std::unique_ptr<OffMeshConnection>>& connections)
{
	// when we have multiple types of connections in the future, this will
	// need to change to only count the number of basic connections.

	if (!connections.empty())
	{
		offMeshConVerts.reserve(connections.size());
		offMeshConRads.reserve(connections.size());
		offMeshConDirs.reserve(connections.size());
		offMeshConAreas.reserve(connections.size());
		offMeshConFlags.reserve(connections.size());
		offMeshConId.reserve(connections.size());

		for (const auto& conn : connections)
		{
			offMeshConVerts.emplace_back(conn->start, conn->end);
			offMeshConRads.emplace_back(navMesh->GetNavMeshConfig().agentRadius);
			offMeshConDirs.emplace_back(conn->bidirectional ? (uint8_t)DT_OFFMESH_CON_BIDIR : 0);
			offMeshConAreas.emplace_back(conn->areaType);
			offMeshConFlags.emplace_back(navMesh->GetPolyArea(conn->areaType).flags);
			offMeshConId.emplace_back(conn->id);
		}
	}

	offMeshConCount = connections.size();
}

void OffMeshConnectionBuffer::UpdateNavMeshCreateParams(dtNavMeshCreateParams& params)
{
	if (offMeshConCount > 0)
	{
		params.offMeshConVerts = glm::value_ptr(offMeshConVerts[0].first);
		params.offMeshConRad = offMeshConRads.data();
		params.offMeshConFlags = offMeshConFlags.data();
		params.offMeshConAreas = offMeshConAreas.data();
		params.offMeshConDir = offMeshConDirs.data();
		params.offMeshConUserID = offMeshConId.data();
	}
	else
	{
		params.offMeshConVerts = nullptr;
		params.offMeshConRad = nullptr;
		params.offMeshConFlags = nullptr;
		params.offMeshConAreas = nullptr;
		params.offMeshConDir = nullptr;
		params.offMeshConUserID = nullptr;
	}

	params.offMeshConCount = static_cast<int>(offMeshConCount);
}

//============================================================================
