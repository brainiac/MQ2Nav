//
// HeadlessBuild.cpp
//
// MeshGenerator.exe build: builds a zone's navmesh without the GUI, into a staging folder, and
// prints one JSON object on stdout. Progress goes to stderr. Nothing is ever overwritten.
//

#include "meshgen/HeadlessBuild.h"

#include "cli/CliSupport.h"
#include "meshgen/Application.h"
#include "meshgen/InputGeom.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"

#include <Recast.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <concrt.h>
#include <map>
#include <tuple>

namespace nav_cli {

	void PrintBuildUsage()
	{
		fmt::print("{}",
			"MeshGenerator.exe build --zone Z --eq-path PATH --stage-root PATH [options]\n"
			"\n"
			"Builds <stage-root>\\MQ2Nav\\<zone>.navmesh and prints one JSON object. Never overwrites a file.\n"
			"\n"
			"  --mesh-root PATH          folder holding <zone>_doors.json, usually MacroQuest's\n"
			"                            resources\\MQ2Nav (defaults to --eq-path)\n"
			"  --no-max-extents          ignore MQ2Nav's built-in zone bounds\n"
			"  --settings-from MESH      start from another mesh's build settings, then any of\n"
			"      --tile-size --cell-size --cell-height --agent-height --agent-radius --max-climb\n"
			"      --max-slope --region-min-size --region-merge-size --edge-max-len --edge-max-error\n"
			"      --verts-per-poly --detail-sample-dist --detail-sample-max-error\n"
			"  --carry-from MESH         copy another mesh's connections, volumes and area types\n"
			"  --fixes FILE              connections, volumes and areas as MeshTool export writes them\n"
			"  --connections FILE        JSON array of {\"start\":[x,y,z],\"end\":[x,y,z]} in mesh axes, y up\n"
			"  --volumes FILE            JSON array of {\"verts\":[[x,y,z],...],\"hmin\":N,\"hmax\":N,\"area\":N}\n"
			"  --base MESH [--box \"Y1,X1:Y2,X2\"]   rebuild only the tiles a /loc box and new connections or\n"
			"                            volumes touch; settings come from the base mesh\n"
			"  --threads N               cap the build threads\n"
			"  --mesh-version 4|5        file version to write\n"
			"\n"
			"Exit codes: 0 ok, 2 usage, 3 missing input, 4 build or write failure, 5 bad mesh, 70 internal error.\n");
	}

	inline bool HasSettingFlag(const Options& options)
	{
		for (const Setting& setting : kSettings)
			if (options.count(setting.name))
				return true;
		return false;
	}

	inline void ApplySettingFlags(NavMeshConfig& config, const Options& options)
	{
		for (const Setting& setting : kSettings)
		{
			const auto found = options.find(setting.name);
			if (found == options.end())
				continue;
			float value = 0.0f;
			try
			{
				size_t used = 0;
				value = std::stof(found->second, &used);
				if (used != found->second.size())
					throw std::invalid_argument(setting.name);
			}
			catch (const std::exception&)
			{
				throw CliError(2, "bad_setting", std::string("Setting must be a number: ") + setting.name);
			}
			if (!std::isfinite(value) || value < setting.low || value > setting.high
				|| (setting.whole && value != std::floor(value)))
			{
				throw CliError(2, "bad_setting", fmt::format("{} must be {}from {} to {}.", setting.name,
					setting.whole ? "a whole number " : "", setting.low, setting.high));
			}
			config.*setting.field = value;
		}
	}

	inline bool ReadVec(const rapidjson::Value& item, const char* key, glm::vec3& out)
	{
		if (!item.HasMember(key) || !item[key].IsArray() || item[key].Size() != 3)
			return false;
		for (rapidjson::SizeType i = 0; i < 3; ++i)
		{
			if (!item[key][i].IsNumber())
				return false;
			out[i] = item[key][i].GetFloat();
			if (!std::isfinite(out[i]))
				return false;
		}
		return true;
	}

	// Area ids 62 and 63 are reserved: 62 is the walkway marker during a build and 63 is Recast's
	// walkable area. Volumes may use 0 (not walkable); links and area types may not.
	inline uint8_t ReadAreaId(const rapidjson::Value& item, const std::string& code, uint8_t fallback, unsigned int lowest)
	{
		if (!item.HasMember("area"))
			return fallback;
		if (!item["area"].IsUint() || item["area"].GetUint() < lowest || item["area"].GetUint() > 61)
			throw CliError(2, code, fmt::format("area must be a whole number from {} to 61.", lowest));
		return static_cast<uint8_t>(item["area"].GetUint());
	}

	inline int AddConnections(const rapidjson::Value& array, NavMesh& navMesh, std::vector<uint32_t>& added)
	{
		if (!array.IsArray())
			throw CliError(2, "bad_connections", "Connections must be a JSON array.");
		int count = 0;
		for (const auto& item : array.GetArray())
		{
			auto connection = std::make_unique<OffMeshConnection>();
			if (!item.IsObject() || !ReadVec(item, "start", connection->start) || !ReadVec(item, "end", connection->end))
				throw CliError(2, "bad_connections", "Each connection needs start and end as [x,y,z].");
			if (item.HasMember("bidirectional") && item["bidirectional"].IsBool())
				connection->bidirectional = item["bidirectional"].GetBool();
			if (item.HasMember("name") && item["name"].IsString())
				connection->name = item["name"].GetString();
			connection->areaType = ReadAreaId(item, "bad_connections", connection->areaType, 1);
			added.push_back(navMesh.AddConnection(std::move(connection))->id);
			++count;
		}
		return count;
	}

	inline int AddVolumes(const rapidjson::Value& array, NavMesh& navMesh, std::vector<uint32_t>& added)
	{
		if (!array.IsArray())
			throw CliError(2, "bad_volumes", "Volumes must be a JSON array.");
		int count = 0;
		for (const auto& item : array.GetArray())
		{
			if (!item.IsObject() || !item.HasMember("verts") || !item["verts"].IsArray() || item["verts"].Size() < 3
				|| !item.HasMember("hmin") || !item["hmin"].IsNumber() || !item.HasMember("hmax") || !item["hmax"].IsNumber()
				|| !item.HasMember("area"))
				throw CliError(2, "bad_volumes", "Each volume needs verts (3 or more), hmin, hmax and area.");
			std::vector<glm::vec3> verts;
			for (const auto& v : item["verts"].GetArray())
			{
				if (!v.IsArray() || v.Size() != 3 || !v[0].IsNumber() || !v[1].IsNumber() || !v[2].IsNumber())
					throw CliError(2, "bad_volumes", "Volume vertices must be [x,y,z].");
				verts.emplace_back(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat());
			}
			const uint8_t area = ReadAreaId(item, "bad_volumes", 0, 0);
			const float hmin = item["hmin"].GetFloat(), hmax = item["hmax"].GetFloat();
			if (!(hmin < hmax))
				throw CliError(2, "bad_volumes", "Volume hmin must be below hmax.");
			const std::string name = item.HasMember("name") && item["name"].IsString() ? item["name"].GetString() : "";
			added.push_back(navMesh.AddConvexVolume(verts, name, hmin, hmax, area)->id);
			++count;
		}
		return count;
	}

	inline int AddAreas(const rapidjson::Value& array, NavMesh& navMesh)
	{
		if (!array.IsArray())
			throw CliError(2, "bad_areas", "Areas must be a JSON array.");
		int count = 0;
		for (const auto& item : array.GetArray())
		{
			if (!item.IsObject() || !item.HasMember("id") || !item["id"].IsUint() || item["id"].GetUint() < 1 || item["id"].GetUint() > 61
				|| !item.HasMember("name") || !item["name"].IsString() || !item.HasMember("cost") || !item["cost"].IsNumber())
				throw CliError(2, "bad_areas", "Each area needs id (1 to 61), name and cost.");
			PolyAreaType area{};
			area.id = static_cast<uint8_t>(item["id"].GetUint());
			area.name = item["name"].GetString();
			area.cost = item["cost"].GetFloat();
			area.color = item.HasMember("color") && item["color"].IsUint() ? item["color"].GetUint() : 0xffffffff;
			area.flags = item.HasMember("flags") && item["flags"].IsUint() ? static_cast<uint16_t>(item["flags"].GetUint()) : +PolyFlags::Walk;
			area.selectable = item.HasMember("selectable") && item["selectable"].IsBool() ? item["selectable"].GetBool() : true;
			area.valid = true;
			navMesh.UpdateArea(area);
			++count;
		}
		return count;
	}

	// Copies links, volumes and area types. IDs are assigned fresh, so diff matches by position.
	inline void CarryFixes(const NavMesh& from, NavMesh& to, std::vector<uint32_t>& connections, std::vector<uint32_t>& volumes,
		int& areas)
	{
		for (const PolyAreaType* area : from.GetPolyAreas())
		{
			to.UpdateArea(*area);
			++areas;
		}
		for (const auto& c : from.GetConnections())
		{
			auto copy = std::make_unique<OffMeshConnection>(*c);
			connections.push_back(to.AddConnection(std::move(copy))->id);
		}
		for (const auto& v : from.GetConvexVolumes())
			volumes.push_back(to.AddConvexVolume(v->verts, v->name, v->hmin, v->hmax, v->areaType)->id);
	}

	Options ParseOptions(int argc, char* argv[], const std::set<std::string>& valueOptions,
		const std::set<std::string>& flagOptions)
	{
		Options options;
		for (int index = 2; index < argc; ++index)
		{
			const std::string key = argv[index];
			if (key == "--help" || key == "-h")
			{
				PrintBuildUsage();
				throw CliError(0, "help", "");
			}

			if (key.size() < 3 || key.substr(0, 2) != "--")
				throw CliError(2, "invalid_argument", "Expected a named option, got: " + key);

			if (flagOptions.count(key))
			{
				if (!options.emplace(key, "").second)
					throw CliError(2, "invalid_argument", "Option provided more than once: " + key);
				continue;
			}

			if (!valueOptions.count(key))
				throw CliError(2, "invalid_argument", "Unknown option: " + key);
			if (index + 1 >= argc || std::string(argv[index + 1]).rfind("--", 0) == 0)
				throw CliError(2, "invalid_argument", "Missing value for option: " + key);
			if (!options.emplace(key, argv[++index]).second)
				throw CliError(2, "invalid_argument", "Option provided more than once: " + key);
		}
		return options;
	}

	//------------------------------------------------------------------------
	// Scheduler cap for --threads. The build already runs tiles on the concurrency runtime.

	class SchedulerScope
	{
	public:
		explicit SchedulerScope(unsigned int threads)
		{
			if (threads > 0)
			{
				concurrency::CurrentScheduler::Create(concurrency::SchedulerPolicy(2,
					concurrency::MinConcurrency, 1u, concurrency::MaxConcurrency, threads));
				m_active = true;
			}
		}
		~SchedulerScope()
		{
			if (m_active)
				concurrency::CurrentScheduler::Detach();
		}
		SchedulerScope(const SchedulerScope&) = delete;
		SchedulerScope& operator=(const SchedulerScope&) = delete;

	private:
		bool m_active = false;
	};

	int BuildMesh(const Options& options)
	{
		const std::string zone = GetZone(options);
		const fs::path eqPath = PathOption(options, "--eq-path");
		const fs::path stageRoot = PathOption(options, "--stage-root");
		const fs::path meshRoot = fs::absolute(options.count("--mesh-root")
			? options.at("--mesh-root") : eqPath.string()).lexically_normal();
		const bool regionBuild = options.count("--base") > 0;

		if (regionBuild && (options.count("--settings-from") || HasSettingFlag(options)))
			throw CliError(2, "conflict", "--base takes its settings from the base mesh; drop the settings options.");
		if (regionBuild && options.count("--carry-from"))
			throw CliError(2, "conflict", "--base already keeps the base mesh's fixes; drop --carry-from.");
		if (options.count("--tiles") && options.count("--box"))
			throw CliError(2, "conflict", "--box and --tiles are the same option; give one.");
		if ((options.count("--tiles") || options.count("--box")) && !regionBuild)
			throw CliError(2, "conflict", "--tiles needs --base.");

		std::error_code error;
		if (!fs::is_directory(eqPath, error))
			throw CliError(3, "missing_eq_path", "EverQuest geometry directory not found: " + eqPath.string());
		if (!fs::is_directory(meshRoot, error))
			throw CliError(3, "missing_mesh_root", "Mesh metadata root not found: " + meshRoot.string());
		if (stageRoot.string().size() >= MAX_PATH - 16)
			throw CliError(2, "path_too_long", "Stage root is too long for this MQ2Nav build.");

		const fs::path outputDirectory = stageRoot / "MQ2Nav";
		const fs::path outputPath = outputDirectory / (zone + ".navmesh");
		if (fs::exists(outputPath, error))
			throw CliError(4, "output_exists", "Refusing to overwrite staged mesh: " + outputPath.string());
		if (error)
			throw CliError(4, "output_error", "Could not inspect staged output path: " + error.message());

		const unsigned int threads = static_cast<unsigned int>(NumberOption(options, "--threads", 0.0f, 1.0f, 256.0f));
		int headerVersion = static_cast<int>(NavMeshHeaderVersion::Latest);
		if (options.count("--mesh-version"))
		{
			headerVersion = static_cast<int>(NumberOption(options, "--mesh-version", 5.0f, 4.0f, 5.0f));
			if (headerVersion != 4 && headerVersion != 5)
				throw CliError(2, "bad_option", "--mesh-version must be 4 or 5.");
		}

		fs::create_directories(outputDirectory);

		auto navMesh = std::make_shared<NavMesh>(outputDirectory.string(), zone);
		RecastContext context;
		auto geometry = std::make_unique<InputGeom>(zone, eqPath.string());
		auto geometryLoader = std::make_unique<MapGeometryLoader>(zone, eqPath.string(), meshRoot.string());
		if (!options.count("--no-max-extents"))
		{
			const auto extents = MaxZoneExtents.find(zone);
			if (extents != MaxZoneExtents.end())
				geometryLoader->SetMaxExtents(extents->second);
		}

		spdlog::info("Loading geometry for zone {}.", zone);
		if (!geometry->loadGeometry(std::move(geometryLoader), &context))
			throw CliError(3, "geometry_load_failed", "Could not load usable zone geometry for: " + zone);

		NavMeshTool navMeshTool(navMesh);
		navMeshTool.setContext(&context);
		navMeshTool.setOutputPath(stageRoot.string().c_str());
		navMeshTool.handleGeometryChanged(geometry.get());

		// Settings: preset or another mesh first, then single flags on top.
		std::string settingsSource = "mq2nav";
		if (regionBuild)
		{
			const fs::path basePath = PathOption(options, "--base");
			if (navMesh->LoadNavMeshFile(basePath.string()) != NavMesh::LoadResult::Success || !navMesh->GetNavMesh())
				throw CliError(5, "invalid_mesh", "Base mesh could not be loaded: " + basePath.string());
			navMeshTool.GetConfig() = navMesh->GetNavMeshConfig();
			navMeshTool.UpdateTileSizes();
			headerVersion = options.count("--mesh-version") ? headerVersion : navMesh->GetHeaderVersion();
			settingsSource = "base";

		}
		else
		{
			NavMeshConfig config = navMeshTool.GetConfig();
			if (options.count("--settings-from"))
			{
				config = LoadMesh(zone, PathOption(options, "--settings-from"))->GetNavMeshConfig();
				settingsSource = "mesh";
			}
			ApplySettingFlags(config, options);
			navMeshTool.GetConfig() = config;
			navMesh->GetNavMeshConfig() = config;
		}
		{
			const NavMeshConfig& config = navMeshTool.GetConfig();
			spdlog::info("Settings ({}): tile {} cell {} cellHeight {} height {} radius {} climb {} slope {}", settingsSource,
				config.tileSize, config.cellSize, config.cellHeight, config.agentHeight,
				config.agentRadius, config.agentMaxClimb, config.agentMaxSlope);
		}

		// Fixes.
		std::vector<uint32_t> carriedConnections, carriedVolumes, newConnections, newVolumes;
		int carriedAreas = 0, newAreas = 0;
		if (options.count("--carry-from"))
		{
			auto from = LoadMesh(zone, PathOption(options, "--carry-from"));
			CarryFixes(*from, *navMesh, carriedConnections, carriedVolumes, carriedAreas);
			if (!options.count("--mesh-version"))
				headerVersion = from->GetHeaderVersion();
			spdlog::info("Carried {} links, {} volumes and {} area types.", carriedConnections.size(),
				carriedVolumes.size(), carriedAreas);
		}
		if (options.count("--fixes"))
		{
			const auto document = ReadJsonFile(options.at("--fixes"), "fixes");
			if (!document.IsObject())
				throw CliError(2, "bad_fixes", "A fixes file is a JSON object, as export writes it.");
			if (document.HasMember("areas"))
				newAreas += AddAreas(document["areas"], *navMesh);
			if (document.HasMember("connections"))
				AddConnections(document["connections"], *navMesh, newConnections);
			if (document.HasMember("volumes"))
				AddVolumes(document["volumes"], *navMesh, newVolumes);
		}
		if (options.count("--connections"))
			AddConnections(ReadJsonFile(options.at("--connections"), "connections"), *navMesh, newConnections);
		if (options.count("--volumes"))
			AddVolumes(ReadJsonFile(options.at("--volumes"), "volumes"), *navMesh, newVolumes);

		SchedulerScope scheduler(threads);
		std::vector<dtTileRef> rebuilt;
		if (regionBuild)
		{
			std::set<dtTileRef> tiles;
			std::set<std::pair<int, int>> emptyCells;
			const dtNavMesh* mesh = navMesh->GetNavMesh().get();
			const dtNavMeshParams* params = mesh->getParams();
			const glm::vec3& boundsMax = navMesh->GetNavMeshBoundsMax();
			const int maxTx = static_cast<int>(std::floor((boundsMax.x - params->orig[0]) / params->tileWidth));
			const int maxTy = static_cast<int>(std::floor((boundsMax.z - params->orig[2]) / params->tileHeight));

			// Every grid cell a box touches: existing tiles are rebuilt, and cells with no tile in the
			// base are refused, because building them would add mesh nobody reviewed.
			const auto addBox = [&](float minX, float maxX, float minZ, float maxZ) {
				const int tx0 = (std::max)(0, static_cast<int>(std::floor((minX - params->orig[0]) / params->tileWidth)));
				const int tx1 = (std::min)(maxTx, static_cast<int>(std::floor((maxX - params->orig[0]) / params->tileWidth)));
				const int ty0 = (std::max)(0, static_cast<int>(std::floor((minZ - params->orig[2]) / params->tileHeight)));
				const int ty1 = (std::min)(maxTy, static_cast<int>(std::floor((maxZ - params->orig[2]) / params->tileHeight)));
				for (int ty = ty0; ty <= ty1; ++ty)
				{
					for (int tx = tx0; tx <= tx1; ++tx)
					{
						const dtMeshTile* tile = mesh->getTileAt(tx, ty, 0);
						if (tile && tile->header)
							tiles.insert(mesh->getTileRef(tile));
						else
							emptyCells.emplace(tx, ty);
					}
				}
			};

			if (options.count("--tiles") || options.count("--box"))
			{
				const std::string region = options.count("--box") ? options.at("--box") : options.at("--tiles");
				const size_t colon = region.find(':');
				if (colon == std::string::npos)
					throw CliError(2, "bad_tiles", "--box must be \"Y1,X1:Y2,X2\" in /loc order.");
				const glm::vec3 a = ParseLoc(region.substr(0, colon) + ",0", "--tiles");
				const glm::vec3 b = ParseLoc(region.substr(colon + 1) + ",0", "--tiles");
				addBox((std::min)(a.x, b.x), (std::max)(a.x, b.x), (std::min)(a.z, b.z), (std::max)(a.z, b.z));
			}
			for (uint32_t id : newConnections)
			{
				const OffMeshConnection* c = navMesh->GetConnectionById(id);
				addBox(c->start.x, c->start.x, c->start.z, c->start.z);
				addBox(c->end.x, c->end.x, c->end.z, c->end.z);
				for (dtTileRef ref : navMesh->GetTilesIntersectingConnection(id))
					tiles.insert(ref);
			}
			for (uint32_t id : newVolumes)
			{
				const ConvexVolume* v = navMesh->GetConvexVolumeById(id);
				float minX = FLT_MAX, maxX = -FLT_MAX, minZ = FLT_MAX, maxZ = -FLT_MAX;
				for (const auto& p : v->verts)
				{
					minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
					minZ = (std::min)(minZ, p.z); maxZ = (std::max)(maxZ, p.z);
				}
				addBox(minX, maxX, minZ, maxZ);
			}
			if (!emptyCells.empty())
			{
				std::string list;
				for (const auto& [tx, ty] : emptyCells)
				{
					const float x0 = params->orig[0] + tx * params->tileWidth;
					const float z0 = params->orig[2] + ty * params->tileHeight;
					list += fmt::format("{}tile {},{} (/loc Y {:.0f} to {:.0f}, X {:.0f} to {:.0f})", list.empty() ? "" : "; ",
						tx, ty, z0, z0 + params->tileHeight, x0, x0 + params->tileWidth);
				}
				throw CliError(5, "empty_tiles", "The region covers grid cells the base mesh has no tile for, and a region "
					"rebuild only rebuilds existing tiles. Do a full build instead. Empty cells: " + list);
			}
			if (tiles.empty())
				throw CliError(2, "no_tiles", "Nothing to rebuild: give --box, or links or volumes that touch existing tiles.");
			rebuilt.assign(tiles.begin(), tiles.end());
			spdlog::info("Rebuilding {} tiles of the base mesh.", rebuilt.size());
			navMeshTool.RebuildTiles(rebuilt);
		}
		else
		{
			navMeshTool.UpdateTileSizes();
			spdlog::info("Building zone {} synchronously.", zone);
			if (!navMeshTool.handleBuild(false))
				throw CliError(4, "build_failed", "MQ2Nav failed to initialize the navmesh build.");
		}

		const int tileCount = CountTiles(navMesh->GetNavMesh().get());
		if (tileCount == 0)
			throw CliError(4, "empty_build", "Build completed without producing any navmesh tiles.");

		// Links whose ends are no longer on the mesh, within 2 units across and 4 up or down.
		struct Unanchored { const OffMeshConnection* c; bool start; bool end; };
		std::vector<Unanchored> unanchored;
		{
			auto query = navMesh->GetNavMeshQuery();
			const dtQueryFilter filter = MakeFilter(*navMesh);
			const float ext[3] = { 2.f, 4.f, 2.f };
			for (const auto& c : navMesh->GetConnections())
			{
				dtPolyRef s = 0, e = 0;
				float hit[3];
				query->findNearestPoly(glm::value_ptr(c->start), ext, &filter, &s, hit);
				query->findNearestPoly(glm::value_ptr(c->end), ext, &filter, &e, hit);
				if (!s || !e)
					unanchored.push_back({ c.get(), s != 0, e != 0 });
			}
		}

		const fs::path temporaryPath = outputPath.string() + ".building";
		if (fs::exists(temporaryPath, error))
			throw CliError(4, "temporary_exists", "Refusing to overwrite temporary build file: " + temporaryPath.string());

		if (!navMesh->SaveNavMeshFile(temporaryPath.string(), static_cast<NavMeshHeaderVersion>(headerVersion)))
		{
			fs::remove(temporaryPath, error);
			throw CliError(4, "save_failed", "Could not save the staged mesh.");
		}

		NavMesh validation("", zone);
		if (validation.LoadNavMeshFile(temporaryPath.string()) != NavMesh::LoadResult::Success
			|| CountTiles(validation.GetNavMesh().get()) != tileCount
			|| !validation.GetNavMeshQuery())
		{
			fs::remove(temporaryPath, error);
			throw CliError(5, "staged_validation_failed", "The staged mesh did not pass MQ2Nav load/query validation.");
		}

		fs::rename(temporaryPath, outputPath);

		rapidjson::StringBuffer buffer;
		Json w(buffer);
		w.StartObject();
		w.Key("ok"); w.Bool(true);
		w.Key("command"); w.String("build");
		w.Key("zone"); w.String(zone.c_str());
		w.Key("mesh"); w.String(outputPath.string().c_str());
		w.Key("tiles"); w.Int(tileCount);
		w.Key("polys"); w.Int(CountPolys(navMesh->GetNavMesh().get()));
		w.Key("validation"); w.String("passed");
		w.Key("headerVersion"); w.Int(headerVersion);
		w.Key("settingsSource"); w.String(settingsSource.c_str());
		w.Key("settings"); WriteSettings(w, navMeshTool.GetConfig());
		w.Key("carried");
		w.StartObject();
		w.Key("connections"); w.Uint64(carriedConnections.size());
		w.Key("volumes"); w.Uint64(carriedVolumes.size());
		w.Key("areas"); w.Int(carriedAreas);
		w.EndObject();
		w.Key("added");
		w.StartObject();
		w.Key("connections"); w.Uint64(newConnections.size());
		w.Key("volumes"); w.Uint64(newVolumes.size());
		w.Key("areas"); w.Int(newAreas);
		w.EndObject();
		w.Key("totalConnections"); w.Uint64(navMesh->GetConnectionCount());
		w.Key("totalVolumes"); w.Uint64(navMesh->GetConvexVolumeCount());
		if (regionBuild)
		{
			w.Key("rebuiltTiles"); w.Uint64(rebuilt.size());
		}
		w.Key("unanchoredLinks");
		w.StartArray();
		for (const auto& u : unanchored)
		{
			w.StartObject();
			w.Key("name"); w.String(u.c->name.c_str());
			w.Key("startLoc"); WriteLoc(w, glm::value_ptr(u.c->start));
			w.Key("endLoc"); WriteLoc(w, glm::value_ptr(u.c->end));
			w.Key("startOnMesh"); w.Bool(u.start);
			w.Key("endOnMesh"); w.Bool(u.end);
			w.EndObject();
		}
		w.EndArray();
		w.EndObject();
		Emit(buffer);
		return 0;
	}

} // namespace nav_cli

int RunHeadlessBuild(int argc, char* argv[])
{
	auto logger = spdlog::stderr_color_mt("MeshGenerator");
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("%^%l:%$ %v");
	spdlog::set_level(spdlog::level::info);
	spdlog::register_logger(logger->clone("Recast"));
	spdlog::register_logger(logger->clone("EQEmu"));

	using namespace nav_cli;
	try
	{
		if (argc < 3 || std::string(argv[2]) == "--help" || std::string(argv[2]) == "-h")
		{
			PrintBuildUsage();
			return 0;
		}
		std::set<std::string> values = { "--zone", "--eq-path", "--stage-root", "--mesh-root",
			"--settings-from", "--carry-from", "--fixes", "--connections", "--volumes",
			"--base", "--tiles", "--box", "--threads", "--mesh-version" };
		for (const Setting& setting : kSettings)
			values.insert(setting.name);
		return BuildMesh(ParseOptions(argc, argv, values, { "--no-max-extents" }));
	}
	catch (const CliError& error)
	{
		if (error.exitCode != 0)
			PrintError(error.code, error.what());
		return error.exitCode;
	}
	catch (const std::exception& error)
	{
		PrintError("internal_error", error.what());
		return 70;
	}
}
