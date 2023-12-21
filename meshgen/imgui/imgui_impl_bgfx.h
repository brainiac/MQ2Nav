// Derived from this Gist by Richard Gale:
//     https://gist.github.com/RichardGale/6e2b74bc42b3005e08397236e4be0fd0

#include "imgui/imgui.h"

// ImGui BGFX binding

// You can copy and use unmodified imgui_impl_* files in your project. See
// main.cpp for an example of using this. If you use this binding you'll need to
// call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(),
// ImGui::Render() and ImGui_ImplXXXX_Shutdown(). If you are new to ImGui, see
// examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

void ImGui_ImplBgfx_Init();
void ImGui_ImplBgfx_Shutdown();
void ImGui_ImplBgfx_NewFrame();
void ImGui_ImplBgfx_RenderDrawLists(struct ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing ImGui state.
void ImGui_ImplBgfx_InvalidateDeviceObjects();
bool ImGui_ImplBgfx_CreateDeviceObjects();

#define BGFX_IMGUI_FLAGS_NONE                0x00
#define BGFX_IMGUI_FLAGS_ALPHA_BLEND         0x01

namespace ImGui
{

	inline ImTextureID toId(bgfx::TextureHandle handle, uint8_t flags, uint8_t mip)
	{
		union { struct { bgfx::TextureHandle handle; uint8_t flags; uint8_t mip; } s; ImTextureID id; } tex;
		tex.s.handle = handle;
		tex.s.flags = flags;
		tex.s.mip = mip;
		return tex.id;
	}

	// Helper function for passing bgfx::TextureHandle to ImGui::Image.
	inline void Image(bgfx::TextureHandle handle, uint8_t flags, uint8_t mip, const ImVec2& size, const ImVec2& uv0 = ImVec2(0.0f, 0.0f),
		const ImVec2& uv1 = ImVec2(1.0f, 1.0f), const ImVec4& tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), const ImVec4& borderCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
	{
		Image(toId(handle, flags, mip), size, uv0, uv1, tintCol, borderCol);
	}

	// Helper function for passing bgfx::TextureHandle to ImGui::ImageButton.
	inline bool ImageButton(const char* str_id, bgfx::TextureHandle handle, uint8_t flags, uint8_t mip, const ImVec2& size,
		const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f), const ImVec4& bgCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
		const ImVec4& tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
	{
		return ImageButton(str_id, toId(handle, flags, mip), size, uv0, uv1, bgCol, tintCol);
	}

	// Helper function for passing bgfx::TextureHandle to ImGui::ImageButton.
	inline bool ImageButton(const char* str_id, bgfx::TextureHandle handle, const ImVec2& size, const ImVec2& uv0 = ImVec2(0.0f, 0.0f),
		const ImVec2& uv1 = ImVec2(1.0f, 1.0f), const ImVec4& bgCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
		const ImVec4& tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
	{
		return ImageButton(str_id, handle, BGFX_IMGUI_FLAGS_ALPHA_BLEND, 0, size, uv0, uv1, bgCol, tintCol);
	}
}
