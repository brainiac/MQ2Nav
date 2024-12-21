
#pragma once

#include <bgfx/bgfx.h>

class BgfxCallback;
class Camera;

class RenderManager
{
public:
	RenderManager();
	~RenderManager();

	bool Initialize(int width, int height, SDL_Window* window);
	void Shutdown();

	void BeginFrame(float timeDelta, const Camera& camera);
	void EndFrame();

	void BeginImGui(float timeDelta, const Camera& camera);

	const glm::ivec4& GetViewport() const { return m_viewport; }

	void SetViewport(const glm::ivec2& origin, const glm::ivec2& size);
	void SetWindowSize(int width, int height);

private:
	void Im3D_NewFrame(float timeDelta, const Camera& camera);
	void Im3D_DrawText();

	std::unique_ptr<BgfxCallback> m_callback;
	int                     m_windowWidth = 0;
	int                     m_windowHeight = 0;
	uint32_t                m_bgfxDebug;
	uint32_t                m_bgfxResetFlags;
	const bgfx::Caps*       m_bgfxCaps = nullptr;
	bgfx::ViewId            m_viewId = 0;
	glm::mat4               m_projMtx;
	glm::mat4               m_viewModelProjMtx;
	glm::ivec4              m_viewport;
	glm::ivec2              m_windowSize;
};

extern RenderManager* g_render;
