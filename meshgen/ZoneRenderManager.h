#pragma once

#include <bgfx/bgfx.h>

class InputGeom;
class ZoneInputGeometryRender;
struct NavMeshConfig;

class ZoneRenderManager
{
public:
	ZoneRenderManager();
	~ZoneRenderManager();

	void init();

	void Render();
	void DestroyObjects();

	void SetInputGeometry(InputGeom* geom);

	void SetNavMeshConfig(const NavMeshConfig* config);
	const NavMeshConfig* GetNavMeshConfig() const { return m_meshConfig; }

	bgfx::TextureHandle GetGridTexture() const { return m_gridTexture; }

private:
	ZoneInputGeometryRender* m_zoneInputGeometry = nullptr;
	const NavMeshConfig* m_meshConfig = nullptr;

	bgfx::TextureHandle m_gridTexture = BGFX_INVALID_HANDLE;
};

class ZoneInputGeometryRender
{
public:
	explicit ZoneInputGeometryRender(ZoneRenderManager* manager);
	~ZoneInputGeometryRender();

	static void init();

	void SetInputGeometry(InputGeom* geom);

	void Render();
	void DestroyObjects();
	void CreateObjects();

private:
	bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texSampler = BGFX_INVALID_HANDLE;

	ZoneRenderManager* m_mgr;
	InputGeom* m_geom = nullptr;
};
