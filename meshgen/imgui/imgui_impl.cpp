
#include "imgui_impl_bgfx.h"
#include "imgui_impl_sdl2.h"

void ImGui_Impl_Init(SDL_Window* window, ImGuiContext* context)
{
	ImGui_ImplBgfx_Init();
	ImGui_ImplSDL2_InitForD3D(window);
}

void ImGui_Impl_Shutdown()
{
	ImGui_ImplSDL2_Shutdown();
	ImGui_ImplBgfx_Shutdown();

	ImGui::DestroyContext();
}


void ImGui_Impl_NewFrame()
{
	ImGui_ImplBgfx_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void ImGui_Impl_Render()
{
	ImGui::Render();

	ImGui_ImplBgfx_RenderDrawLists(ImGui::GetDrawData());
}
