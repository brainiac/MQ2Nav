//
// Main.cpp
//

#include <iostream>
#include <args/args.hxx>

#include "common/NavMesh.h"
#include "MeshCommands.h"

#include <filesystem>
#include <fmt/format.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
	args::ArgumentParser parser("MeshTool", "For help about a command, run MeshTool <command> -h");

	args::Group commands(parser, "commands");
	args::Command convert(commands, "convert", "Convert mesh from one format to another");
		args::Positional<std::string> inputMesh(convert, "input", "Input navmesh file to load", args::Options::Required);
		args::Positional<std::string> outputMesh(convert, "output", "Output navmesh file to save");
		args::ValueFlag<int> meshVersion(convert, "version", "Navmesh version to save (defaults to latest)", { "version" }, (int)NavMeshHeaderVersion::Latest);
	args::Command validate(commands, "validate", "Load a navmesh and report its tiles, polygons, connections and volumes");
		args::Positional<std::string> validateMesh(validate, "mesh", "Navmesh file to check", args::Options::Required);
		args::Flag validateJson(validate, "json", "Print the result as JSON", { "json" });
	args::Command exportCmd(commands, "export", "Write a navmesh's off-mesh connections, convex volumes, area types and settings to JSON");
		args::Positional<std::string> exportMesh(exportCmd, "mesh", "Navmesh file to read", args::Options::Required);
		args::Positional<std::string> exportOut(exportCmd, "output", "JSON file to write (never overwritten)", args::Options::Required);
		args::Flag exportJson(exportCmd, "json", "Print the result as JSON", { "json" });
	args::Command diff(commands, "diff", "Compare two navmeshes: connections, volumes, area types, settings and tile geometry");
		args::Positional<std::string> diffOld(diff, "old", "Navmesh to compare from", args::Options::Required);
		args::Positional<std::string> diffNew(diff, "new", "Navmesh to compare to", args::Options::Required);
		args::ValueFlag<float> diffTolerance(diff, "distance", "How far apart matching points may be (default 0.5)", { "tolerance" }, 0.5f);
		args::Flag diffStrict(diff, "strict", "Also fail when settings or tile geometry changed", { "strict" });
		args::Flag diffJson(diff, "json", "Print the result as JSON", { "json" });

	args::Group arguments("arguments");
	args::GlobalOptions globals(parser, arguments);
		args::HelpFlag help(arguments, "help", "Display this help menu", { 'h', "help" });
		args::Flag verbose(arguments, "verbose", "Display verbose output", { 'v', "verbose" });

	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (args::Help)
	{
		std::cout << parser;
		return 0;
	}
	catch (const args::Error& err)
	{
		std::cerr << err.what() << std::endl << std::endl;
		if (std::string_view{ err.what() } == "Command is required")
			std::cerr << parser;
		return 1;
	}

	// Init logging
	auto logger = spdlog::stdout_color_mt("Console");
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("%^%l:%$ %v");
	if (verbose)
		spdlog::set_level(spdlog::level::trace);
	else
		spdlog::set_level(spdlog::level::info);

	SPDLOG_DEBUG("Logging Initialized");

	if (convert)
	{
		std::string inputMeshStr = inputMesh.Get();
		std::string outputMeshStr = outputMesh.Get();
		if (outputMeshStr.empty())
			outputMeshStr = inputMeshStr;

		fs::path inputMeshPath = inputMeshStr;
		inputMeshPath = fs::absolute(inputMeshPath);

		std::error_code ec;
		if (!fs::is_regular_file(inputMeshPath, ec))
		{
			SPDLOG_ERROR("Missing input file: {}", inputMeshStr);
			return 1;
		}

		fmt::print("Converting {}...\n", inputMeshStr);

		NavMesh navmesh;
		NavMesh::LoadResult result = navmesh.LoadNavMeshFile(inputMeshStr);

		switch (result)
		{
		case NavMesh::LoadResult::Corrupt:
			SPDLOG_ERROR("Load failed: navmesh is corrupt");
			return 1;

		case NavMesh::LoadResult::MissingFile:
			SPDLOG_ERROR("Load failed: navmesh is missing");
			return 1;

		case NavMesh::LoadResult::OutOfMemory:
			SPDLOG_ERROR("Load failed: out of memory");
			return 1;

		case NavMesh::LoadResult::VersionMismatch:
			SPDLOG_ERROR("Load failed: incompatible version");
			return 1;

		case NavMesh::LoadResult::ZoneMismatch:
			SPDLOG_ERROR("Load failed: wrong zone");
			return 1;

		case NavMesh::LoadResult::Success:
			break;
		}

		NavMeshHeaderVersion version = static_cast<NavMeshHeaderVersion>(meshVersion.Get());

		fmt::print("Saving to: {0}...", outputMeshStr);

		if (navmesh.SaveNavMeshFile(outputMeshStr, version))
		{
			fmt::print(" Success!\n");
		}
		else
		{
			fmt::print("Failed!\n");
		}
	}
	else if (validate)
	{
		return ValidateMesh(validateMesh.Get(), validateJson);
	}
	else if (exportCmd)
	{
		return ExportMesh(exportMesh.Get(), exportOut.Get(), exportJson);
	}
	else if (diff)
	{
		return DiffMeshes(diffOld.Get(), diffNew.Get(), diffTolerance.Get(), diffStrict, diffJson);
	}
	else
	{
		std::cout << parser;
	}

	return 0;
}
