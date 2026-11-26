//
// MeshCommands.h
//

#pragma once

#include <string>

// Each returns the process exit code: 0 success, 1 error, and for DiffMeshes 2 when connections,
// volumes or area types were lost (or, with strict, when settings or tile geometry changed).
int ValidateMesh(const std::string& meshPath, bool json);
int ExportMesh(const std::string& meshPath, const std::string& outPath, bool json);
int DiffMeshes(const std::string& oldPath, const std::string& newPath, float tolerance, bool strict, bool json);
