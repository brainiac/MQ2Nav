//
// InputGeom.h
//

#pragma once

#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/MapGeometryLoader.h"

#include "common/NavMeshData.h"

static const int MAX_CONVEXVOL_PTS = 12;

class InputGeom
{
public:
	InputGeom(const std::string& zoneShortName, const std::string& eqPath, const std::string& meshPath);
	~InputGeom();

	bool loadGeometry(std::unique_ptr<MapGeometryLoader> loader, class rcContext* ctx);

	// Method to return static mesh data.
	inline const glm::vec3& getMeshBoundsMin() const { return m_meshBMin; }
	inline const glm::vec3& getMeshBoundsMax() const { return m_meshBMax; }

	inline const MapGeometryLoader* getMeshLoader() const { return m_loader.get(); }
	inline const rcChunkyTriMesh* getChunkyMesh() const { return m_chunkyMesh.get(); }

	// Utilities
	bool raycastMesh(float* src, float* dst, float& tmin);

private:
	std::string m_eqPath;
	std::string m_zoneShortName;
	std::string m_meshPath;

	std::unique_ptr<rcChunkyTriMesh> m_chunkyMesh;
	std::unique_ptr<MapGeometryLoader> m_loader;

	// bounds
	glm::vec3 m_meshBMin, m_meshBMax;

	// Convex Volumes.
	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;
};
