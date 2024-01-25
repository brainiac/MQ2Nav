
#include "pch.h"
#include "RenderManager.h"

#include "ZoneRenderManager.h"

#include "mq/base/Color.h"
#include "imgui/imgui_impl.h"

#include <bx/pixelformat.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.inl>
#include <im3d/im3d.h>
#include <im3d/im3d_math.h>
#include <SDL2/SDL_syswm.h>

//============================================================================

RenderManager* g_render = nullptr;

static HWND SDLNativeWindowHandle(SDL_Window* window)
{
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (!SDL_GetWindowWMInfo(window, &wmi))
	{
		return nullptr;
	}

	return wmi.info.win.window;
}

//============================================================================

RenderManager::RenderManager()
	: m_bgfxDebug(BGFX_DEBUG_TEXT)
	, m_bgfxResetFlags(BGFX_RESET_MSAA_X4)
{
	m_camera = std::make_unique<Camera>();


}

RenderManager::~RenderManager()
{

}

bool RenderManager::Initialize(int width, int height, SDL_Window* window)
{
	m_windowWidth = width;
	m_windowHeight = height;
	m_window = window;

	bgfx::Init init;
	init.type = bgfx::RendererType::Direct3D11;
	init.vendorId = BGFX_PCI_ID_NONE;
	init.platformData.nwh = SDLNativeWindowHandle(window);
	init.platformData.ndt = nullptr;
	init.platformData.type = bgfx::NativeWindowHandleType::Default;
	init.resolution.width = m_windowWidth;
	init.resolution.height = m_windowHeight;
	init.resolution.reset = m_bgfxResetFlags;
	if (!bgfx::init(init))
	{
		char szMessage[256];
		sprintf_s(szMessage, "Error: bgfx::init() failed");

		::MessageBoxA(nullptr, szMessage, "Error Starting Engine", MB_OK | MB_ICONERROR);
		return false;
	}

	m_bgfxCaps = bgfx::getCaps();

	bgfx::setViewMode(0, bgfx::ViewMode::Sequential);
	bgfx::setDebug(m_bgfxDebug);

	ImVec4 clear_color = ImVec4(0.3f, 0.3f, 0.32f, 1.00f);
	bgfx::setViewClear(m_viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, mq::MQColor(clear_color).ToRGBA(), 1.0f, 0);

	SetViewport({ 0, 0 }, { m_windowWidth, m_windowHeight });

	ZoneRenderManager::init();

	bgfx::frame();

	return true;
}

void RenderManager::Shutdown()
{
	ZoneRenderManager::shutdown();

	// shutdown ImGui
	ImGui_Impl_Shutdown();

	// Shutdown bgfx.
	bgfx::shutdown();
}

void RenderManager::BeginFrame(float timeDelta)
{
	ImGui_Impl_NewFrame();
	Im3D_NewFrame(timeDelta);

	bgfx::setViewTransform(0, glm::value_ptr(m_camera->GetViewMtx()), glm::value_ptr(m_camera->GetProjMtx()));
	m_cursorRayStart = glm::unProject(glm::vec3(m_mousePos.x, m_mousePos.y, 0.0f), m_camera->GetViewMtx(), m_camera->GetProjMtx(), m_viewport);
	m_cursorRayEnd   = glm::unProject(glm::vec3(m_mousePos.x, m_mousePos.y, 1.0f), m_camera->GetViewMtx(), m_camera->GetProjMtx(), m_viewport);

	m_viewModelProjMtx = m_camera->GetViewProjMtx();

	bgfx::touch(m_viewId);
}

void RenderManager::EndFrame()
{
	Im3d::Draw();
	Im3D_DrawText();

	ImGui_Impl_Render();

	// Advance to next frame. Rendering thread will be kicked to process submitted rendering primitives.
	bgfx::frame();
}

void RenderManager::Im3D_DrawText()
{
	uint32_t drawListCount = Im3d::GetTextDrawListCount();
	ImDrawList* imDrawList = ImGui::GetBackgroundDrawList();

	for (uint32_t i = 0; i < drawListCount; ++i)
	{
		const Im3d::TextDrawList& textDrawList = Im3d::GetTextDrawLists()[i];

		for (uint32_t j = 0; j < textDrawList.m_textDataCount; ++j)
		{
			const Im3d::TextData& textData = textDrawList.m_textData[j];
			if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
			{
				continue;
			}

			// Project world -> screen space.
			glm::vec4 clip = m_viewModelProjMtx * glm::vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f);
			glm::vec2 screen = glm::vec2(clip.x / clip.w, clip.y / clip.w);

			// Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
			if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f)
			{
				continue;
			}

			// Pixel coordinates for the ImGuiWindow ImGui.
			screen = screen * glm::vec2(0.5f) + glm::vec2(0.5f);
			screen.y = 1.0f - screen.y; // screen space origin is reversed by the projection.
			screen = screen * glm::vec2(m_viewport.zw()) + glm::vec2(m_viewport.xy());

			// All text data is stored in a single buffer; each textData instance has an offset into this buffer.
			const char* text = textDrawList.m_textBuffer + textData.m_textBufferOffset;

			// Calculate the final text size in pixels to apply alignment flags correctly.
			ImGuiContext* g = ImGui::GetCurrentContext();
			ImFont* font = ImGui::GetFont();
			float fontSize = font->FontSize * textData.m_positionSize.w;
			ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, text, text + textData.m_textLength, nullptr);

			textSize.x = static_cast<float>(static_cast<int>(textSize.x + 0.99999f));

			// Generate a pixel offset based on text flags.
			glm::vec2 textOffset = glm::vec2(-textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
			if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0)
			{
				textOffset.x = -textSize.x;
			}
			else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0)
			{
				textOffset.x = 0.0f;
			}

			if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0)
			{
				textOffset.y = -textSize.y;
			}
			else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0)
			{
				textOffset.y = 0.0f;
			}

			// Add text to the window draw list.
			screen = screen + textOffset;

			imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(),
				ImVec2(screen.x, screen.y), textData.m_color.getABGR(), text, text + textData.m_textLength);
		}
	}
}

void RenderManager::Im3D_NewFrame(float timeDelta)
{
	Im3d::AppData& ad = Im3d::GetAppData();

	ad.m_deltaTime = timeDelta;
	ad.m_viewportSize = Im3d::Vec2(static_cast<float>(m_viewport.z), static_cast<float>(m_viewport.w));
	ad.m_viewOrigin = m_camera->GetPosition();
	ad.m_viewDirection = m_camera->GetDirection();
	ad.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
	ad.m_projOrtho = false;
	ad.m_projScaleY = glm::tan(glm::radians(m_camera->GetFieldOfView()) * 0.5f) * 2.0f;

	ad.m_cursorRayOrigin = m_cursorRayStart;
	ad.m_cursorRayDirection = glm::normalize(m_cursorRayEnd - m_cursorRayStart);

	bool ctrlDown = (SDL_GetModState() & KMOD_CTRL) != 0;

	const Uint8* keyState = SDL_GetKeyboardState(nullptr);
	bool keyOk = !ImGui::GetIO().WantCaptureKeyboard;

	ad.m_keyDown[Im3d::Mouse_Left/*Im3d::Action_Select*/] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/] = keyOk && keyState[SDL_SCANCODE_L] != 0;
	ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = keyOk && keyState[SDL_SCANCODE_T] != 0;
	ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/] = keyOk && keyState[SDL_SCANCODE_R] != 0;
	ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/] = keyOk && keyState[SDL_SCANCODE_S] != 0;

	ad.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
	ad.m_snapRotation = ctrlDown ? Im3d::Radians(30.0f) : 0.0f;
	ad.m_snapScale = ctrlDown ? 0.5f : 0.0f;

	Im3d::NewFrame();
}

void RenderManager::SetViewport(const glm::ivec2& origin, const glm::ivec2& size)
{
	m_viewport = { origin.x, origin.y, size.x, size.y };

	float aspectRatio = static_cast<float>(m_viewport.z) / static_cast<float>(m_viewport.w);

	m_camera->SetAspectRatio(aspectRatio);

	bgfx::setViewRect(m_viewId, static_cast<uint16_t>(m_viewport.x), static_cast<uint16_t>(m_viewport.y),
		static_cast<uint16_t>(m_viewport.z), static_cast<uint16_t>(m_viewport.w));
}

void RenderManager::SetWindowSize(int width, int height)
{
	m_windowWidth = width;
	m_windowHeight = height;

	bgfx::reset(m_windowWidth, m_windowHeight, m_bgfxResetFlags);
}

void RenderManager::SetMousePosition(const glm::ivec2& mousePos)
{
	int mouseX = mousePos.x;
	int mouseY = mousePos.y - m_viewport.y;

	mouseY = (m_viewport.w - 1 - mouseY) + m_viewport.y;

	if (mouseY < 0)
		mouseY = 0;

	m_mousePos = { mouseX, mouseY };
}
