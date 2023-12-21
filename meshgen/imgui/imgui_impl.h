#pragma once

#include "SDL2/SDL_video.h"

void ImGui_Impl_Init(SDL_Window* window, ImGuiContext* context);
void ImGui_Impl_Shutdown();

void ImGui_Impl_NewFrame();
void ImGui_Impl_Render();
