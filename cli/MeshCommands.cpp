//
// MeshCommands.cpp
//
// MeshTool commands that read navmeshes: validate, export and diff.
//

#include "MeshCommands.h"

#include "common/NavMesh.h"
#include "common/NavMeshData.h"

#include <DetourNavMesh.h>
#include <fmt/format.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

struct MeshSetting
{
	const char* name;
	float NavMeshConfig::* field;
};

const MeshSetting s_settings[] = {
	{ "tileSize", &NavMeshConfig::tileSize },
	{ "cellSize", &NavMeshConfig::cellSize },
	{ "cellHeight", &NavMeshConfig::cellHeight },
	{ "agentHeight", &NavMeshConfig::agentHeight },
	{ "agentRadius", &NavMeshConfig::agentRadius },
	{ "agentMaxClimb", &NavMeshConfig::agentMaxClimb },
	{ "agentMaxSlope", &NavMeshConfig::agentMaxSlope },
	{ "regionMinSize", &NavMeshConfig::regionMinSize },
	{ "regionMergeSize", &NavMeshConfig::regionMergeSize },
	{ "edgeMaxLen", &NavMeshConfig::edgeMaxLen },
	{ "edgeMaxError", &NavMeshConfig::edgeMaxError },
	{ "vertsPerPoly", &NavMeshConfig::vertsPerPoly },
	{ "detailSampleDist", &NavMeshConfig::detailSampleDist },
	{ "detailSampleMaxError", &NavMeshConfig::detailSampleMaxError },
};

double Round(float value, double scale)
{
	return std::round(static_cast<double>(value) * scale) / scale;
}

// Mesh axes are x, y (up), z. /loc order is y, x, z, so a mesh point reads as (z, x, y).
std::string LocText(const glm::vec3& p)
{
	return fmt::format("{:.1f}, {:.1f}, {:.1f}", p.z, p.x, p.y);
}

template <typename Writer>
void WriteLoc(Writer& w, const glm::vec3& p)
{
	w.StartArray();
	w.Double(Round(p.z, 10));
	w.Double(Round(p.x, 10));
	w.Double(Round(p.y, 10));
	w.EndArray();
}

template <typename Writer>
void WriteVec(Writer& w, const glm::vec3& p)
{
	w.StartArray();
	w.Double(p.x);
	w.Double(p.y);
	w.Double(p.z);
	w.EndArray();
}

template <typename Writer>
void WriteSettings(Writer& w, const NavMeshConfig& config)
{
	w.StartObject();
	for (const MeshSetting& setting : s_settings)
	{
		w.Key(setting.name);
		w.Double(Round(config.*setting.field, 10000));
	}
	w.Key("partitionType");
	w.Int(static_cast<int>(config.partitionType));
	w.EndObject();
}

void PrintJsonError(const std::string& message)
{
	rapidjson::StringBuffer buffer;
	JsonWriter w(buffer);
	w.StartObject();
	w.Key("ok"); w.Bool(false);
	w.Key("error"); w.String(message.c_str());
	w.EndObject();
	fmt::print("{}\n", buffer.GetString());
}

// MeshTool logs to stdout, so with --json the error is printed only as JSON.
int Fail(bool json, const std::string& message)
{
	if (json)
		PrintJsonError(message);
	else
		SPDLOG_ERROR("{}", message);
	return 1;
}

std::unique_ptr<NavMesh> LoadMesh(const std::string& path, std::string& error)
{
	std::error_code ec;
	if (!fs::is_regular_file(path, ec))
	{
		error = fmt::format("Missing navmesh file: {}", path);
		return nullptr;
	}

	auto navMesh = std::make_unique<NavMesh>();
	switch (navMesh->LoadNavMeshFile(path))
	{
	case NavMesh::LoadResult::Success:
		break;
	case NavMesh::LoadResult::Corrupt:
		error = fmt::format("Navmesh is corrupt: {}", path);
		return nullptr;
	case NavMesh::LoadResult::VersionMismatch:
		error = fmt::format("Navmesh has an incompatible version: {}", path);
		return nullptr;
	case NavMesh::LoadResult::OutOfMemory:
		error = fmt::format("Out of memory loading: {}", path);
		return nullptr;
	default:
		error = fmt::format("Navmesh could not be loaded: {}", path);
		return nullptr;
	}

	if (!navMesh->GetNavMesh())
	{
		error = fmt::format("Navmesh has no tiles: {}", path);
		return nullptr;
	}
	return navMesh;
}

template <typename Fn>
void ForEachTile(const dtNavMesh* mesh, Fn&& fn)
{
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (tile && tile->header)
			fn(tile);
	}
}

struct MeshCounts
{
	int tiles = 0;
	int polys = 0;
	double walkableArea = 0;
};

MeshCounts CountMesh(const dtNavMesh* mesh)
{
	MeshCounts counts;
	ForEachTile(mesh, [&](const dtMeshTile* tile) {
		++counts.tiles;
		counts.polys += tile->header->polyCount;
		for (int p = 0; p < tile->header->polyCount; ++p)
		{
			const dtPoly& poly = tile->polys[p];
			if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION || (poly.flags & +PolyFlags::Disabled))
				continue;

			float sum = 0;
			for (int v = 0; v < poly.vertCount; ++v)
			{
				const float* a = &tile->verts[poly.verts[v] * 3];
				const float* b = &tile->verts[poly.verts[(v + 1) % poly.vertCount] * 3];
				sum += a[0] * b[2] - b[0] * a[2];
			}
			counts.walkableArea += std::fabs(sum) * 0.5;
		}
	});
	return counts;
}

} // namespace

//----------------------------------------------------------------------------

int ValidateMesh(const std::string& meshPath, bool json)
{
	std::string error;
	auto navMesh = LoadMesh(meshPath, error);
	if (!navMesh)
		return Fail(json, error);

	const MeshCounts counts = CountMesh(navMesh->GetNavMesh().get());
	if (counts.tiles == 0)
		return Fail(json, fmt::format("Navmesh has no tiles: {}", meshPath));
	if (!navMesh->GetNavMeshQuery())
		return Fail(json, fmt::format("Navmesh loaded but cannot be queried: {}", meshPath));

	if (json)
	{
		rapidjson::StringBuffer buffer;
		JsonWriter w(buffer);
		w.StartObject();
		w.Key("ok"); w.Bool(true);
		w.Key("mesh"); w.String(meshPath.c_str());
		w.Key("headerVersion"); w.Int(navMesh->GetHeaderVersion());
		w.Key("tiles"); w.Int(counts.tiles);
		w.Key("polys"); w.Int(counts.polys);
		w.Key("walkableArea"); w.Double(std::round(counts.walkableArea));
		w.Key("connections"); w.Uint64(navMesh->GetConnectionCount());
		w.Key("volumes"); w.Uint64(navMesh->GetConvexVolumeCount());
		w.Key("settings"); WriteSettings(w, navMesh->GetNavMeshConfig());
		w.EndObject();
		fmt::print("{}\n", buffer.GetString());
	}
	else
	{
		fmt::print("{}: valid\n", meshPath);
		fmt::print("  version {}, {} tiles, {} polygons, walkable area {:.0f}\n",
			navMesh->GetHeaderVersion(), counts.tiles, counts.polys, counts.walkableArea);
		fmt::print("  {} off-mesh connections, {} convex volumes\n",
			navMesh->GetConnectionCount(), navMesh->GetConvexVolumeCount());
	}
	return 0;
}

//----------------------------------------------------------------------------

int ExportMesh(const std::string& meshPath, const std::string& outPath, bool json)
{
	std::error_code ec;
	if (fs::exists(outPath, ec))
		return Fail(json, fmt::format("Refusing to overwrite: {}", outPath));

	std::string error;
	auto navMesh = LoadMesh(meshPath, error);
	if (!navMesh)
		return Fail(json, error);

	std::vector<const PolyAreaType*> areas(navMesh->GetPolyAreas().begin(), navMesh->GetPolyAreas().end());
	std::sort(areas.begin(), areas.end(), [](const PolyAreaType* a, const PolyAreaType* b) { return a->id < b->id; });

	rapidjson::StringBuffer fileBuffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> w(fileBuffer);
	w.StartObject();
	w.Key("schemaVersion"); w.Int(1);
	w.Key("sourceMesh"); w.String(meshPath.c_str());
	w.Key("headerVersion"); w.Int(navMesh->GetHeaderVersion());
	w.Key("settings"); WriteSettings(w, navMesh->GetNavMeshConfig());

	w.Key("connections");
	w.StartArray();
	for (const auto& c : navMesh->GetConnections())
	{
		w.StartObject();
		w.Key("name"); w.String(c->name.c_str());
		w.Key("start"); WriteVec(w, c->start);
		w.Key("end"); WriteVec(w, c->end);
		w.Key("bidirectional"); w.Bool(c->bidirectional);
		w.Key("area"); w.Uint(c->areaType);
		w.Key("type"); w.Uint(static_cast<unsigned>(c->type));
		w.EndObject();
	}
	w.EndArray();

	w.Key("volumes");
	w.StartArray();
	for (const auto& v : navMesh->GetConvexVolumes())
	{
		w.StartObject();
		w.Key("name"); w.String(v->name.c_str());
		w.Key("verts");
		w.StartArray();
		for (const glm::vec3& p : v->verts)
			WriteVec(w, p);
		w.EndArray();
		w.Key("hmin"); w.Double(v->hmin);
		w.Key("hmax"); w.Double(v->hmax);
		w.Key("area"); w.Uint(v->areaType);
		w.EndObject();
	}
	w.EndArray();

	w.Key("areas");
	w.StartArray();
	for (const PolyAreaType* a : areas)
	{
		w.StartObject();
		w.Key("id"); w.Uint(a->id);
		w.Key("name"); w.String(a->name.c_str());
		w.Key("cost"); w.Double(a->cost);
		w.Key("flags"); w.Uint(a->flags);
		w.Key("color"); w.Uint(a->color);
		w.Key("selectable"); w.Bool(a->selectable);
		w.EndObject();
	}
	w.EndArray();
	w.EndObject();

	{
		const fs::path out = fs::absolute(outPath);
		if (out.has_parent_path())
			fs::create_directories(out.parent_path(), ec);
		std::ofstream file(out, std::ios::binary);
		if (!file || !(file << fileBuffer.GetString() << "\n"))
			return Fail(json, fmt::format("Could not write: {}", outPath));
	}

	if (json)
	{
		rapidjson::StringBuffer buffer;
		JsonWriter j(buffer);
		j.StartObject();
		j.Key("ok"); j.Bool(true);
		j.Key("out"); j.String(outPath.c_str());
		j.Key("connections"); j.Uint64(navMesh->GetConnectionCount());
		j.Key("volumes"); j.Uint64(navMesh->GetConvexVolumeCount());
		j.Key("areas"); j.Uint64(areas.size());
		j.EndObject();
		fmt::print("{}\n", buffer.GetString());
	}
	else
	{
		fmt::print("Exported {} off-mesh connections, {} convex volumes and {} area types to {}\n",
			navMesh->GetConnectionCount(), navMesh->GetConvexVolumeCount(), areas.size(), outPath);
	}
	return 0;
}

//----------------------------------------------------------------------------

namespace {

struct TileKey
{
	int x, y, layer;
	bool operator<(const TileKey& o) const { return std::tie(x, y, layer) < std::tie(o.x, o.y, o.layer); }
};

struct TileInfo
{
	uint64_t signature;
	std::array<float, 4> bounds; // min x, min z, max x, max z
};

std::map<TileKey, TileInfo> TileSignatures(const dtNavMesh* mesh)
{
	std::map<TileKey, TileInfo> out;
	ForEachTile(mesh, [&](const dtMeshTile* tile) {
		uint64_t hash = 1469598103934665603ull;
		const auto mix = [&hash](const void* data, size_t size) {
			const unsigned char* bytes = static_cast<const unsigned char*>(data);
			for (size_t b = 0; b < size; ++b)
			{
				hash ^= bytes[b];
				hash *= 1099511628211ull;
			}
		};
		mix(&tile->header->polyCount, sizeof(int));
		for (int v = 0; v < tile->header->vertCount * 3; ++v)
		{
			const int rounded = static_cast<int>(std::lround(tile->verts[v] * 10.0f));
			mix(&rounded, sizeof(rounded));
		}
		out[{ tile->header->x, tile->header->y, tile->header->layer }] = { hash,
			{ tile->header->bmin[0], tile->header->bmin[2], tile->header->bmax[0], tile->header->bmax[2] } };
	});
	return out;
}

// Same outline whatever vertex the list starts at, in either winding.
bool SameOutline(const ConvexVolume& a, const ConvexVolume& b, float tolerance)
{
	const size_t count = a.verts.size();
	if (count != b.verts.size())
		return false;

	for (size_t shift = 0; shift < count; ++shift)
	{
		for (int dir : { 1, -1 })
		{
			bool all = true;
			for (size_t v = 0; v < count && all; ++v)
			{
				const size_t k = (shift + count + dir * static_cast<long long>(v)) % count;
				all = glm::distance(a.verts[v], b.verts[k]) <= tolerance;
			}
			if (all)
				return true;
		}
	}
	return false;
}

} // namespace

int DiffMeshes(const std::string& oldPath, const std::string& newPath, float tolerance, bool strict, bool json)
{
	std::string error;
	auto oldMesh = LoadMesh(oldPath, error);
	if (!oldMesh)
		return Fail(json, error);
	auto newMesh = LoadMesh(newPath, error);
	if (!newMesh)
		return Fail(json, error);

	const auto closeTo = [tolerance](const glm::vec3& a, const glm::vec3& b) { return glm::distance(a, b) <= tolerance; };

	// Off-mesh connections match by their end points, either way round when both are two-way.
	const auto& oldLinks = oldMesh->GetConnections();
	const auto& newLinks = newMesh->GetConnections();
	std::vector<bool> linkUsed(newLinks.size(), false);
	std::vector<const OffMeshConnection*> lostLinks, changedLinks, addedLinks;
	for (const auto& o : oldLinks)
	{
		bool found = false;
		for (size_t i = 0; i < newLinks.size() && !found; ++i)
		{
			const auto& n = newLinks[i];
			if (linkUsed[i])
				continue;

			const bool same = closeTo(o->start, n->start) && closeTo(o->end, n->end);
			const bool swapped = o->bidirectional && n->bidirectional && closeTo(o->start, n->end) && closeTo(o->end, n->start);
			if (same || swapped)
			{
				linkUsed[i] = found = true;
				if (o->bidirectional != n->bidirectional || o->areaType != n->areaType || o->name != n->name || o->type != n->type)
					changedLinks.push_back(o.get());
			}
		}
		if (!found)
			lostLinks.push_back(o.get());
	}
	for (size_t i = 0; i < newLinks.size(); ++i)
	{
		if (!linkUsed[i])
			addedLinks.push_back(newLinks[i].get());
	}

	// Convex volumes match by outline and height range.
	const auto& oldVols = oldMesh->GetConvexVolumes();
	const auto& newVols = newMesh->GetConvexVolumes();
	std::vector<bool> volUsed(newVols.size(), false);
	std::vector<const ConvexVolume*> lostVols, changedVols, addedVols;
	for (const auto& o : oldVols)
	{
		bool found = false;
		for (size_t i = 0; i < newVols.size() && !found; ++i)
		{
			const auto& n = newVols[i];
			if (volUsed[i] || std::fabs(n->hmin - o->hmin) > tolerance || std::fabs(n->hmax - o->hmax) > tolerance
				|| !SameOutline(*o, *n, tolerance))
			{
				continue;
			}

			volUsed[i] = found = true;
			if (o->areaType != n->areaType || o->name != n->name)
				changedVols.push_back(o.get());
		}
		if (!found)
			lostVols.push_back(o.get());
	}
	for (size_t i = 0; i < newVols.size(); ++i)
	{
		if (!volUsed[i])
			addedVols.push_back(newVols[i].get());
	}

	// Area types match by id.
	std::map<int, const PolyAreaType*> oldAreas, newAreas;
	for (const PolyAreaType* a : oldMesh->GetPolyAreas())
		oldAreas[a->id] = a;
	for (const PolyAreaType* a : newMesh->GetPolyAreas())
		newAreas[a->id] = a;
	std::vector<int> lostAreas, addedAreas, changedAreas;
	for (const auto& [id, a] : oldAreas)
	{
		const auto it = newAreas.find(id);
		if (it == newAreas.end())
			lostAreas.push_back(id);
		else if (!(*a == *it->second))
			changedAreas.push_back(id);
	}
	for (const auto& [id, a] : newAreas)
	{
		if (!oldAreas.count(id))
			addedAreas.push_back(id);
	}

	// Settings.
	std::vector<std::tuple<const char*, float, float>> settingsChanged;
	for (const MeshSetting& s : s_settings)
	{
		const float a = oldMesh->GetNavMeshConfig().*s.field;
		const float b = newMesh->GetNavMeshConfig().*s.field;
		if (std::fabs(a - b) > 1e-4f)
			settingsChanged.emplace_back(s.name, a, b);
	}

	// Tile geometry. Hand-edited tiles that a rebuild cannot reproduce show up here even when no
	// connection or volume was lost.
	const auto oldTiles = TileSignatures(oldMesh->GetNavMesh().get());
	const auto newTiles = TileSignatures(newMesh->GetNavMesh().get());
	std::vector<std::pair<TileKey, TileInfo>> changedTiles;
	size_t tilesOnlyOld = 0, tilesOnlyNew = 0;
	for (const auto& [key, info] : oldTiles)
	{
		const auto it = newTiles.find(key);
		if (it == newTiles.end())
			++tilesOnlyOld;
		else if (it->second.signature != info.signature)
			changedTiles.emplace_back(key, info);
	}
	for (const auto& [key, info] : newTiles)
	{
		if (!oldTiles.count(key))
			++tilesOnlyNew;
	}

	const MeshCounts oldCounts = CountMesh(oldMesh->GetNavMesh().get());
	const MeshCounts newCounts = CountMesh(newMesh->GetNavMesh().get());
	const size_t lost = lostLinks.size() + lostVols.size() + lostAreas.size();
	const bool geometryChanged = !changedTiles.empty() || tilesOnlyOld > 0 || tilesOnlyNew > 0;
	const bool failed = lost > 0 || (strict && (geometryChanged || !settingsChanged.empty()));

	if (json)
	{
		rapidjson::StringBuffer buffer;
		JsonWriter w(buffer);
		const auto writeLinks = [&w](const char* key, const std::vector<const OffMeshConnection*>& list) {
			w.Key(key);
			w.StartArray();
			for (const OffMeshConnection* c : list)
			{
				w.StartObject();
				w.Key("name"); w.String(c->name.c_str());
				w.Key("startLoc"); WriteLoc(w, c->start);
				w.Key("endLoc"); WriteLoc(w, c->end);
				w.EndObject();
			}
			w.EndArray();
		};
		const auto writeVols = [&w](const char* key, const std::vector<const ConvexVolume*>& list) {
			w.Key(key);
			w.StartArray();
			for (const ConvexVolume* v : list)
			{
				w.StartObject();
				w.Key("name"); w.String(v->name.c_str());
				w.Key("area"); w.Uint(v->areaType);
				w.Key("firstVertLoc"); WriteLoc(w, v->verts.front());
				w.EndObject();
			}
			w.EndArray();
		};
		const auto writeIds = [&w](const char* key, const std::vector<int>& ids) {
			w.Key(key);
			w.StartArray();
			for (int id : ids)
				w.Int(id);
			w.EndArray();
		};

		w.StartObject();
		w.Key("ok"); w.Bool(true);
		w.Key("lost"); w.Uint64(lost);
		w.Key("tolerance"); w.Double(tolerance);
		w.Key("tiles"); w.StartObject(); w.Key("old"); w.Int(oldCounts.tiles); w.Key("new"); w.Int(newCounts.tiles); w.EndObject();
		w.Key("polys"); w.StartObject(); w.Key("old"); w.Int(oldCounts.polys); w.Key("new"); w.Int(newCounts.polys); w.EndObject();
		w.Key("headerVersion"); w.StartObject();
		w.Key("old"); w.Int(oldMesh->GetHeaderVersion()); w.Key("new"); w.Int(newMesh->GetHeaderVersion());
		w.EndObject();
		w.Key("settingsChanged");
		w.StartArray();
		for (const auto& [name, a, b] : settingsChanged)
		{
			w.StartObject();
			w.Key("setting"); w.String(name);
			w.Key("old"); w.Double(a);
			w.Key("new"); w.Double(b);
			w.EndObject();
		}
		w.EndArray();
		w.Key("tileGeometry");
		w.StartObject();
		w.Key("changed"); w.Uint64(changedTiles.size());
		w.Key("onlyOld"); w.Uint64(tilesOnlyOld);
		w.Key("onlyNew"); w.Uint64(tilesOnlyNew);
		w.Key("changedTiles");
		w.StartArray();
		for (size_t i = 0; i < changedTiles.size() && i < 50; ++i)
		{
			const auto& [key, info] = changedTiles[i];
			w.StartObject();
			w.Key("tile"); w.StartArray(); w.Int(key.x); w.Int(key.y); w.Int(key.layer); w.EndArray();
			w.Key("locY"); w.StartArray(); w.Double(Round(info.bounds[1], 10)); w.Double(Round(info.bounds[3], 10)); w.EndArray();
			w.Key("locX"); w.StartArray(); w.Double(Round(info.bounds[0], 10)); w.Double(Round(info.bounds[2], 10)); w.EndArray();
			w.EndObject();
		}
		w.EndArray();
		w.EndObject();
		w.Key("connections");
		w.StartObject();
		w.Key("old"); w.Uint64(oldLinks.size());
		w.Key("new"); w.Uint64(newLinks.size());
		writeLinks("lost", lostLinks);
		writeLinks("added", addedLinks);
		writeLinks("changed", changedLinks);
		w.EndObject();
		w.Key("volumes");
		w.StartObject();
		w.Key("old"); w.Uint64(oldVols.size());
		w.Key("new"); w.Uint64(newVols.size());
		writeVols("lost", lostVols);
		writeVols("added", addedVols);
		writeVols("changed", changedVols);
		w.EndObject();
		w.Key("areas");
		w.StartObject();
		writeIds("lost", lostAreas);
		writeIds("added", addedAreas);
		writeIds("changed", changedAreas);
		w.EndObject();
		w.EndObject();
		fmt::print("{}\n", buffer.GetString());
	}
	else
	{
		fmt::print("Comparing {}\n       to {}\n", oldPath, newPath);
		fmt::print("Off-mesh connections: {} -> {} ({} lost, {} added, {} changed)\n",
			oldLinks.size(), newLinks.size(), lostLinks.size(), addedLinks.size(), changedLinks.size());
		for (const OffMeshConnection* c : lostLinks)
			fmt::print("  lost connection \"{}\" from {} to {}\n", c->name, LocText(c->start), LocText(c->end));
		fmt::print("Convex volumes: {} -> {} ({} lost, {} added, {} changed)\n",
			oldVols.size(), newVols.size(), lostVols.size(), addedVols.size(), changedVols.size());
		for (const ConvexVolume* v : lostVols)
			fmt::print("  lost volume \"{}\" (area {}) at {}\n", v->name, v->areaType, LocText(v->verts.front()));
		fmt::print("Area types: {} lost, {} added, {} changed\n", lostAreas.size(), addedAreas.size(), changedAreas.size());
		if (settingsChanged.empty())
			fmt::print("Settings: unchanged\n");
		for (const auto& [name, a, b] : settingsChanged)
			fmt::print("Settings: {} {} -> {}\n", name, a, b);
		fmt::print("Tiles: {} -> {}, polygons {} -> {}, {} tiles with changed geometry\n",
			oldCounts.tiles, newCounts.tiles, oldCounts.polys, newCounts.polys, changedTiles.size() + tilesOnlyOld + tilesOnlyNew);
		fmt::print("{}\n", lost > 0 ? fmt::format("{} hand-made items were lost", lost)
			: failed ? std::string("Nothing was lost, but settings or tile geometry changed")
			: std::string("Nothing was lost"));
	}

	return failed ? 2 : 0;
}
