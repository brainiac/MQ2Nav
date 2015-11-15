//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef RECASTSAMPLETILEMESH_H
#define RECASTSAMPLETILEMESH_H

#include "Sample.h"
#include "DetourNavMesh.h"
#include "Recast.h"
#include "ChunkyTriMesh.h"

#include <atomic>
#include <memory>
#include <functional>

template <typename T>
using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

class Sample_TileMesh : public Sample
{
protected:
	bool m_buildAll;
	float m_totalBuildTimeMs;

	const int MAX_NODES = 1024 * 1024;

	enum DrawMode
	{
		DRAWMODE_NAVMESH,
		DRAWMODE_NAVMESH_TRANS,
		DRAWMODE_NAVMESH_BVTREE,
		DRAWMODE_NAVMESH_NODES,
		DRAWMODE_NAVMESH_PORTALS,
		DRAWMODE_NAVMESH_INVIS,
		DRAWMODE_MESH,
		DRAWMODE_VOXELS,
		DRAWMODE_VOXELS_WALKABLE,
		DRAWMODE_COMPACT,
		DRAWMODE_COMPACT_DISTANCE,
		DRAWMODE_COMPACT_REGIONS,
		DRAWMODE_REGION_CONNECTIONS,
		DRAWMODE_RAW_CONTOURS,
		DRAWMODE_BOTH_CONTOURS,
		DRAWMODE_CONTOURS,
		DRAWMODE_POLYMESH,
		DRAWMODE_POLYMESH_DETAIL,
		MAX_DRAWMODE
	};
		
	DrawMode m_drawMode;
	
	int m_maxTiles;
	int m_maxPolysPerTile;
	float m_tileSize;
	
	unsigned int m_tileCol;
	float m_tileBmin[3];
	float m_tileBmax[3];
	float m_tileBuildTime;
	float m_tileMemUsage;
	int m_tileTriCount;
	char* m_outputPath;

	int m_tilesWidth = 0;
	int m_tilesHeight = 0;
	int m_tilesCount = 0;
	std::atomic<int> m_tilesBuilt = 0;

	std::atomic<bool> m_buildingTiles = false;
	std::atomic<bool> m_cancelTiles = false;

	unsigned char* buildTileMesh(const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize);

	void saveAll(const char* path, const dtNavMesh* mesh);
	dtNavMesh* loadAll(const char* path);
	
public:
	Sample_TileMesh();
	virtual ~Sample_TileMesh();

	void SaveMesh(const std::string& outputPath);
	bool LoadMesh(const std::string& outputPath);

	void ResetMesh();
	
	virtual void handleSettings();
	virtual void handleTools();
	virtual void handleDebugMode();
	virtual void handleRender();
	virtual void handleRenderOverlay(double* proj, double* model, int* view);
	virtual void handleMeshChanged(class InputGeom* geom);
	virtual bool handleBuild();
	
	void getTilePos(const float* pos, int& tx, int& ty);
	
	void buildTile(const float* pos);
	void removeTile(const float* pos);
	void buildAllTiles();
	void removeAllTiles();
	void cancelBuildAllTiles();

	bool isBuildingTiles() const { return m_buildingTiles; }

	void getTileStatistics(int& width, int& height, int& maxTiles) const;
	int getTilesBuilt() const { return m_tilesBuilt; }

	void setOutputPath(const char* output_path);

	deleted_unique_ptr<rcCompactHeightfield> rasterizeGeometry(rcConfig& cfg);
};


#endif // RECASTSAMPLETILEMESH_H
