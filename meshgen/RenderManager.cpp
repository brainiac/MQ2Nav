
#include "pch.h"
#include "RenderManager.h"

#include "meshgen/Camera.h"
#include "meshgen/Logging.h"
#include "meshgen/ZoneRenderManager.h"

#include "mq/base/Color.h"
#include "imgui/imgui_impl.h"

#include <bx/file.h>
#include <bx/pixelformat.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.inl>
#include <im3d/im3d.h>
#include <im3d/im3d_math.h>
#include <spdlog/spdlog.h>
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

class BgfxCallback : public bgfx::CallbackI
{
public:
	BgfxCallback()
	{
		m_logger = Logging::GetLogger(LoggingCategory::Bgfx);
		m_logger->set_level(spdlog::level::debug);
	}

	~BgfxCallback() override
	{
	}

	void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override
	{
		m_logger->log(spdlog::source_loc(_filePath, _line, ""), spdlog::level::critical, "BGFX FATAL 0x{:08x}: {}", static_cast<int>(_code), _str);

		if (bgfx::Fatal::DebugCheck == _code)
		{
			bx::debugBreak();
		}
		else
		{
			abort();
		}
	}

	void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
	{
		char temp[2048];
		char* out = temp;
		va_list argListCopy;
		va_copy(argListCopy, _argList);
		int32_t total = vsnprintf(out, sizeof(temp), _format, argListCopy);
		va_end(argListCopy);
		if ((int32_t)sizeof(temp) < total)
		{
			out = (char*)alloca(total + 1);
			vsnprintf(out, total, _format, _argList);
		}
		if (out[total - 1] == '\n')
			out[total - 1] = '\0';
		else
			out[total] = '\0';

		if (strncmp(out, "BGFX ", 5) == 0)
			out += 5;

		m_logger->log(spdlog::source_loc(_filePath, _line, ""), spdlog::level::debug, "{}", out);
	}

	void profilerBegin(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override
	{
	}

	void profilerBeginLiteral(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override
	{
	}

	void profilerEnd() override
	{
	}

	uint32_t cacheReadSize(uint64_t _id) override { return 0; }

	bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override { return false; }

	void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override {}

	void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data,
		uint32_t _size, bool _yflip) override
	{
		const int32_t len = bx::strLen(_filePath) + 5;
		char* filePath = (char*)alloca(len);
		bx::strCopy(filePath, len, _filePath);
		bx::strCat(filePath, len, ".tga");

		bx::FileWriter writer;
		if (bx::open(&writer, filePath))
		{
			bimg::imageWriteTga(&writer, _width, _height, _pitch, _data, false, _yflip);
			bx::close(&writer);
		}
	}

	void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format,
		bool _yflip) override
	{
	}

	void captureEnd() override
	{
	}

	void captureFrame(const void* _data, uint32_t _size) override
	{
	}

private:
	std::shared_ptr<spdlog::logger> m_logger;
};

//============================================================================

RenderManager::RenderManager()
	: m_bgfxDebug(BGFX_DEBUG_TEXT)
	, m_bgfxResetFlags(BGFX_RESET_MSAA_X4)
{
	m_callback = std::make_unique<BgfxCallback>();
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
	init.callback = m_callback.get();
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

void RenderManager::BeginFrame(float timeDelta, const Camera& camera)
{
	auto& viewMtx = camera.GetViewMtx();
	auto& projMtx = camera.GetProjMtx();

	bgfx::setViewTransform(0, glm::value_ptr(viewMtx), glm::value_ptr(projMtx));

	m_cursorRayStart = glm::unProject(glm::vec3(m_mousePos.x, m_mousePos.y, 0.0f), viewMtx, projMtx, m_viewport);
	m_cursorRayEnd   = glm::unProject(glm::vec3(m_mousePos.x, m_mousePos.y, 1.0f), viewMtx, projMtx, m_viewport);

	m_viewModelProjMtx =camera.GetViewProjMtx();

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

void RenderManager::BeginImGui(float timeDelta, const Camera& camera)
{
	ImGui_Impl_NewFrame();
	Im3D_NewFrame(timeDelta, camera);
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

void RenderManager::Im3D_NewFrame(float timeDelta, const Camera& camera)
{
	Im3d::AppData& ad = Im3d::GetAppData();

	ad.m_deltaTime = timeDelta;
	ad.m_viewportSize = Im3d::Vec2(static_cast<float>(m_viewport.z), static_cast<float>(m_viewport.w));
	ad.m_viewOrigin = camera.GetPosition();
	ad.m_viewDirection = camera.GetDirection();
	ad.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
	ad.m_projOrtho = false;
	ad.m_projScaleY = glm::tan(glm::radians(camera.GetFieldOfView()) * 0.5f) * 2.0f;

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
