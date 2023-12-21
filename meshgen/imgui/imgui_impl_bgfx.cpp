// Derived from this Gist by Richard Gale:
//     https://gist.github.com/RichardGale/6e2b74bc42b3005e08397236e4be0fd0

// ImGui BFFX binding
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture
// identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See
// main.cpp for an example of using this. If you use this binding you'll need to
// call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(),
// ImGui::Render() and ImGui_ImplXXXX_Shutdown(). If you are new to ImGui, see
// examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "imgui_impl_bgfx.h"
#include "imgui.h"

#include "engine/embedded_shader.h"
#include "engine/bgfx_utils.h"

// BGFX/BX
#include "bgfx/bgfx.h"
#include "bx/math.h"
#include "bx/timer.h"

// Data
static uint8_t s_viewId = 255;
static bgfx::TextureHandle s_texture = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_program = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_imageProgram = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_imageLodEnabled = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout s_layout;

// This is the main rendering function that you have to implement and call after
// ImGui::Render(). Pass ImGui::GetDrawData() to this function.
// Note: If text or lines are blurry when integrating ImGui into your engine,
// in your Render function, try translating your projection matrix by
// (0.5f,0.5f) or (0.375f,0.375f)
void ImGui_ImplBgfx_RenderDrawLists(ImDrawData* draw_data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays
	// (screen coordinates != framebuffer coordinates)
	ImGuiIO& io = ImGui::GetIO();
	int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0) {
		return;
	}
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	bgfx::setViewName(s_viewId, "ImGui");
	bgfx::setViewMode(s_viewId, bgfx::ViewMode::Sequential);

	// Setup viewport, orthographic projection matrix
	const bgfx::Caps* caps = bgfx::getCaps();
	{
		float ortho[16];
		float x = draw_data->DisplayPos.x;
		float y = draw_data->DisplayPos.y;
		float width = draw_data->DisplaySize.x;
		float height = draw_data->DisplaySize.y;

		bx::mtxOrtho(ortho, x, x + width, height, y, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
		bgfx::setViewTransform(s_viewId, nullptr, ortho);
		bgfx::setViewRect(s_viewId, 0, 0, (uint16_t)fb_width, (uint16_t)fb_height);
	}

	const ImVec2 clipPos = draw_data->DisplayPos;       // (0,0) unless using multi-viewports
	const ImVec2 clipScale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	// Render command lists
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer tib;

		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		uint32_t numVertices = (uint32_t)cmd_list->VtxBuffer.size();
		uint32_t numIndices  = (uint32_t)cmd_list->IdxBuffer.size();

		if (!checkAvailTransientBuffers(numVertices, s_layout, numIndices))
		{
			// not enough space in transient buffer, quit drawing the rest...
			break;
		}

		bgfx::allocTransientVertexBuffer(&tvb, numVertices, s_layout);
		bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

		ImDrawVert* verts = (ImDrawVert*)tvb.data;
		memcpy(verts, cmd_list->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

		ImDrawIdx* indices = (ImDrawIdx*)tib.data;
		memcpy(indices, cmd_list->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

		bgfx::Encoder* encoder = bgfx::begin();

		for (const ImDrawCmd* cmd = cmd_list->CmdBuffer.begin(), *cmdEnd = cmd_list->CmdBuffer.end(); cmd != cmdEnd; ++cmd)
		{
			if (cmd->UserCallback)
			{
				cmd->UserCallback(cmd_list, cmd);
			}
			else if (cmd->ElemCount != 0)
			{
				// Setup render state: alpha-blending enabled, no face culling,
				// no depth testing, scissor enabled
				uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

				bgfx::TextureHandle th = s_texture;
				bgfx::ProgramHandle program = s_program;

				if (cmd->TextureId != nullptr)
				{
					union
					{
						ImTextureID ptr;
						struct
						{
							bgfx::TextureHandle handle;
							uint8_t flags;
							uint8_t mip;
						} s;
					} texture = { cmd->TextureId };

					state |= (BGFX_IMGUI_FLAGS_ALPHA_BLEND & texture.s.flags) != 0
						? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
						: BGFX_STATE_NONE;

					th = texture.s.handle;

					if (texture.s.mip != 0)
					{
						const float lodEnabled[4] = { float(texture.s.mip), 1.0f, 0.0f, 0.0f };
						bgfx::setUniform(s_imageLodEnabled, lodEnabled);
						program = s_imageProgram;
					}
				}
				else
				{
					state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
				}

				// Project scissor/clipping rectangles into framebuffer space
				ImVec4 clipRect;
				clipRect.x = (cmd->ClipRect.x - clipPos.x) * clipScale.x;
				clipRect.y = (cmd->ClipRect.y - clipPos.y) * clipScale.y;
				clipRect.z = (cmd->ClipRect.z - clipPos.x) * clipScale.x;
				clipRect.w = (cmd->ClipRect.w - clipPos.y) * clipScale.y;

				if (clipRect.x < fb_width && clipRect.y < fb_height
					&& clipRect.z >= 0.0f && clipRect.w >= 0.0f)
				{
					const uint16_t xx = uint16_t(bx::max(clipRect.x, 0.0f));
					const uint16_t yy = uint16_t(bx::max(clipRect.y, 0.0f));
					encoder->setScissor(xx, yy, uint16_t(bx::min(clipRect.z, 65535.0f) - xx) , uint16_t(bx::min(clipRect.w, 65535.0f) - yy));

					encoder->setState(state);
					encoder->setTexture(0, s_tex, th);
					encoder->setVertexBuffer(0, &tvb, cmd->VtxOffset, numVertices);
					encoder->setIndexBuffer(&tib, cmd->IdxOffset, cmd->ElemCount);
					encoder->submit(s_viewId, program);
				}
			}
		}

		bgfx::end(encoder);
	}
}

bool ImGui_ImplBgfx_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();

	uint8_t* data;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

	// Upload texture to graphics system
	s_texture = bgfx::createTexture2D(
		(uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, 0,
		bgfx::copy(data, width * height * 4));

	// Store our identifier
	io.Fonts->TexID = ImGui::toId(s_texture, BGFX_IMGUI_FLAGS_ALPHA_BLEND, 0);

	return true;
}

#include "fs_imgui.bin.h"
#include "vs_imgui.bin.h"
#include "fs_imgui_image.bin.h"
#include "vs_imgui_image.bin.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(vs_imgui),
	BGFX_EMBEDDED_SHADER(fs_imgui),
	BGFX_EMBEDDED_SHADER(vs_imgui_image),
	BGFX_EMBEDDED_SHADER(fs_imgui_image),
	
	BGFX_EMBEDDED_SHADER_END()
};

bool ImGui_ImplBgfx_CreateDeviceObjects()
{
	bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;

	s_program = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui"),
		true);

	s_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
	s_imageProgram = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"),
		true);

	s_layout
		.begin()
			.add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
		.end();

	s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

	ImGui_ImplBgfx_CreateFontsTexture();

	return true;
}

void ImGui_ImplBgfx_InvalidateDeviceObjects()
{
	bgfx::destroy(s_tex);
	if (isValid(s_texture))
	{
		bgfx::destroy(s_texture);
		ImGui::GetIO().Fonts->TexID = 0;
		s_texture.idx = bgfx::kInvalidHandle;
	}

	bgfx::destroy(s_imageLodEnabled);
	bgfx::destroy(s_imageProgram);
	bgfx::destroy(s_program);
}

void ImGui_ImplBgfx_Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
}

void ImGui_ImplBgfx_Shutdown()
{
	ImGui_ImplBgfx_InvalidateDeviceObjects();
}

void ImGui_ImplBgfx_NewFrame()
{
	if (!isValid(s_texture))
	{
		ImGui_ImplBgfx_CreateDeviceObjects();
	}
}
