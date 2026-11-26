//
// CliSupport.h
//
// Helpers shared by MeshTool's commands and MeshGenerator's headless build: JSON output, /loc
// parsing, build settings, mesh queries, plugin-style path planning, and reading and writing
// off-mesh connections, convex volumes and area types as JSON.
//

#pragma once

#include "common/NavMesh.h"
#include "common/NavMeshData.h"

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNode.h>
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Windows.h>

namespace nav_cli {

namespace fs = std::filesystem;

// Named options as given on the command line ("--zone" -> "everfrost"). Flags have empty values.
using Options = std::unordered_map<std::string, std::string>;

// Exit codes: 0 ok, 2 usage, 3 missing input, 4 build or write failure, 5 bad mesh,
// 6 a check failed, 70 internal error.
	class CliError : public std::runtime_error
	{
	public:
		CliError(int exitCode, const std::string& code, const std::string& message)
			: std::runtime_error(message)
			, exitCode(exitCode)
			, code(code)
		{
		}

		int exitCode;
		std::string code;
	};

	using Json = rapidjson::Writer<rapidjson::StringBuffer>;

	// The plugin's own search box and limits (plugin/NavigationPath.h and .cpp).
	const float kPluginExtents[3] = { 5.f, 10.f, 5.f };
	const int kMaxStraightPath = 16384;
	const float kNodePoolGrowth = 1.5f;
	const int kNodePoolMaxSize = 1024 * 1024;

	//------------------------------------------------------------------------
	// Output helpers

	inline void Emit(const rapidjson::StringBuffer& buffer)
	{
		fmt::print("{}\n", buffer.GetString());
	}

	inline void PrintError(const std::string& code, const std::string& message)
	{
		rapidjson::StringBuffer buffer;
		Json w(buffer);
		w.StartObject();
		w.Key("ok"); w.Bool(false);
		w.Key("error"); w.String(code.c_str());
		w.Key("message"); w.String(message.c_str());
		w.EndObject();
		Emit(buffer);
	}

	inline double Round1(float value)
	{
		return std::round(static_cast<double>(value) * 10.0) / 10.0;
	}

	// Mesh axes are x, y (up), z. /loc order is y, x, z with z up, so a mesh point prints as [z, x, y].
	template <typename Writer>
	inline void WriteLoc(Writer& w, const float* p)
	{
		w.StartArray();
		w.Double(Round1(p[2]));
		w.Double(Round1(p[0]));
		w.Double(Round1(p[1]));
		w.EndArray();
	}

	template <typename Writer>
	inline void WriteVec(Writer& w, const glm::vec3& p)
	{
		w.StartArray();
		w.Double(p.x);
		w.Double(p.y);
		w.Double(p.z);
		w.EndArray();
	}

	inline std::string RequiredOption(const Options& options, const std::string& name)
	{
		auto iter = options.find(name);
		if (iter == options.end() || iter->second.empty())
			throw CliError(2, "invalid_argument", "Required option missing: " + name);
		return iter->second;
	}

	inline fs::path PathOption(const Options& options, const std::string& name)
	{
		return fs::absolute(RequiredOption(options, name)).lexically_normal();
	}

	inline std::string GetZone(const Options& options)
	{
		std::string zone = RequiredOption(options, "--zone");
		for (char& character : zone)
		{
			if (!((character >= 'a' && character <= 'z')
				|| (character >= 'A' && character <= 'Z')
				|| (character >= '0' && character <= '9')
				|| character == '_'))
			{
				throw CliError(2, "invalid_zone", "Zone short name may contain only letters, digits, and underscores.");
			}
			if (character >= 'A' && character <= 'Z')
				character = static_cast<char>(character - 'A' + 'a');
		}
		return zone;
	}

	inline float NumberOption(const Options& options, const std::string& name, float fallback, float low, float high)
	{
		const auto found = options.find(name);
		if (found == options.end())
			return fallback;
		float value = 0.0f;
		try
		{
			size_t used = 0;
			value = std::stof(found->second, &used);
			if (used != found->second.size())
				throw std::invalid_argument(name);
		}
		catch (const std::exception&)
		{
			throw CliError(2, "bad_option", name + " must be a number.");
		}
		if (!std::isfinite(value) || value < low || value > high)
			throw CliError(2, "bad_option", fmt::format("{} must be from {} to {}.", name, low, high));
		return value;
	}

	// Three numbers separated by commas, all finite.
	inline bool ParseTriple(const std::string& text, float out[3])
	{
		if (sscanf_s(text.c_str(), "%f,%f,%f", &out[0], &out[1], &out[2]) != 3)
			return false;
		return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
	}

	// /loc "Y,X,Z" to mesh axes.
	inline glm::vec3 ParseLoc(const std::string& text, const std::string& name)
	{
		float v[3];
		if (!ParseTriple(text, v))
			throw CliError(2, "bad_points", name + " must be Y,X,Z in /loc order.");
		return glm::vec3(v[1], v[2], v[0]);
	}

	inline std::vector<glm::vec3> ParseLocList(const std::string& text, const std::string& name)
	{
		std::vector<glm::vec3> result;
		std::stringstream stream(text);
		std::string item;
		while (std::getline(stream, item, ';'))
			result.push_back(ParseLoc(item, name));
		if (result.empty())
			throw CliError(2, "bad_points", name + " must be Y,X,Z;Y,X,Z in /loc order.");
		return result;
	}

	inline void GetExtents(const Options& options, float out[3])
	{
		dtVcopy(out, kPluginExtents);
		const auto found = options.find("--extents");
		if (found == options.end())
			return;
		if (!ParseTriple(found->second, out) || out[0] <= 0 || out[1] <= 0 || out[2] <= 0)
			throw CliError(2, "bad_option", "--extents must be three positive numbers X,Y,Z in mesh axes (Y is up).");
	}

	inline rapidjson::Document ReadJsonFile(const std::string& path, const std::string& code)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			throw CliError(3, "missing_" + code, "File not found: " + path);
		const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		rapidjson::Document document;
		document.Parse(text.c_str());
		if (document.HasParseError())
			throw CliError(2, "bad_" + code, "Not valid JSON: " + path);
		return document;
	}

	inline void WriteNewFile(const fs::path& path, const std::string& text)
	{
		std::error_code error;
		if (fs::exists(path, error))
			throw CliError(4, "output_exists", "Refusing to overwrite: " + path.string());
		if (path.has_parent_path())
			fs::create_directories(path.parent_path(), error);
		std::ofstream file(path, std::ios::binary);
		if (!file || !(file << text))
			throw CliError(4, "write_failed", "Could not write: " + path.string());
	}

	//------------------------------------------------------------------------
	// Settings

	struct Setting
	{
		const char* name;
		const char* key;
		float NavMeshConfig::* field;
		float low;
		float high;
		bool whole;
	};

	// Limits follow the MeshGenerator sliders. Detail sample distance is the exception: the slider
	// stops at 0.9 but MQ2Nav's default is 6, and Recast treats anything under 0.9 as off.
	const Setting kSettings[] = {
		{ "--tile-size", "tileSize", &NavMeshConfig::tileSize, 16.0f, 1024.0f, true },
		{ "--cell-size", "cellSize", &NavMeshConfig::cellSize, 0.1f, 1.0f, false },
		{ "--cell-height", "cellHeight", &NavMeshConfig::cellHeight, 0.1f, 1.0f, false },
		{ "--agent-height", "agentHeight", &NavMeshConfig::agentHeight, 0.1f, 15.0f, false },
		{ "--agent-radius", "agentRadius", &NavMeshConfig::agentRadius, 0.1f, 15.0f, false },
		{ "--max-climb", "agentMaxClimb", &NavMeshConfig::agentMaxClimb, 0.1f, 15.0f, false },
		{ "--max-slope", "agentMaxSlope", &NavMeshConfig::agentMaxSlope, 0.0f, 90.0f, false },
		{ "--region-min-size", "regionMinSize", &NavMeshConfig::regionMinSize, 0.0f, 150.0f, false },
		{ "--region-merge-size", "regionMergeSize", &NavMeshConfig::regionMergeSize, 0.0f, 150.0f, false },
		{ "--edge-max-len", "edgeMaxLen", &NavMeshConfig::edgeMaxLen, 0.0f, 50.0f, false },
		{ "--edge-max-error", "edgeMaxError", &NavMeshConfig::edgeMaxError, 0.1f, 3.0f, false },
		{ "--verts-per-poly", "vertsPerPoly", &NavMeshConfig::vertsPerPoly, 3.0f, 12.0f, true },
		{ "--detail-sample-dist", "detailSampleDist", &NavMeshConfig::detailSampleDist, 0.0f, 32.0f, false },
		{ "--detail-sample-max-error", "detailSampleMaxError", &NavMeshConfig::detailSampleMaxError, 0.0f, 100.0f, false },
	};

	template <typename Writer>
	inline void WriteSettings(Writer& w, const NavMeshConfig& config)
	{
		w.StartObject();
		for (const Setting& setting : kSettings)
		{
			w.Key(setting.key);
			w.Double(std::round(static_cast<double>(config.*setting.field) * 10000.0) / 10000.0);
		}
		w.Key("partitionType"); w.Int(static_cast<int>(config.partitionType));
		w.EndObject();
	}

	//------------------------------------------------------------------------
	// Mesh helpers

	inline std::unique_ptr<NavMesh> LoadMesh(const std::string& zone, const fs::path& path)
	{
		std::error_code error;
		if (!fs::is_regular_file(path, error))
			throw CliError(3, "missing_mesh", "Mesh file not found: " + path.string());
		auto mesh = std::make_unique<NavMesh>("", zone);
		const NavMesh::LoadResult result = mesh->LoadNavMeshFile(path.string());
		if (result != NavMesh::LoadResult::Success || !mesh->GetNavMesh())
			throw CliError(5, "invalid_mesh", fmt::format("Mesh could not be loaded (load result {}): {}",
				static_cast<int>(result), path.string()));
		return mesh;
	}

	inline dtQueryFilter MakeFilter(NavMesh& navMesh)
	{
		// The plugin's filter (plugin/NavigationPath.cpp): every flag but Disabled, with the
		// mesh's own area costs.
		dtQueryFilter filter;
		filter.setIncludeFlags(+PolyFlags::All);
		filter.setExcludeFlags(+PolyFlags::Disabled);
		navMesh.FillFilterAreaCosts(filter);
		return filter;
	}

	template <typename Fn>
	inline void ForEachPoly(const dtNavMesh* mesh, Fn&& fn)
	{
		for (int i = 0; i < mesh->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = mesh->getTile(i);
			if (!tile || !tile->header)
				continue;
			const dtPolyRef base = mesh->getPolyRefBase(tile);
			for (int p = 0; p < tile->header->polyCount; ++p)
				fn(base | static_cast<dtPolyRef>(p), tile, &tile->polys[p]);
		}
	}

	inline int CountTiles(const dtNavMesh* mesh)
	{
		if (!mesh)
			return 0;
		int count = 0;
		for (int i = 0; i < mesh->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = mesh->getTile(i);
			if (tile && tile->header)
				++count;
		}
		return count;
	}

	inline int CountPolys(const dtNavMesh* mesh)
	{
		int count = 0;
		if (mesh)
			ForEachPoly(mesh, [&](dtPolyRef, const dtMeshTile*, const dtPoly*) { ++count; });
		return count;
	}

	inline std::array<float, 3> PolyCentre(const dtMeshTile* tile, const dtPoly* poly)
	{
		std::array<float, 3> c{ 0, 0, 0 };
		for (int v = 0; v < poly->vertCount; ++v)
			for (int a = 0; a < 3; ++a)
				c[a] += tile->verts[poly->verts[v] * 3 + a] / poly->vertCount;
		return c;
	}

	inline float PolyAreaXZ(const dtMeshTile* tile, const dtPoly* poly)
	{
		float sum = 0;
		for (int v = 0; v < poly->vertCount; ++v)
		{
			const float* a = &tile->verts[poly->verts[v] * 3];
			const float* b = &tile->verts[poly->verts[(v + 1) % poly->vertCount] * 3];
			sum += a[0] * b[2] - b[0] * a[2];
		}
		return std::fabs(sum) * 0.5f;
	}

	// Every poly reachable from `from` along links that pass the filter.
	inline std::unordered_set<dtPolyRef> Flood(const dtNavMesh* mesh, const dtQueryFilter& filter, dtPolyRef from,
		const std::unordered_set<dtPolyRef>* skip = nullptr)
	{
		std::unordered_set<dtPolyRef> seen{ from };
		std::vector<dtPolyRef> open{ from };
		while (!open.empty())
		{
			const dtPolyRef cur = open.back();
			open.pop_back();
			const dtMeshTile* tile = nullptr;
			const dtPoly* poly = nullptr;
			mesh->getTileAndPolyByRefUnsafe(cur, &tile, &poly);
			for (unsigned int l = poly->firstLink; l != DT_NULL_LINK; l = tile->links[l].next)
			{
				const dtPolyRef next = tile->links[l].ref;
				if (!next || seen.count(next) || (skip && skip->count(next)))
					continue;
				const dtMeshTile* nextTile = nullptr;
				const dtPoly* nextPoly = nullptr;
				mesh->getTileAndPolyByRefUnsafe(next, &nextTile, &nextPoly);
				if (!filter.passFilter(next, nextTile, nextPoly))
					continue;
				seen.insert(next);
				open.push_back(next);
			}
		}
		return seen;
	}

	//------------------------------------------------------------------------
	// Path planning, the same way the plugin does it (plugin/NavigationPath.cpp RecomputePath).

	struct PathResult
	{
		std::string status;
		std::vector<float> verts;
		std::vector<unsigned char> flags;
		std::vector<dtPolyRef> polys;
		float length = 0;
		int links = 0;
	};

	inline PathResult PlanPath(dtNavMeshQuery& query, const dtQueryFilter& filter, const float extents[3],
		const glm::vec3& from, const glm::vec3& to)
	{
		PathResult result;
		dtPolyRef startRef = 0, endRef = 0;
		float spos[3], epos[3];
		query.findNearestPoly(glm::value_ptr(from), extents, &filter, &startRef, spos);
		if (!startRef)
		{
			result.status = "start_not_on_mesh";
			return result;
		}
		query.findNearestPoly(glm::value_ptr(to), extents, &filter, &endRef, epos);
		if (!endRef)
		{
			result.status = "end_not_on_mesh";
			return result;
		}

		int polysSize = kMaxStraightPath;
		std::vector<dtPolyRef> polys(polysSize);
		int numPolys = 0;
		dtStatus status = 0;
		for (int iters = 0; iters < 100; ++iters)
		{
			status = query.findPath(startRef, endRef, spos, epos, &filter, polys.data(), &numPolys, polysSize);
			bool retry = false;
			if (dtStatusDetail(status, DT_OUT_OF_NODES))
			{
				const uint32_t maxNodes = static_cast<uint32_t>(query.getNodePool()->getMaxNodes());
				const uint32_t newMaxNodes = (std::min<uint32_t>)(static_cast<uint32_t>(maxNodes * kNodePoolGrowth),
					(std::min<uint32_t>)(DT_NULL_IDX, 1 << DT_NODE_PARENT_BITS) - 1);
				if (maxNodes != newMaxNodes && newMaxNodes < static_cast<uint32_t>(kNodePoolMaxSize))
				{
					query.init(query.getAttachedNavMesh(), newMaxNodes);
					retry = true;
				}
			}
			if (dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
			{
				const int newSize = static_cast<int>(polysSize * kNodePoolGrowth);
				if (newSize < kNodePoolMaxSize)
				{
					polysSize = newSize;
					polys.assign(polysSize, 0);
					retry = true;
				}
			}
			if (!retry)
				break;
		}

		if (dtStatusFailed(status))
			result.status = "failed";
		else if (dtStatusDetail(status, DT_OUT_OF_NODES) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
			result.status = "too_far";
		else if ((numPolys > 0 && polys[numPolys - 1] != endRef) || dtStatusDetail(status, DT_PARTIAL_RESULT))
			result.status = "partial";
		if (!result.status.empty() || numPolys == 0)
		{
			if (result.status.empty())
				result.status = "failed";
			return result;
		}

		result.verts.resize(kMaxStraightPath * 3);
		result.flags.resize(kMaxStraightPath);
		result.polys.resize(kMaxStraightPath);
		int count = 0;
		query.findStraightPath(spos, epos, polys.data(), numPolys, result.verts.data(), result.flags.data(),
			result.polys.data(), &count, kMaxStraightPath, DT_STRAIGHTPATH_AREA_CROSSINGS);
		result.verts.resize(count * 3);
		result.flags.resize(count);
		result.polys.resize(count);
		for (int i = 1; i < count; ++i)
			result.length += dtVdist(&result.verts[(i - 1) * 3], &result.verts[i * 3]);
		for (int i = 0; i < count; ++i)
			if (result.flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
				++result.links;
		result.status = "found";
		return result;
	}

	//------------------------------------------------------------------------
	// Fixes: off-mesh links, area volumes and area types, read and written as JSON.

	template <typename Writer>
	inline void WriteConnection(Writer& w, const OffMeshConnection& c)
	{
		w.StartObject();
		w.Key("name"); w.String(c.name.c_str());
		w.Key("start"); WriteVec(w, c.start);
		w.Key("end"); WriteVec(w, c.end);
		w.Key("bidirectional"); w.Bool(c.bidirectional);
		w.Key("area"); w.Uint(c.areaType);
		w.Key("type"); w.Uint(static_cast<unsigned>(c.type));
		w.EndObject();
	}

	template <typename Writer>
	inline void WriteVolume(Writer& w, const ConvexVolume& v)
	{
		w.StartObject();
		w.Key("name"); w.String(v.name.c_str());
		w.Key("verts");
		w.StartArray();
		for (const auto& p : v.verts)
			WriteVec(w, p);
		w.EndArray();
		w.Key("hmin"); w.Double(v.hmin);
		w.Key("hmax"); w.Double(v.hmax);
		w.Key("area"); w.Uint(v.areaType);
		w.EndObject();
	}

	template <typename Writer>
	inline void WriteArea(Writer& w, const PolyAreaType& a)
	{
		w.StartObject();
		w.Key("id"); w.Uint(a.id);
		w.Key("name"); w.String(a.name.c_str());
		w.Key("cost"); w.Double(a.cost);
		w.Key("flags"); w.Uint(a.flags);
		w.Key("color"); w.Uint(a.color);
		w.Key("selectable"); w.Bool(a.selectable);
		w.EndObject();
	}

} // namespace nav_cli
