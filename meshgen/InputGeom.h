//
// InputGeom.h
//

#pragma once

#include "ChunkyTriMesh.h"
#include "MapGeometryLoader.h"

#include "common/NavMeshData.h"

static const int MAX_CONVEXVOL_PTS = 12;

struct ConvexVolume
{
	std::vector<glm::vec3> verts;
	float hmin, hmax;
	PolyArea areaType;
};

class InputGeom
{
public:
	InputGeom(const std::string& zoneShortName, const std::string& eqPath, const std::string& meshPath);
	~InputGeom();

	bool loadGeometry(class rcContext* ctx);

	// Method to return static mesh data.
	inline const glm::vec3& getMeshBoundsMin() const { return m_meshBMinCustom; }
	inline const glm::vec3& getMeshBoundsMax() const { return m_meshBMaxCustom; }

	inline const glm::vec3& getRealMeshBoundsMin() const { return m_meshBMin; }
	inline const glm::vec3& getRealMeshBoundsMax() const { return m_meshBMax; }


	void resetMeshBounds();

	inline void setMeshBoundsMin(const glm::vec3& min) { m_meshBMinCustom = min; }
	inline void setMeshBoundsMax(const glm::vec3& max) { m_meshBMaxCustom = max; }

	inline const MapGeometryLoader* getMeshLoader() const { return m_loader.get(); }
	inline const rcChunkyTriMesh* getChunkyMesh() const { return m_chunkyMesh.get(); }

	// Off-Mesh connections.
	int getOffMeshConnectionCount() const { return m_offMeshConCount; }
	const float* getOffMeshConnectionVerts() const { return m_offMeshConVerts; }
	const float* getOffMeshConnectionRads() const { return m_offMeshConRads; }
	const unsigned char* getOffMeshConnectionDirs() const { return m_offMeshConDirs; }
	const unsigned char* getOffMeshConnectionAreas() const { return m_offMeshConAreas; }
	const unsigned short* getOffMeshConnectionFlags() const { return m_offMeshConFlags; }
	const unsigned int* getOffMeshConnectionId() const { return m_offMeshConId; }
	void addOffMeshConnection(const float* spos, const float* epos, const float rad,
		unsigned char bidir, unsigned char area, unsigned short flags);
	void deleteOffMeshConnection(int i);
	void drawOffMeshConnections(struct duDebugDraw* dd, bool hilight = false);

	// box volumes
	size_t getConvexVolumeCount() const { return m_volumes.size(); }
	const ConvexVolume* getConvexVolume(size_t i) const { return m_volumes[i].get(); }
	void addConvexVolume(const std::vector<glm::vec3>& verts,
		const float minh, const float maxh, PolyArea areaType);
	void deleteConvexVolume(size_t i);
	void drawConvexVolumes(struct duDebugDraw* dd, bool hilight = false);

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
	glm::vec3 m_meshBMinCustom, m_meshBMaxCustom;

	// Off-Mesh connections.
	static const int MAX_OFFMESH_CONNECTIONS = 256;
	float m_offMeshConVerts[MAX_OFFMESH_CONNECTIONS * 3 * 2];
	float m_offMeshConRads[MAX_OFFMESH_CONNECTIONS];
	unsigned char m_offMeshConDirs[MAX_OFFMESH_CONNECTIONS];
	unsigned char m_offMeshConAreas[MAX_OFFMESH_CONNECTIONS];
	unsigned short m_offMeshConFlags[MAX_OFFMESH_CONNECTIONS];
	unsigned int m_offMeshConId[MAX_OFFMESH_CONNECTIONS];
	int m_offMeshConCount = 0;

	// Convex Volumes.
	std::vector<std::unique_ptr<ConvexVolume>> m_volumes;
};
