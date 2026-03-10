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

#include "meshgen/ResourceManager.h"

// BGFX/BX
#include "bgfx/bgfx.h"
#include "bx/math.h"
#include "bx/timer.h"

// Data
static uint8_t s_viewId = 255;
static bgfx::ProgramHandle s_program = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_imageProgram = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_imageLodEnabled = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout s_layout;

#define IMGUI_FLAGS_NONE        UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)

inline bool checkAvailTransientBuffers(uint32_t numVertices, const bgfx::VertexLayout& layout, uint32_t numIndices)
{
	return numVertices == bgfx::getAvailTransientVertexBuffer(numVertices, layout)
		&& (0 == numIndices || numIndices == bgfx::getAvailTransientIndexBuffer(numIndices));
}

static void ImGui_ImplBgfx_DestroyTexture(ImTextureData* texData)
{
	ImGui::TextureBgfx tex = bx::bitCast<ImGui::TextureBgfx>(texData->GetTexID());
	bgfx::destroy(tex.handle);
	texData->SetTexID(ImTextureID_Invalid);
	texData->SetStatus(ImTextureStatus_Destroyed);
}

static void ImGui_ImplBgfx_UpdateTexture(ImTextureData* texData)
{
	if (texData->Status == ImTextureStatus_WantCreate)
	{
		ImGui::TextureBgfx tex =
		{
			.handle = bgfx::createTexture2D(
				  (uint16_t)texData->Width
				, (uint16_t)texData->Height
				, false
				, 1
				, bgfx::TextureFormat::BGRA8
				, 0
				),
			.flags = IMGUI_FLAGS_ALPHA_BLEND,
			.mip = 0,
			.unused = 0,
		};

		bgfx::setName(tex.handle, "ImGui Font Atlas");
		bgfx::updateTexture2D(tex.handle, 0, 0, 0, 0
			, bx::narrowCast<uint16_t>(texData->Width)
			, bx::narrowCast<uint16_t>(texData->Height)
			, bgfx::copy(texData->GetPixels(), texData->GetSizeInBytes())
		);

		texData->SetTexID(bx::bitCast<ImTextureID>(tex));
		texData->SetStatus(ImTextureStatus_OK);
	}
	else if (texData->Status == ImTextureStatus_WantUpdates)
	{
		ImGui::TextureBgfx tex = bx::bitCast<ImGui::TextureBgfx>(texData->GetTexID());

		for (ImTextureRect& rect : texData->Updates)
		{
			const uint32_t bpp = texData->BytesPerPixel;
			const bgfx::Memory* pix = bgfx::alloc(rect.h * rect.w * bpp);
			bx::gather(pix->data, texData->GetPixelsAt(rect.x, rect.y), texData->GetPitch(), rect.w * bpp, rect.h);
			bgfx::updateTexture2D(tex.handle, 0, 0, rect.x, rect.y, rect.w, rect.h, pix);
		}
	}

	if (texData->Status == ImTextureStatus_WantDestroy && texData->UnusedFrames > 0)
	{
		ImGui_ImplBgfx_DestroyTexture(texData);
	}
}

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

	if (draw_data->Textures != nullptr)
	{
		for (ImTextureData* tex : *draw_data->Textures)
		{
			if (tex->Status != ImTextureStatus_OK)
			{
				ImGui_ImplBgfx_UpdateTexture(tex);
			}
		}
	}

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

				bgfx::TextureHandle th = BGFX_INVALID_HANDLE;
				bgfx::ProgramHandle program = s_program;

				const ImTextureID texId = cmd->GetTexID();

				if (texId != ImTextureID_Invalid)
				{
					ImGui::TextureBgfx tex = bx::bitCast<ImGui::TextureBgfx>(texId);

					state |= (BGFX_IMGUI_FLAGS_ALPHA_BLEND & tex.flags) != 0
						? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
						: BGFX_STATE_NONE;

					th = tex.handle;

					if (tex.mip != 0)
					{
						const float lodEnabled[4] = { float(tex.mip), 1.0f, 0.0f, 0.0f };
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

void ImGui_ImplBgfx_InvalidateDeviceObjects()
{
	bgfx::destroy(s_tex);

	for (ImTextureData* texData : ImGui::GetPlatformIO().Textures)
	{
		if (texData->RefCount == 1)
		{
			ImGui::TextureBgfx tex = bx::bitCast<ImGui::TextureBgfx>(texData->GetTexID());
			bgfx::destroy(tex.handle);
			texData->SetTexID(ImTextureID_Invalid);
			texData->SetStatus(ImTextureStatus_Destroyed);
		}
	}

	bgfx::destroy(s_imageLodEnabled);
}

void ImGui_ImplBgfx_Init()
{
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

	s_program = g_resourceMgr->GetProgramHandle("imgui");
	s_imageProgram = g_resourceMgr->GetProgramHandle("imgui_image");

	s_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);

	s_layout
		.begin()
			.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
}

void ImGui_ImplBgfx_Shutdown()
{
	ImGui_ImplBgfx_InvalidateDeviceObjects();
}

void ImGui_ImplBgfx_NewFrame()
{
}
