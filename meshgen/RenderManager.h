
#pragma once

#include "Camera.h"

#include <bgfx/bgfx.h>

class BgfxCallback;

class RenderManager
{
public:
	RenderManager();
	~RenderManager();

	bool Initialize(int width, int height, SDL_Window* window);
	void Shutdown();

	void BeginFrame(float timeDelta);
	void EndFrame();

	Camera* GetCamera() const { return m_camera.get(); }
	const glm::ivec4& GetViewport() const { return m_viewport; }

	const glm::vec3& GetCursorRayStart() const { return m_cursorRayStart; }
	const glm::vec3& GetCursorRayEnd() const { return m_cursorRayEnd; }

	void SetViewport(const glm::ivec2& origin, const glm::ivec2& size);
	void SetWindowSize(int width, int height);

	void SetMousePosition(const glm::ivec2& mousePos);
	const glm::ivec2& GetMousePos() const { return m_mousePos; }

private:
	void Im3D_NewFrame(float timeDelta);
	void Im3D_DrawText();

	std::unique_ptr<BgfxCallback> m_callback;
	int                     m_windowWidth = 0;
	int                     m_windowHeight = 0;
	SDL_Window*             m_window = nullptr;
	std::unique_ptr<Camera> m_camera = nullptr;
	uint32_t                m_bgfxDebug;
	uint32_t                m_bgfxResetFlags;
	const bgfx::Caps*       m_bgfxCaps = nullptr;
	bgfx::ViewId            m_viewId = 0;
	glm::mat4               m_projMtx;
	glm::mat4               m_viewModelProjMtx;
	glm::ivec4              m_viewport;
	glm::vec3               m_cursorRayStart;
	glm::vec3               m_cursorRayEnd;
	glm::ivec2              m_mousePos;
	glm::ivec2              m_windowSize;
};

template <typename T, glm::qualifier Q>
glm::vec<3, T, Q> to_eq_coord(const glm::vec<3, T, Q>& world)
{
	return world.zxy();
}

template <typename T, glm::qualifier Q>
glm::vec<3, T, Q> from_eq_coord(const glm::vec<3, T, Q>& eq)
{
	return eq.yzx();
}

extern RenderManager* g_render;
