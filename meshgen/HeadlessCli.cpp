//
// HeadlessCli.cpp
//

#include "meshgen/HeadlessCli.h"

#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "meshgen/InputGeom.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshTool.h"

#include <DetourNavMesh.h>
#include <Recast.h>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <Windows.h>

namespace fs = std::filesystem;

namespace
{
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

	class ConsoleRecastContext : public rcContext
	{
	public:
		ConsoleRecastContext()
			: rcContext(false)
		{
			enableLog(true);
		}

	protected:
		void doLog(const rcLogCategory category, const char* message, const int length) override
		{
			auto logger = spdlog::default_logger();
			if (!logger)
				return;

			spdlog::level::level_enum level = spdlog::level::info;
			if (category == RC_LOG_WARNING)
				level = spdlog::level::warn;
			else if (category == RC_LOG_ERROR)
				level = spdlog::level::err;

			logger->log(level, spdlog::string_view_t(message, static_cast<size_t>(length)));
		}
	};

	using Options = std::unordered_map<std::string, std::string>;

	std::string JsonEscape(const std::string& value)
	{
		std::ostringstream escaped;
		for (unsigned char character : value)
		{
			switch (character)
			{
			case '"': escaped << "\\\""; break;
			case '\\': escaped << "\\\\"; break;
			case '\b': escaped << "\\b"; break;
			case '\f': escaped << "\\f"; break;
			case '\n': escaped << "\\n"; break;
			case '\r': escaped << "\\r"; break;
			case '\t': escaped << "\\t"; break;
			default:
				if (character < 0x20)
				{
					escaped << fmt::format("\\u{:04x}", static_cast<unsigned int>(character));
				}
				else
				{
					escaped << static_cast<char>(character);
				}
			}
		}
		return escaped.str();
	}

	void PrintError(const std::string& code, const std::string& message)
	{
		fmt::print("{{\"ok\":false,\"error\":\"{}\",\"message\":\"{}\"}}\n",
			JsonEscape(code), JsonEscape(message));
	}

	void PrintUsage()
	{
		fmt::print(
			"MeshGenerator headless commands:\n"
			"  MeshGenerator.exe build --zone SHORTNAME --eq-path PATH --stage-root PATH [--mesh-root PATH] [--no-max-extents]\n"
			"  MeshGenerator.exe validate --zone SHORTNAME --mesh PATH\n"
			"\n"
			"Build writes <stage-root>\\MQ2Nav\\<zone>.navmesh and never overwrites an existing file.\n"
			"Progress and diagnostics are written to stderr; each command writes one JSON result to stdout.\n");
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
				PrintUsage();
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

	std::string RequiredOption(const Options& options, const std::string& name)
	{
		auto iter = options.find(name);
		if (iter == options.end() || iter->second.empty())
			throw CliError(2, "invalid_argument", "Required option missing: " + name);
		return iter->second;
	}

	std::string GetZone(const Options& options)
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

	int CountTiles(const std::shared_ptr<dtNavMesh>& navMesh)
	{
		if (!navMesh)
			return 0;

		int count = 0;
		for (int index = 0; index < navMesh->getMaxTiles(); ++index)
		{
			const dtMeshTile* tile = const_cast<const dtNavMesh*>(navMesh.get())->getTile(index);
			if (tile && tile->header)
				++count;
		}
		return count;
	}

	int ValidateMesh(const Options& options)
	{
		const std::string zone = GetZone(options);
		const fs::path meshPath = fs::absolute(RequiredOption(options, "--mesh")).lexically_normal();
		std::error_code error;
		if (!fs::is_regular_file(meshPath, error))
			throw CliError(3, "missing_mesh", "Mesh file not found: " + meshPath.string());

		NavMesh navMesh("", zone);
		const NavMesh::LoadResult result = navMesh.LoadNavMeshFile(meshPath.string());
		if (result != NavMesh::LoadResult::Success)
			throw CliError(5, "invalid_mesh", fmt::format("Mesh validation failed (load result {}).", static_cast<int>(result)));

		const int tileCount = CountTiles(navMesh.GetNavMesh());
		if (tileCount == 0)
			throw CliError(5, "empty_mesh", "Mesh loaded successfully but contains no tiles.");

		if (!navMesh.GetNavMeshQuery())
			throw CliError(5, "invalid_mesh", "Mesh loaded but its navigation query could not be initialized.");

		fmt::print("{{\"ok\":true,\"command\":\"validate\",\"zone\":\"{}\",\"mesh\":\"{}\",\"tiles\":{},\"headerVersion\":{}}}\n",
			JsonEscape(zone), JsonEscape(meshPath.string()), tileCount, navMesh.GetHeaderVersion());
		return 0;
	}

	int BuildMesh(const Options& options)
	{
		const std::string zone = GetZone(options);
		const fs::path eqPath = fs::absolute(RequiredOption(options, "--eq-path")).lexically_normal();
		const fs::path stageRoot = fs::absolute(RequiredOption(options, "--stage-root")).lexically_normal();
		const fs::path meshRoot = fs::absolute(options.count("--mesh-root")
			? options.at("--mesh-root") : eqPath.string()).lexically_normal();

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

		fs::create_directories(outputDirectory);

		auto navMesh = std::make_shared<NavMesh>(outputDirectory.string(), zone);
		ConsoleRecastContext context;
		auto geometry = std::make_unique<InputGeom>(zone, eqPath.string(), meshRoot.string());
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
		navMeshTool.UpdateTileSizes();
		spdlog::info("Building zone {} synchronously.", zone);
		if (!navMeshTool.handleBuild(false, false))
			throw CliError(4, "build_failed", "MQ2Nav failed to initialize the navmesh build.");

		const int tileCount = CountTiles(navMesh->GetNavMesh());
		if (tileCount == 0)
			throw CliError(4, "empty_build", "Build completed without producing any navmesh tiles.");

		const fs::path temporaryPath = outputPath.string() + ".building";
		if (fs::exists(temporaryPath, error))
			throw CliError(4, "temporary_exists", "Refusing to overwrite temporary build file: " + temporaryPath.string());

		if (!navMesh->SaveNavMeshFile(temporaryPath.string()))
		{
			fs::remove(temporaryPath, error);
			throw CliError(4, "save_failed", "Could not save the staged mesh.");
		}

		NavMesh validation("", zone);
		if (validation.LoadNavMeshFile(temporaryPath.string()) != NavMesh::LoadResult::Success
			|| CountTiles(validation.GetNavMesh()) != tileCount
			|| !validation.GetNavMeshQuery())
		{
			fs::remove(temporaryPath, error);
			throw CliError(5, "staged_validation_failed", "The staged mesh did not pass MQ2Nav load/query validation.");
		}

		fs::rename(temporaryPath, outputPath);
		fmt::print("{{\"ok\":true,\"command\":\"build\",\"zone\":\"{}\",\"mesh\":\"{}\",\"tiles\":{},\"validation\":\"passed\"}}\n",
			JsonEscape(zone), JsonEscape(outputPath.string()), tileCount);
		return 0;
	}
}

int RunHeadlessCli(int argc, char* argv[])
{
	auto logger = spdlog::stderr_color_mt("MeshGeneratorCLI");
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("%^%l:%$ %v");
	spdlog::set_level(spdlog::level::info);

	try
	{
		if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
		{
			PrintUsage();
			return 0;
		}

		const std::string command = argv[1];
		if (command == "build")
		{
			const Options options = ParseOptions(argc, argv,
				{ "--zone", "--eq-path", "--stage-root", "--mesh-root" },
				{ "--no-max-extents" });
			return BuildMesh(options);
		}
		if (command == "validate")
		{
			const Options options = ParseOptions(argc, argv, { "--zone", "--mesh" }, {});
			return ValidateMesh(options);
		}

		throw CliError(2, "unknown_command", "Unknown headless command: " + command);
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
