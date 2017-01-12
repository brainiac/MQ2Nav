#include "imguipanelmanager.h"
//#include <stdio.h> // fprintf


bool gImGuiDockpanelManagerExtendedStyle = true;
float gImGuiDockPanelManagerActiveResizeSize = 6.f;
bool gImGuiDockPanelManagerAddExtraTitleBarResizing = false;
bool gImGuiDockPanelManagerAlwaysDrawExternalBorders = true;

// Static Helper Methods:
namespace ImGui {

static ImVec4 ColorDockButton(0.75f, 0.75f, 0.30f, 0.50f);
static ImVec4 ColorDockButtonHovered(0.85f, 0.85f, 0.5f, 0.60f);
static ImVec4 ColorDockButtonActive(0.70f, 0.70f, 0.60f, 1.00f);

static ImVec4 ColorDockButtonLines(0.80f, 0.90f, 0.70f, 1.00f);
static ImVec4 ColorDockButtonLinesHovered(0.85f, 1.f, 0.65f, 1.00f);

static ImVec4 ColorDockButtonIconBg(0.75f, 0.75f, 0.75f, 0.70f);
static ImVec4 ColorDockButtonIconBgHovered(0.85f, 0.85f, 0.85f, 0.85f);
static ImVec4 ColorDockButtonIconBgActive(0.70f, 0.70f, 0.70f, 1.00f);

static ImVec4 ColorDockButtonIcon(0.80f, 0.90f, 0.70f, 1.00f);
static ImVec4 ColorDockButtonIconHovered(0.85f, 1.f, 0.65f, 1.00f);


#ifndef NO_IMGUITABWINDOW
#include "../imguitabwindow/imguitabwindow.h"
#endif //NO_IMGUITABWINDOW


// Upper-right button to close a window.
static bool DockWindowButton(bool* p_undocked,bool *p_open=NULL)
{
    ImGuiWindow* window = GetCurrentWindow();

    const ImGuiID id = window->GetID("#DOCK");
    const float size = window->TitleBarHeight() - 4.0f;
    const ImRect bb(window->Rect().GetTR() + ImVec2(-3.0f-size,2.0f) + (p_open ? ImVec2(-3.0f-size,0) : ImVec2(0,0)),
                       window->Rect().GetTR() + ImVec2(-3.0f,2.0f+size) + (p_open ? ImVec2(-3.0f-size,0) : ImVec2(0,0))
                       );

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);//, true);

    if (p_undocked) {
#       ifndef NO_IMGUITABWINDOW
        if (gImGuiDockpanelManagerExtendedStyle && ImGui::TabWindow::DockPanelIconTextureID)    {
            // Render
            ImU32 col = GetColorU32((held && hovered) ? ColorDockButtonIconBgActive : hovered ? ColorDockButtonIconBgHovered : ColorDockButtonIconBg);
            window->DrawList->AddRectFilled(bb.Min, bb.Max, col,2.f);

            col = !hovered ? GetColorU32(ColorDockButtonIcon) : GetColorU32(ColorDockButtonIconHovered);
            if (*p_undocked)    {
               window->DrawList->AddImage(ImGui::TabWindow::DockPanelIconTextureID,bb.Min,bb.Max,ImVec2(0.0f,0.75f),ImVec2(0.25f,1.f), col);
            }
            else    {
               window->DrawList->AddImage(ImGui::TabWindow::DockPanelIconTextureID,bb.Min,bb.Max,ImVec2(0.25f,0.75f),ImVec2(0.5f,1.f), col);
            }
        }
        else
#       endif //NO_IMGUITABWINDOW
        {
            // Render
            ImU32 col = GetColorU32((held && hovered) ? ColorDockButtonActive : hovered ? ColorDockButtonHovered : ColorDockButton);
            window->DrawList->AddRectFilled(bb.Min, bb.Max, col, 0);

            col = !hovered ? GetColorU32(ColorDockButtonLines) : GetColorU32(ColorDockButtonLinesHovered);
            ImRect bb2 = bb;
            const ImVec2  sz = bb.GetSize();
            if (*p_undocked)    {
                bb2.Expand(ImVec2(-.2f*sz.x,-.7*sz.y));
                window->DrawList->AddRect(bb2.Min,bb2.Max, col, 0);
            }
            else    {
                bb2.Expand(ImVec2(-.7f*sz.x,-.2*sz.y));
                window->DrawList->AddRect(bb2.Min,bb2.Max, col, 0);
            }
        }
    }
    else {
        // Render
        ImU32 col = GetColorU32((held && hovered) ? ColorDockButtonActive : hovered ? ColorDockButtonHovered : ColorDockButton);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col, 0);
    }

    if (p_undocked != NULL && pressed)
        *p_undocked = !*p_undocked;

    return pressed;
}
static bool DockWindowBegin(const char* name, bool* p_opened,bool* p_undocked, const ImVec2& size, float bg_alpha, ImGuiWindowFlags flags,bool* pDraggingStarted=NULL,ImGui::PanelManager::WindowData* wd=NULL)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    IM_ASSERT(name != NULL);                        // Window name required
    IM_ASSERT(g.Initialized);                       // Forgot to call ImGui::NewFrame()
    IM_ASSERT(g.FrameCountEnded != g.FrameCount);   // Called ImGui::Render() or ImGui::EndFrame() and haven't called ImGui::NewFrame() again yet

    if (flags & ImGuiWindowFlags_NoInputs)
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

    // Find or create
    bool window_is_new = false;
    ImGuiWindow* window = FindWindowByName(name);
    if (!window)
    {
        window = CreateNewWindow(name, size, flags);
        window_is_new = true;
    }

    const int current_frame = ImGui::GetFrameCount();
    const bool first_begin_of_the_frame = (window->LastFrameActive != current_frame);
    if (first_begin_of_the_frame)
        window->Flags = (ImGuiWindowFlags)flags;
    else
        flags = window->Flags;

    // Add to stack
    ImGuiWindow* parent_window = !g.CurrentWindowStack.empty() ? g.CurrentWindowStack.back() : NULL;
    g.CurrentWindowStack.push_back(window);
    SetCurrentWindow(window);
    CheckStacksSize(window, true);
    IM_ASSERT(parent_window != NULL || !(flags & ImGuiWindowFlags_ChildWindow));

    bool window_was_visible = (window->LastFrameActive == current_frame - 1);   // Not using !WasActive because the implicit "Debug" window would always toggle off->on
    if (flags & ImGuiWindowFlags_Popup)
    {
        ImGuiPopupRef& popup_ref = g.OpenPopupStack[g.CurrentPopupStack.Size];
        window_was_visible &= (window->PopupId == popup_ref.PopupId);
        window_was_visible &= (window == popup_ref.Window);
        popup_ref.Window = window;
        g.CurrentPopupStack.push_back(popup_ref);
        window->PopupId = popup_ref.PopupId;
    }

    // Process SetNextWindow***() calls
    bool window_pos_set_by_api = false, window_size_set_by_api = false;
    if (g.SetNextWindowPosCond)
    {
        const ImVec2 backup_cursor_pos = window->DC.CursorPos;                  // FIXME: not sure of the exact reason of this anymore :( need to look into that.
        if (!window_was_visible) window->SetWindowPosAllowFlags |= ImGuiSetCond_Appearing;
        window_pos_set_by_api = (window->SetWindowPosAllowFlags & g.SetNextWindowPosCond) != 0;
        if (window_pos_set_by_api && ImLengthSqr(g.SetNextWindowPosVal - ImVec2(-FLT_MAX,-FLT_MAX)) < 0.001f)
        {
            window->SetWindowPosCenterWanted = true;                            // May be processed on the next frame if this is our first frame and we are measuring size
            window->SetWindowPosAllowFlags &= ~(ImGuiSetCond_Once | ImGuiSetCond_FirstUseEver | ImGuiSetCond_Appearing);
        }
        else
        {
            ImGui::SetWindowPos(g.SetNextWindowPosVal, g.SetNextWindowPosCond);
        }
        window->DC.CursorPos = backup_cursor_pos;
        g.SetNextWindowPosCond = 0;
    }
    if (g.SetNextWindowSizeCond)
    {
        if (!window_was_visible) window->SetWindowSizeAllowFlags |= ImGuiSetCond_Appearing;
        window_size_set_by_api = (window->SetWindowSizeAllowFlags & g.SetNextWindowSizeCond) != 0;
        ImGui::SetWindowSize(g.SetNextWindowSizeVal, g.SetNextWindowSizeCond);
        g.SetNextWindowSizeCond = 0;
    }
    if (g.SetNextWindowContentSizeCond)
    {
        window->SizeContentsExplicit = g.SetNextWindowContentSizeVal;
        g.SetNextWindowContentSizeCond = 0;
    }
    else if (first_begin_of_the_frame)
    {
        window->SizeContentsExplicit = ImVec2(0.0f, 0.0f);
    }
    if (g.SetNextWindowCollapsedCond)
    {
        if (!window_was_visible) window->SetWindowCollapsedAllowFlags |= ImGuiSetCond_Appearing;
        ImGui::SetWindowCollapsed(g.SetNextWindowCollapsedVal, g.SetNextWindowCollapsedCond);
        g.SetNextWindowCollapsedCond = 0;
    }
    if (g.SetNextWindowFocus)
    {
        ImGui::SetWindowFocus();
        g.SetNextWindowFocus = false;
    }

    // Update known root window (if we are a child window, otherwise window == window->RootWindow)
    int root_idx, root_non_popup_idx;
    for (root_idx = g.CurrentWindowStack.Size - 1; root_idx > 0; root_idx--)
        if (!(g.CurrentWindowStack[root_idx]->Flags & ImGuiWindowFlags_ChildWindow))
            break;
    for (root_non_popup_idx = root_idx; root_non_popup_idx > 0; root_non_popup_idx--)
        if (!(g.CurrentWindowStack[root_non_popup_idx]->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup)))
            break;
    window->RootWindow = g.CurrentWindowStack[root_idx];
    window->RootNonPopupWindow = g.CurrentWindowStack[root_non_popup_idx];      // This is merely for displaying the TitleBgActive color.

    // Default alpha
    if (bg_alpha < 0.0f)
        bg_alpha = 1.0f; //It was 0.7f (e.g. style.WindowFillAlphaDefault);

    // When reusing window again multiple times a frame, just append content (don't need to setup again)
    if (first_begin_of_the_frame)
    {
        window->Active = true;
        window->BeginCount = 0;
        window->DrawList->Clear();
        window->ClipRect = ImVec4(-FLT_MAX,-FLT_MAX,+FLT_MAX,+FLT_MAX);
        window->LastFrameActive = current_frame;
        window->IDStack.resize(1);

        // Setup texture, outer clipping rectangle
        window->DrawList->PushTextureID(g.Font->ContainerAtlas->TexID);
        ImRect fullscreen_rect(GetVisibleRect());
        if ((flags & ImGuiWindowFlags_ChildWindow) && !(flags & (ImGuiWindowFlags_ComboBox|ImGuiWindowFlags_Popup)))
            PushClipRect(parent_window->ClipRect.Min, parent_window->ClipRect.Max, true);
        else
            PushClipRect(fullscreen_rect.Min, fullscreen_rect.Max, true);

        // New windows appears in front
        if (!window_was_visible)
        {
            window->AutoPosLastDirection = -1;

            if (!(flags & ImGuiWindowFlags_NoFocusOnAppearing))
                if (!(flags & (ImGuiWindowFlags_ChildWindow|ImGuiWindowFlags_Tooltip)) || (flags & ImGuiWindowFlags_Popup))
                    FocusWindow(window);

            // Popup first latch mouse position, will position itself when it appears next frame
            if ((flags & ImGuiWindowFlags_Popup) != 0 && !window_pos_set_by_api)
                window->PosFloat = g.IO.MousePos;
        }

        // Collapse window by double-clicking on title bar
        // At this point we don't have a clipping rectangle setup yet, so we can use the title bar area for hit detection and drawing
        if (!(flags & ImGuiWindowFlags_NoTitleBar))// && !(flags & ImGuiWindowFlags_NoCollapse))
        {
            ImRect title_bar_rect = window->TitleBarRect();
            /*
            if (g.HoveredWindow == window && IsMouseHoveringRect(title_bar_rect.Min, title_bar_rect.Max) && g.IO.MouseDoubleClicked[0])
            {
                window->Collapsed = !window->Collapsed;
                if (!(flags & ImGuiWindowFlags_NoSavedSettings))
                    MarkSettingsDirty();
                FocusWindow(window);
            }
            */
            float buttonsSize = 0;
            if (p_opened)   buttonsSize+=window->TitleBarHeight() - 4.0f +3.0f;
            if (p_undocked) buttonsSize+=window->TitleBarHeight() - 4.0f +3.0f;

            const ImRect title_bar_without_buttons_rect(title_bar_rect.Min,title_bar_rect.Max-ImVec2(buttonsSize,0));

            // NEW PART: resize by dragging title bar (and mousewheel ?)
            if (wd && pDraggingStarted && (wd->dockPos==ImGui::PanelManager::BOTTOM || gImGuiDockPanelManagerAddExtraTitleBarResizing || gImGuiDockPanelManagerActiveResizeSize==0) && (*pDraggingStarted || (g.HoveredWindow == window && IsMouseHoveringRect(title_bar_rect.Min, title_bar_rect.Max))))  {
                const ImGuiID resize_id = window->GetID("#RESIZE");
                bool hovered = false, held=false,wheel = false;//g.IO.MouseWheel!=0.f;
                if (!wheel) ButtonBehavior(title_bar_without_buttons_rect, resize_id, &hovered, &held);//, true);
                if (hovered || held || wheel) {
                    g.MouseCursor = wd->dockPos<PanelManager::TOP ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
                    if (!wd->isToggleWindow) wd->isResizing = true;
                }
                *pDraggingStarted = held;
                if (held || wheel)   {
                    ImVec2 newSize(0,0);
                    ImVec2 amount = wheel ? ImVec2(g.IO.MouseWheel*20.f,g.IO.MouseWheel*20.f) : g.IO.MouseDelta;
                    if (wd->dockPos==ImGui::PanelManager::LEFT || wd->dockPos==ImGui::PanelManager::TOP) newSize = ImMax(window->SizeFull + amount, style.WindowMinSize);
                    else newSize = ImMax(window->SizeFull - amount, style.WindowMinSize);
                    wd->length = wd->dockPos<ImGui::PanelManager::TOP ? newSize.x : newSize.y;
                }
            }
        }
        //else window->Collapsed = false;


        const bool window_appearing_after_being_hidden = (window->HiddenFrames == 1);
        if (window->HiddenFrames > 0)
            window->HiddenFrames--;

        // SIZE

        // Save contents size from last frame for auto-fitting (unless explicitly specified)
        window->SizeContents.x = (window->SizeContentsExplicit.x != 0.0f) ? window->SizeContentsExplicit.x : ((window_is_new ? 0.0f : window->DC.CursorMaxPos.x - window->Pos.x) + window->Scroll.x);
        window->SizeContents.y = (window->SizeContentsExplicit.y != 0.0f) ? window->SizeContentsExplicit.y : ((window_is_new ? 0.0f : window->DC.CursorMaxPos.y - window->Pos.y) + window->Scroll.y);

        // Hide popup/tooltip window when first appearing while we measure size (because we recycle them)
        if ((flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0 && !window_was_visible)
        {
            window->HiddenFrames = 1;
            if (flags & ImGuiWindowFlags_AlwaysAutoResize)
            {
                if (!window_size_set_by_api)
                    window->Size = window->SizeFull = ImVec2(0.f, 0.f);
                window->SizeContents = ImVec2(0.f, 0.f);
            }
        }

        // Lock window padding so that altering the ShowBorders flag for children doesn't have side-effects.
        window->WindowPadding = ((flags & ImGuiWindowFlags_ChildWindow) && !(flags & (ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_ComboBox | ImGuiWindowFlags_Popup))) ? ImVec2(0,0) : style.WindowPadding;

        // Calculate auto-fit size
        ImVec2 size_auto_fit;
        if ((flags & ImGuiWindowFlags_Tooltip) != 0)
        {
            // Tooltip always resize. We keep the spacing symmetric on both axises for aesthetic purpose.
            size_auto_fit = window->SizeContents + window->WindowPadding - ImVec2(0.0f, style.ItemSpacing.y);
        }
        else
        {
            size_auto_fit = ImClamp(window->SizeContents + window->WindowPadding, style.WindowMinSize, ImMax(style.WindowMinSize, g.IO.DisplaySize - window->WindowPadding));

            // Handling case of auto fit window not fitting in screen on one axis, we are growing auto fit size on the other axis to compensate for expected scrollbar. FIXME: Might turn bigger than DisplaySize-WindowPadding.
            if (size_auto_fit.x < window->SizeContents.x && !(flags & ImGuiWindowFlags_NoScrollbar) && (flags & ImGuiWindowFlags_HorizontalScrollbar))
                size_auto_fit.y += style.ScrollbarSize;
            if (size_auto_fit.y < window->SizeContents.y && !(flags & ImGuiWindowFlags_NoScrollbar))
                size_auto_fit.x += style.ScrollbarSize;
            size_auto_fit.y = ImMax(size_auto_fit.y - style.ItemSpacing.y, 0.0f);
        }

        // Handle automatic resize
        if (window->Collapsed)
        {
            // We still process initial auto-fit on collapsed windows to get a window width,
            // But otherwise we don't honor ImGuiWindowFlags_AlwaysAutoResize when collapsed.
            if (window->AutoFitFramesX > 0)
                window->SizeFull.x = window->AutoFitOnlyGrows ? ImMax(window->SizeFull.x, size_auto_fit.x) : size_auto_fit.x;
            if (window->AutoFitFramesY > 0)
                window->SizeFull.y = window->AutoFitOnlyGrows ? ImMax(window->SizeFull.y, size_auto_fit.y) : size_auto_fit.y;
            window->Size = window->TitleBarRect().GetSize();
        }
        else
        {
            if ((flags & ImGuiWindowFlags_AlwaysAutoResize) && !window_size_set_by_api)
            {
                window->SizeFull = size_auto_fit;
            }
            else if ((window->AutoFitFramesX > 0 || window->AutoFitFramesY > 0) && !window_size_set_by_api)
            {
                // Auto-fit only grows during the first few frames
                if (window->AutoFitFramesX > 0)
                    window->SizeFull.x = window->AutoFitOnlyGrows ? ImMax(window->SizeFull.x, size_auto_fit.x) : size_auto_fit.x;
                if (window->AutoFitFramesY > 0)
                    window->SizeFull.y = window->AutoFitOnlyGrows ? ImMax(window->SizeFull.y, size_auto_fit.y) : size_auto_fit.y;
                if (!(flags & ImGuiWindowFlags_NoSavedSettings))
                    MarkIniSettingsDirty();
            }
            window->Size = window->SizeFull;
        }

        // Minimum window size
        if (!(flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_AlwaysAutoResize)))
        {
            window->SizeFull = ImMax(window->SizeFull, style.WindowMinSize);
            if (!window->Collapsed)
                window->Size = window->SizeFull;
        }

        // POSITION

        // Position child window
        if (flags & ImGuiWindowFlags_ChildWindow)
            parent_window->DC.ChildWindows.push_back(window);
        if ((flags & ImGuiWindowFlags_ChildWindow) && !(flags & ImGuiWindowFlags_Popup))
        {
            window->Pos = window->PosFloat = parent_window->DC.CursorPos;
            window->Size = window->SizeFull = size; // NB: argument name 'size_on_first_use' misleading here, it's really just 'size' as provided by user.
        }

        bool window_pos_center = false;
        window_pos_center |= (window->SetWindowPosCenterWanted && window->HiddenFrames == 0);
        window_pos_center |= ((flags & ImGuiWindowFlags_Modal) && !window_pos_set_by_api && window_appearing_after_being_hidden);
        if (window_pos_center)
        {
            // Center (any sort of window)
            //SetWindowPos(ImMax(style.DisplaySafeAreaPadding, fullscreen_rect.GetCenter() - window->SizeFull * 0.5f));
            SetWindowPos(window, ImMax(style.DisplaySafeAreaPadding, fullscreen_rect.GetCenter() - window->SizeFull * 0.5f), 0);
        }
        else if (flags & ImGuiWindowFlags_ChildMenu)
        {
            IM_ASSERT(window_pos_set_by_api);
            ImRect rect_to_avoid;
            if (parent_window->DC.MenuBarAppending)
                rect_to_avoid = ImRect(-FLT_MAX, parent_window->Pos.y + parent_window->TitleBarHeight(), FLT_MAX, parent_window->Pos.y + parent_window->TitleBarHeight() + parent_window->MenuBarHeight());
            else
                rect_to_avoid = ImRect(parent_window->Pos.x + style.ItemSpacing.x, -FLT_MAX, parent_window->Pos.x + parent_window->Size.x - style.ItemSpacing.x - parent_window->ScrollbarSizes.x, FLT_MAX); // We want some overlap to convey the relative depth of each popup (here hard-coded to 4)
            window->PosFloat = FindBestPopupWindowPos(window->PosFloat, window->Size,  &window->AutoPosLastDirection, rect_to_avoid);
        }
        else if ((flags & ImGuiWindowFlags_Popup) != 0 && !window_pos_set_by_api && window_appearing_after_being_hidden)
        {
            ImRect rect_to_avoid(window->PosFloat.x - 1, window->PosFloat.y - 1, window->PosFloat.x + 1, window->PosFloat.y + 1);
            window->PosFloat = FindBestPopupWindowPos(window->PosFloat, window->Size,  &window->AutoPosLastDirection, rect_to_avoid);
        }

        // Position tooltip (always follows mouse)
        if ((flags & ImGuiWindowFlags_Tooltip) != 0 && !window_pos_set_by_api)
        {
            ImRect rect_to_avoid(g.IO.MousePos.x - 16, g.IO.MousePos.y - 8, g.IO.MousePos.x + 24, g.IO.MousePos.y + 24); // FIXME: Completely hard-coded. Perhaps center on cursor hit-point instead?
            window->PosFloat = FindBestPopupWindowPos(g.IO.MousePos, window->Size,  &window->AutoPosLastDirection, rect_to_avoid);
        }

        // User moving window (at the beginning of the frame to avoid input lag or sheering). Only valid for root windows.
        KeepAliveID(window->MoveId);
        if (g.ActiveId == window->MoveId)
        {
            if (g.IO.MouseDown[0])
            {
                if (!(flags & ImGuiWindowFlags_NoMove))
                {
                    window->PosFloat += g.IO.MouseDelta;
                    if (!(flags & ImGuiWindowFlags_NoSavedSettings))
                        MarkIniSettingsDirty();
                }
                IM_ASSERT(g.MovedWindow != NULL);
                FocusWindow(g.MovedWindow);
            }
            else
            {
                SetActiveID(0);
                g.MovedWindow = NULL;   // Not strictly necessary but doing it for sanity.
            }
        }

        // Clamp position so it stays visible
        if (!(flags & ImGuiWindowFlags_ChildWindow) && !(flags & ImGuiWindowFlags_Tooltip))
        {
            if (!window_pos_set_by_api && window->AutoFitFramesX <= 0 && window->AutoFitFramesY <= 0 && g.IO.DisplaySize.x > 0.0f && g.IO.DisplaySize.y > 0.0f) // Ignore zero-sized display explicitly to avoid losing positions if a window manager reports zero-sized window when initializing or minimizing.
            {
                ImVec2 padding = ImMax(style.DisplayWindowPadding, style.DisplaySafeAreaPadding);
                window->PosFloat = ImMax(window->PosFloat + window->Size, padding) - window->Size;
                window->PosFloat = ImMin(window->PosFloat, g.IO.DisplaySize - padding);
            }
        }
        window->Pos = ImVec2((float)(int)window->PosFloat.x, (float)(int)window->PosFloat.y);

        // Default item width. Make it proportional to window size if window manually resizes
        if (window->Size.x > 0.0f && !(flags & ImGuiWindowFlags_Tooltip) && !(flags & ImGuiWindowFlags_AlwaysAutoResize))
            window->ItemWidthDefault = (float)(int)(window->Size.x * 0.65f);
        else
            window->ItemWidthDefault = (float)(int)(g.FontSize * 16.0f);

        // Prepare for focus requests
        window->FocusIdxAllRequestCurrent = (window->FocusIdxAllRequestNext == INT_MAX || window->FocusIdxAllCounter == -1) ? INT_MAX : (window->FocusIdxAllRequestNext + (window->FocusIdxAllCounter+1)) % (window->FocusIdxAllCounter+1);
        window->FocusIdxTabRequestCurrent = (window->FocusIdxTabRequestNext == INT_MAX || window->FocusIdxTabCounter == -1) ? INT_MAX : (window->FocusIdxTabRequestNext + (window->FocusIdxTabCounter+1)) % (window->FocusIdxTabCounter+1);
        window->FocusIdxAllCounter = window->FocusIdxTabCounter = -1;
        window->FocusIdxAllRequestNext = window->FocusIdxTabRequestNext = INT_MAX;

        // Apply scrolling
        if (window->ScrollTarget.x < FLT_MAX)
        {
            window->Scroll.x = window->ScrollTarget.x;
            window->ScrollTarget.x = FLT_MAX;
        }
        if (window->ScrollTarget.y < FLT_MAX)
        {
            float center_ratio = window->ScrollTargetCenterRatio.y;
            window->Scroll.y = window->ScrollTarget.y - ((1.0f - center_ratio) * window->TitleBarHeight()) - (center_ratio * window->SizeFull.y);
            window->ScrollTarget.y = FLT_MAX;
        }
        window->Scroll = ImMax(window->Scroll, ImVec2(0.0f, 0.0f));
        if (!window->Collapsed && !window->SkipItems)
            window->Scroll = ImMin(window->Scroll, ImMax(ImVec2(0.0f, 0.0f), window->SizeContents - window->SizeFull + window->ScrollbarSizes));

        // Modal window darkens what is behind them
        if ((flags & ImGuiWindowFlags_Modal) != 0 && window == GetFrontMostModalRootWindow())
            window->DrawList->AddRectFilled(fullscreen_rect.Min, fullscreen_rect.Max, GetColorU32(ImGuiCol_ModalWindowDarkening, g.ModalWindowDarkeningRatio));


        // Draw window + handle manual resize
        ImRect title_bar_rect = window->TitleBarRect();
        const float window_rounding = (flags & ImGuiWindowFlags_ChildWindow) ? style.ChildWindowRounding : style.WindowRounding;
        if (window->Collapsed)
        {
            // Draw title bar only
            window->DrawList->AddRectFilled(title_bar_rect.GetTL(), title_bar_rect.GetBR(), GetColorU32(ImGuiCol_TitleBgCollapsed), window_rounding);
            if ((flags & ImGuiWindowFlags_ShowBorders) || gImGuiDockPanelManagerAlwaysDrawExternalBorders)
            {
                window->DrawList->AddRect(title_bar_rect.GetTL()+ImVec2(1,1), title_bar_rect.GetBR()+ImVec2(1,1), GetColorU32(ImGuiCol_BorderShadow), window_rounding);
                window->DrawList->AddRect(title_bar_rect.GetTL(), title_bar_rect.GetBR(), GetColorU32(ImGuiCol_Border), window_rounding);
            }
        }
        else
        {

            ImU32 resize_col = 0;
            const float resize_corner_size = ImMax(g.FontSize * 1.35f, window_rounding + 1.0f + g.FontSize * 0.2f);
            /*if (!(flags & ImGuiWindowFlags_AlwaysAutoResize) && window->AutoFitFramesX <= 0 && window->AutoFitFramesY <= 0 && !(flags & ImGuiWindowFlags_NoResize))
            {
                // Manual resize
                const ImVec2 br = window->Rect().GetBR();
                const ImRect resize_rect(br - ImVec2(resize_corner_size * 0.75f, resize_corner_size * 0.75f), br);
                const ImGuiID resize_id = window->GetID("#RESIZE");
                bool hovered, held;
                ButtonBehavior(resize_rect, resize_id, &hovered, &held, ImGuiButtonFlags_FlattenChilds);
                resize_col = GetColorU32(held ? ImGuiCol_ResizeGripActive : hovered ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip);

                if (hovered || held)
                    g.MouseCursor = ImGuiMouseCursor_ResizeNWSE;

                if (g.HoveredWindow == window && held && g.IO.MouseDoubleClicked[0])
                {
                    // Manual auto-fit when double-clicking
                    window->SizeFull = size_auto_fit;
                    if (!(flags & ImGuiWindowFlags_NoSavedSettings))
                        MarkSettingsDirty();
                    SetActiveID(0);
                }
                else if (held)
                {
                    window->SizeFull = ImMax(window->SizeFull + g.IO.MouseDelta, style.WindowMinSize);
                    if (!(flags & ImGuiWindowFlags_NoSavedSettings))
                        MarkSettingsDirty();
                }

                window->Size = window->SizeFull;
                title_bar_rect = window->TitleBarRect();
            }*/

            // NEW PART: resize by dragging window border
            if (gImGuiDockPanelManagerActiveResizeSize>0 && !(flags & ImGuiWindowFlags_AlwaysAutoResize) && window->AutoFitFramesX <= 0 && window->AutoFitFramesY <= 0
                && pDraggingStarted && wd && (wd->dockPos != ImGui::PanelManager::BOTTOM || (flags & ImGuiWindowFlags_NoTitleBar))
            )
            {
                ImVec2 rectMin(0,0),rectMax(0,0);
                if (wd->dockPos == ImGui::PanelManager::LEFT)   {
                    rectMin = window->Rect().GetTR(); rectMin.x-=gImGuiDockPanelManagerActiveResizeSize;rectMin.y+=title_bar_rect.GetHeight();
                    rectMax = window->Rect().GetBR();
                }
                else if (wd->dockPos == ImGui::PanelManager::RIGHT) {
                    rectMin = window->Rect().GetTL();rectMin.y+=title_bar_rect.GetHeight();
                    rectMax = window->Rect().GetBL();rectMax.x+=gImGuiDockPanelManagerActiveResizeSize;
                }
                else if (wd->dockPos == ImGui::PanelManager::TOP) {
                    rectMin = window->Rect().GetBL();rectMin.y-=gImGuiDockPanelManagerActiveResizeSize;
                    rectMax = window->Rect().GetBR();
                }
                else if (wd->dockPos == ImGui::PanelManager::BOTTOM) {
                    rectMin = window->Rect().GetTL();
                    rectMax = window->Rect().GetTR();rectMax.y+=gImGuiDockPanelManagerActiveResizeSize;
                }

                const ImRect resize_rect(rectMin,rectMax);
                if (*pDraggingStarted || (g.HoveredWindow == window && IsMouseHoveringRect(resize_rect.Min, resize_rect.Max)))    {
                    const ImGuiID resize_id = window->GetID("#RESIZE");
                    bool hovered = false, held=false,wheel = false;
                    if (!wheel) ButtonBehavior(resize_rect, resize_id, &hovered, &held);
                    if (hovered || held || wheel) {
                        g.MouseCursor = wd->dockPos<PanelManager::TOP ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
                        if (!wd->isToggleWindow) wd->isResizing = true;
                    }
                    *pDraggingStarted = held;
                    if (held || wheel)   {
                        ImVec2 newSize(0,0);
                        ImVec2 amount = wheel ? ImVec2(g.IO.MouseWheel*20.f,g.IO.MouseWheel*20.f) : g.IO.MouseDelta;
                        if (wd->dockPos==ImGui::PanelManager::LEFT || wd->dockPos==ImGui::PanelManager::TOP) newSize = ImMax(window->SizeFull + amount, style.WindowMinSize);
                        else newSize = ImMax(window->SizeFull - amount, style.WindowMinSize);
                        wd->length = wd->dockPos<ImGui::PanelManager::TOP ? newSize.x : newSize.y;
                    }

                }
            }

            // Scrollbars
            window->ScrollbarY = (window->SizeContents.y > window->Size.y + style.ItemSpacing.y) && !(flags & ImGuiWindowFlags_NoScrollbar);
            window->ScrollbarX = (window->SizeContents.x > window->Size.x - (window->ScrollbarY ? style.ScrollbarSize : 0.0f) - window->WindowPadding.x) && !(flags & ImGuiWindowFlags_NoScrollbar) && (flags & ImGuiWindowFlags_HorizontalScrollbar);
            window->ScrollbarSizes = ImVec2(window->ScrollbarY ? style.ScrollbarSize : 0.0f, window->ScrollbarX ? style.ScrollbarSize : 0.0f);
            //window->BorderSize = (flags & ImGuiWindowFlags_ShowBorders) ? 1.0f : 0.0f;

            // Window background, Default alpha
            ImGuiCol bg_color_idx = ImGuiCol_WindowBg;
            if ((flags & ImGuiWindowFlags_ComboBox) != 0)
                bg_color_idx = ImGuiCol_ComboBg;
            else if ((flags & ImGuiWindowFlags_Tooltip) != 0 || (flags & ImGuiWindowFlags_Popup) != 0)
                bg_color_idx = ImGuiCol_PopupBg;
            else if ((flags & ImGuiWindowFlags_ChildWindow) != 0)
                bg_color_idx = ImGuiCol_ChildWindowBg;
            ImVec4 bg_color = style.Colors[bg_color_idx];
            if (bg_alpha >= 0.0f)
                bg_color.w = bg_alpha;
            bg_color.w *= style.Alpha;
            if (bg_color.w > 0.0f)
                window->DrawList->AddRectFilled(window->Pos, window->Pos+window->Size, ColorConvertFloat4ToU32(bg_color), window_rounding);
                //window->DrawList->AddRectFilled(window->Pos+ImVec2(0,window->TitleBarHeight()), window->Pos+window->Size, ColorConvertFloat4ToU32(bg_color), window_rounding, (flags & ImGuiWindowFlags_NoTitleBar) ? ImGuiCorner_All : ImGuiCorner_BottomLeft|ImGuiCorner_BottomRight);


            // Title bar
            if (!(flags & ImGuiWindowFlags_NoTitleBar)) {
#               if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_DRAW_METHODS))
                if (gImGuiDockpanelManagerExtendedStyle)    {
                    ImU32 col = GetColorU32((g.FocusedWindow && window->RootNonPopupWindow == g.FocusedWindow->RootNonPopupWindow) ? ImGuiCol_TitleBgActive : ImGuiCol_TitleBg);
                    ImGui::ImDrawListAddRectWithVerticalGradient(window->DrawList,
                        title_bar_rect.GetTL(), title_bar_rect.GetBR(),col,0.15f,0,
                        window_rounding, ImGuiCorner_TopLeft|ImGuiCorner_TopRight,1.f,ImGui::GetStyle().AntiAliasedShapes
                    );
                }
                else
#               endif // (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_DRAW_METHODS))
                {
                    window->DrawList->AddRectFilled(title_bar_rect.GetTL(), title_bar_rect.GetBR(), GetColorU32((g.FocusedWindow && window->RootNonPopupWindow == g.FocusedWindow->RootNonPopupWindow) ? ImGuiCol_TitleBgActive : ImGuiCol_TitleBg), window_rounding, ImGuiCorner_TopLeft|ImGuiCorner_TopRight);
                }
            }

            // Menu bar
            if (flags & ImGuiWindowFlags_MenuBar)
            {
                ImRect menu_bar_rect = window->MenuBarRect();
                //if (flags & ImGuiWindowFlags_ShowBorders) window->DrawList->AddLine(menu_bar_rect.GetBL()-ImVec2(0,0), menu_bar_rect.GetBR()-ImVec2(0,0), GetColorU32(ImGuiCol_Border));
                window->DrawList->AddRectFilled(menu_bar_rect.GetTL(), menu_bar_rect.GetBR(), GetColorU32(ImGuiCol_MenuBarBg), (flags & ImGuiWindowFlags_NoTitleBar) ? window_rounding : 0.0f, ImGuiCorner_TopLeft|ImGuiCorner_TopRight);
            }

            // Scrollbars
            if (window->ScrollbarX)
                Scrollbar(window, true);
            if (window->ScrollbarY)
                Scrollbar(window, false);

            // Render resize grip
            // (after the input handling so we don't have a frame of latency)
            if (!(flags & ImGuiWindowFlags_NoResize))
            {
                const ImVec2 br = window->Rect().GetBR();
                window->DrawList->PathLineTo(br + ImVec2(-resize_corner_size, 0.0f));
                window->DrawList->PathLineTo(br + ImVec2(0.0f, -resize_corner_size));
                window->DrawList->PathArcToFast(ImVec2(br.x - window_rounding, br.y - window_rounding), window_rounding, 0, 3);
                window->DrawList->PathFill(resize_col);
            }

            // Borders
            if ((flags & ImGuiWindowFlags_ShowBorders) || gImGuiDockPanelManagerAlwaysDrawExternalBorders)
            {
                window->DrawList->AddRect(window->Pos+ImVec2(1,1), window->Pos+window->Size+ImVec2(1,1), GetColorU32(ImGuiCol_BorderShadow), window_rounding);
                window->DrawList->AddRect(window->Pos, window->Pos+window->Size, GetColorU32(ImGuiCol_Border), window_rounding);
            }
            if ((flags & ImGuiWindowFlags_ShowBorders) && !(flags & ImGuiWindowFlags_NoTitleBar))
                window->DrawList->AddLine(title_bar_rect.GetBL(), title_bar_rect.GetBR(), GetColorU32(ImGuiCol_Border));

        }

        // Update ContentsRegionMax. All the variable it depends on are set above in this function.
        window->ContentsRegionRect.Min.x = -window->Scroll.x + window->WindowPadding.x;
        window->ContentsRegionRect.Min.y = -window->Scroll.y + window->WindowPadding.y + window->TitleBarHeight() + window->MenuBarHeight();
        window->ContentsRegionRect.Max.x = -window->Scroll.x - window->WindowPadding.x + (window->SizeContentsExplicit.x != 0.0f ? window->SizeContentsExplicit.x : (window->Size.x - window->ScrollbarSizes.x));
        window->ContentsRegionRect.Max.y = -window->Scroll.y - window->WindowPadding.y + (window->SizeContentsExplicit.y != 0.0f ? window->SizeContentsExplicit.y : (window->Size.y - window->ScrollbarSizes.y));

        // Setup drawing context
        window->DC.IndentX = 0.0f + window->WindowPadding.x - window->Scroll.x;
        window->DC.ColumnsOffsetX = 0.0f;
        window->DC.CursorStartPos = window->Pos + ImVec2(window->DC.IndentX + window->DC.ColumnsOffsetX, window->TitleBarHeight() + window->MenuBarHeight() + window->WindowPadding.y - window->Scroll.y);
        window->DC.CursorPos = window->DC.CursorStartPos;
        window->DC.CursorPosPrevLine = window->DC.CursorPos;
        window->DC.CursorMaxPos = window->DC.CursorStartPos;
        window->DC.CurrentLineHeight = window->DC.PrevLineHeight = 0.0f;
        window->DC.CurrentLineTextBaseOffset = window->DC.PrevLineTextBaseOffset = 0.0f;
        window->DC.MenuBarAppending = false;
        window->DC.MenuBarOffsetX = ImMax(window->WindowPadding.x, style.ItemSpacing.x);
        window->DC.LogLinePosY = window->DC.CursorPos.y - 9999.0f;
        window->DC.ChildWindows.resize(0);
        window->DC.LayoutType = ImGuiLayoutType_Vertical;
        window->DC.ItemWidth = window->ItemWidthDefault;
        window->DC.TextWrapPos = -1.0f; // disabled
        window->DC.AllowKeyboardFocus = true;
        window->DC.ButtonRepeat = false;
        window->DC.ItemWidthStack.resize(0);
        window->DC.AllowKeyboardFocusStack.resize(0);
        window->DC.ButtonRepeatStack.resize(0);
        window->DC.TextWrapPosStack.resize(0);
        window->DC.ColumnsCurrent = 0;
        window->DC.ColumnsCount = 1;
        window->DC.ColumnsStartPosY = window->DC.CursorPos.y;
        window->DC.ColumnsCellMinY = window->DC.ColumnsCellMaxY = window->DC.ColumnsStartPosY;
        window->DC.TreeDepth = 0;
        window->DC.StateStorage = &window->StateStorage;
        window->DC.GroupStack.resize(0);
        window->DC.ColorEditMode = ImGuiColorEditMode_UserSelect;
        window->MenuColumns.Update(3, style.ItemSpacing.x, !window_was_visible);

        if (window->AutoFitFramesX > 0)
            window->AutoFitFramesX--;
        if (window->AutoFitFramesY > 0)
            window->AutoFitFramesY--;

        // Title bar
        if (!(flags & ImGuiWindowFlags_NoTitleBar))
        {
            if (p_opened != NULL)   {
                //*p_opened = CloseWindowButton(p_opened);
                const float pad = 2.0f;
                const float rad = (window->TitleBarHeight() - pad*2.0f) * 0.5f;
                const bool pressed = CloseButton(window->GetID("#CLOSE"), window->Rect().GetTR() + ImVec2(-pad - rad, pad + rad), rad);
                if (p_opened != NULL && pressed) *p_opened = false;
                *p_opened = pressed;    // Bad but safer
            }
            if (p_undocked != NULL)
                DockWindowButton(p_undocked,p_opened);

            const ImVec2 text_size = CalcTextSize(name, NULL, true);
            if (!(flags & ImGuiWindowFlags_NoCollapse))
                RenderCollapseTriangle(window->Pos + style.FramePadding, !window->Collapsed, 1.0f);

            /*
            ImVec2 text_min = window->Pos + style.FramePadding;
            //ImVec2 text_max = window->Pos + ImVec2(window->Size.x - style.FramePadding.x, style.FramePadding.y*2 + text_size.y);
            //ImVec2 clip_max = ImVec2(window->Pos.x + window->Size.x - (p_opened ? title_bar_rect.GetHeight() - 3 : style.FramePadding.x), text_max.y); // Match the size of CloseWindowButton()
            ImVec2 text_max = window->Pos + ImVec2(window->Size.x - (p_opened ? (title_bar_rect.GetHeight()-3) : style.FramePadding.x) - (p_undocked ? (title_bar_rect.GetHeight()-3) : style.FramePadding.x), style.FramePadding.y*2 + text_size.y);
            ImVec2 clip_max = ImVec2(window->Pos.x + window->Size.x - (p_opened ? title_bar_rect.GetHeight() - 3 : style.FramePadding.x) - (p_undocked ? title_bar_rect.GetHeight() - 3 : style.FramePadding.x), text_max.y); // Match the size of CloseWindowButton()

            bool pad_left = (flags & ImGuiWindowFlags_NoCollapse) == 0;
            bool pad_right = (p_opened != NULL);
            if (style.WindowTitleAlign & ImGuiAlign_Center) pad_right = pad_left;
            if (pad_left) text_min.x += g.FontSize + style.ItemInnerSpacing.x;
            if (pad_right) text_max.x -= g.FontSize + style.ItemInnerSpacing.x;
            RenderTextClipped(text_min, text_max, name, NULL, &text_size, style.WindowTitleAlign, NULL, &clip_max);
            */

            ImVec2 text_min = window->Pos;
            ImVec2 text_max = window->Pos + ImVec2(window->Size.x, style.FramePadding.y*2 + text_size.y);
            ImRect clip_rect;
            clip_rect.Max = ImVec2(window->Pos.x + window->Size.x - (p_opened ? title_bar_rect.GetHeight() - 3 : style.FramePadding.x) - (p_undocked ? title_bar_rect.GetHeight() - 3 : style.FramePadding.x), text_max.y); // Match the size of CloseWindowButton()

            float pad_left = (flags & ImGuiWindowFlags_NoCollapse) == 0 ? (style.FramePadding.x + g.FontSize + style.ItemInnerSpacing.x) : style.FramePadding.x;
            float pad_right = (p_opened != NULL) ? (style.FramePadding.x + g.FontSize + style.ItemInnerSpacing.x) : style.FramePadding.x;

            if (style.WindowTitleAlign.x > 0.0f) pad_right = ImLerp(pad_right, pad_left, style.WindowTitleAlign.x);
            text_min.x += pad_left;
            text_max.x -= pad_right;
            clip_rect.Min = ImVec2(text_min.x, window->Pos.y);
            RenderTextClipped(text_min, text_max, name, NULL, &text_size, style.WindowTitleAlign, &clip_rect);

        }

        // Save clipped aabb so we can access it in constant-time in FindHoveredWindow()
        window->WindowRectClipped = window->Rect();
        window->WindowRectClipped.Clip(window->ClipRect);

        // Pressing CTRL+C while holding on a window copy its content to the clipboard
        // This works but 1. doesn't handle multiple Begin/End pairs, 2. recursing into another Begin/End pair - so we need to work that out and add better logging scope.
        // Maybe we can support CTRL+C on every element?
        /*
        if (g.ActiveId == move_id)
            if (g.IO.KeyCtrl && IsKeyPressedMap(ImGuiKey_C))
                ImGui::LogToClipboard();
        */
    }

    // Inner clipping rectangle
    // We set this up after processing the resize grip so that our clip rectangle doesn't lag by a frame
    // Note that if our window is collapsed we will end up with a null clipping rectangle which is the correct behavior.
    const ImRect title_bar_rect = window->TitleBarRect();
    const float border_size = ((flags & ImGuiWindowFlags_ShowBorders) || gImGuiDockPanelManagerAlwaysDrawExternalBorders) ? 1.0f : 0.0f;
    ImRect clip_rect;
    clip_rect.Min.x = title_bar_rect.Min.x + 0.5f + ImMax(border_size, window->WindowPadding.x*0.5f);
    clip_rect.Min.y = title_bar_rect.Max.y + window->MenuBarHeight() + 0.5f + border_size;
    clip_rect.Max.x = window->Pos.x + window->Size.x - window->ScrollbarSizes.x - ImMax(border_size, window->WindowPadding.x*0.5f);
    clip_rect.Max.y = window->Pos.y + window->Size.y - border_size - window->ScrollbarSizes.y;

    PushClipRect(clip_rect.Min, clip_rect.Max, true);

    // Clear 'accessed' flag last thing
    if (first_begin_of_the_frame)
        window->Accessed = false;
    window->BeginCount++;

    // Child window can be out of sight and have "negative" clip windows.
    // Mark them as collapsed so commands are skipped earlier (we can't manually collapse because they have no title bar).
    if (flags & ImGuiWindowFlags_ChildWindow)
    {
        IM_ASSERT((flags & ImGuiWindowFlags_NoTitleBar) != 0);
        window->Collapsed = parent_window && parent_window->Collapsed;

        if (!(flags & ImGuiWindowFlags_AlwaysAutoResize) && window->AutoFitFramesX <= 0 && window->AutoFitFramesY <= 0)
            window->Collapsed |= (window->ClipRect.Min.x >= window->ClipRect.Max.x || window->ClipRect.Min.y >= window->ClipRect.Max.y);

        // We also hide the window from rendering because we've already added its border to the command list.
        // (we could perform the check earlier in the function but it is simpler at this point)
        if (window->Collapsed)
            window->Active = false;
    }
    if (style.Alpha <= 0.0f)
        window->Active = false;

    // Return false if we don't intend to display anything to allow user to perform an early out optimization
    window->SkipItems = (window->Collapsed || !window->Active) && window->AutoFitFramesX <= 0 && window->AutoFitFramesY <= 0;
    return !window->SkipItems;
}
static void DockWindowEnd()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImGui::Columns(1, "#CloseColumns");
    PopClipRect();   // inner window clip rectangle

    // Stop logging
    if (!(window->Flags & ImGuiWindowFlags_ChildWindow))    // FIXME: add more options for scope of logging
        ImGui::LogFinish();

    // Pop
    // NB: we don't clear 'window->RootWindow'. The pointer is allowed to live until the next call to Begin().
    g.CurrentWindowStack.pop_back();
    if (window->Flags & ImGuiWindowFlags_Popup)
        g.CurrentPopupStack.pop_back();
    CheckStacksSize(window, false);
    SetCurrentWindow(g.CurrentWindowStack.empty() ? NULL : g.CurrentWindowStack.back());
}

} // namespace ImGui


void ImGui::PanelManager::Pane::AssociatedWindow::updateSizeInHoverMode(const ImGui::PanelManager &mgr, const ImGui::PanelManager::Pane &pane, size_t winAndBarIndex) const  {
    if (!isValid() || !dirty) return;
    const Toolbutton* button = pane.bar.getButton(winAndBarIndex);
    const bool togglable = button && button->isToggleButton;
    if (!togglable)  {
        //fprintf(stderr,"ok \"%s\"\n",windowName);
        if (mgr.innerBarQuadSize.x<=0 || mgr.innerBarQuadSize.y<=0) mgr.calculateInnerBarQuadPlacement();
        static const float DefaultHoverSizePortion = 0.2f;
        switch (pane.pos)   {
        case TOP:
            if (sizeHoverMode<0) sizeHoverMode = ImGui::GetIO().DisplaySize.y * DefaultHoverSizePortion;
            if (sizeHoverMode<2.f * ImGui::GetStyle().WindowMinSize.x)   sizeHoverMode = 2.f * ImGui::GetStyle().WindowMinSize.x;// greater than "ImGui::GetStyle().WindowMinSize.x", otherwise the title bar is not accessible
            curPos.x = mgr.innerBarQuadPos.x;
            curPos.y = mgr.innerBarQuadPos.y;
            curSize.x = mgr.innerBarQuadSize.x;
            if (sizeHoverMode > mgr.innerBarQuadSize.y) sizeHoverMode = mgr.innerBarQuadSize.y;
            curSize.y = sizeHoverMode;
            break;
        case LEFT:
            if (sizeHoverMode<0) sizeHoverMode = ImGui::GetIO().DisplaySize.x * DefaultHoverSizePortion;
            if (sizeHoverMode<2.f * ImGui::GetStyle().WindowMinSize.x)   sizeHoverMode = 2.f * ImGui::GetStyle().WindowMinSize.x;// greater than "ImGui::GetStyle().WindowMinSize.x", otherwise the title bar is not accessible
            curPos.x = mgr.innerBarQuadPos.x;
            curPos.y = mgr.innerBarQuadPos.y;
            if (sizeHoverMode > mgr.innerBarQuadSize.x) sizeHoverMode = mgr.innerBarQuadSize.x;
            curSize.x = sizeHoverMode;
            curSize.y = mgr.innerBarQuadSize.y;
            break;
        case RIGHT:
            if (sizeHoverMode<0) sizeHoverMode = ImGui::GetIO().DisplaySize.x * DefaultHoverSizePortion;
            curPos.y = mgr.innerBarQuadPos.y;
            if (sizeHoverMode > mgr.innerBarQuadSize.x) sizeHoverMode = mgr.innerBarQuadSize.x;
            curSize.x = sizeHoverMode;
            curPos.x = mgr.innerBarQuadPos.x + mgr.innerBarQuadSize.x - curSize.x;
            curSize.y = mgr.innerBarQuadSize.y;
            break;
        case BOTTOM:
            if (sizeHoverMode<0) sizeHoverMode = ImGui::GetIO().DisplaySize.y * DefaultHoverSizePortion;
            curPos.x = mgr.innerBarQuadPos.x;
            curSize.x =  mgr.innerBarQuadSize.x;
            if (sizeHoverMode > mgr.innerBarQuadSize.y) sizeHoverMode = mgr.innerBarQuadSize.y;
            curSize.y = sizeHoverMode;
            curPos.y = mgr.innerBarQuadPos.y + mgr.innerBarQuadSize.y - curSize.y;
            break;
        default:
            IM_ASSERT("Error: unknown case\n");
            break;
        }
    }
    dirty = false;
}


void ImGui::PanelManager::Pane::AssociatedWindow::draw(const ImGui::PanelManager &mgr, const ImGui::PanelManager::Pane &pane, size_t winAndBarIndex) const  {
    if (!isValid()) return;

    const Toolbutton* button = pane.bar.getButton(winAndBarIndex);
    const bool togglable = button && button->isToggleButton;
    const bool togglableAndVisible = togglable && button->isDown;
    const bool selected = (this==pane.getSelectedWindow());
    const bool hovered = !selected && (pane.allowHoverOnTogglableWindows ? !togglableAndVisible : !togglable) && (this==pane.getHoverWindow() || persistHoverFocus);
    if (!togglableAndVisible && !selected && !hovered) return;

    if (dirty)  {
        if (selected) mgr.updateSizes();
        else if (hovered) updateSizeInHoverMode(mgr,pane,winAndBarIndex);
    }

    if (curSize.x<0 || curSize.y<0) return;

    float& sizeToUse = hovered ? sizeHoverMode : size;
    if (selected) persistHoverFocus = false;
    //else if (hovered) ImGui::SetNextWindowFocus();  // is this line necessary ?


    const float oldSize = sizeToUse;bool open = true;bool undocked = hovered;
    WindowData wd(windowName,curPos,curSize,togglable,pane.pos,sizeToUse,open,persistHoverFocus,userData);
    //===============================================================================================
    if (wd.isToggleWindow) windowDrawer(wd);
    else if (wd.size.x>=ImGui::GetStyle().WindowMinSize.x){
        ImGui::SetNextWindowPos(wd.pos);
        ImGui::SetNextWindowSize(wd.size);
            static const ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
            ImGuiWindowFlags windowFlagsTotal = wd.isToggleWindow ? ImGuiWindowFlags_NoSavedSettings : (windowFlags|mgr.dockedWindowsExtraFlags|extraWindowFlags);
            //if (selected && !wd.isToggleWindow) windowFlagsTotal|=ImGuiWindowFlags_NoBringToFrontOnFocus;  // optional line (To be tested). [It doesn't seem to cover all cases]
            if ((windowFlagsTotal & ImGuiWindowFlags_NoTitleBar)) open=!open;   // Terrible hack to make it work (but I remind that "open" was previously called "closed": so that had a sense!). See *p_opened = CloseWindowButton(p_opened); in BeginDockWindow.
        if (ImGui::DockWindowBegin(wd.name, &wd.open,&undocked, wd.size,mgr.dockedWindowsAlpha,windowFlagsTotal,&draggingStarted,&wd))     {
            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            ImGuiWindow* window = NULL;
            //wd.persistFocus = ImGui::IsMouseHoveringWindow();
            //-----------------------------------------------------------------------------------------------
            windowDrawer(wd);
            //-----------------------------------------------------------------------------------------------
            window = g.CurrentWindow;
            wd.persistFocus = (g.HoveredWindow == window || g.HoveredRootWindow == window || (hovered && wd.isResizing));
            if (hovered && !wd.persistFocus)    {
                const ImVec2& mp = ImGui::GetIO().MousePos;
                if (mp.x!=-1 && mp.y!=-1)   {
                    const ImRect bb(wd.pos,ImVec2(wd.pos.x+wd.size.x,wd.pos.y+wd.size.y));
                    wd.persistFocus = bb.Contains(mp);
                }
            }

            const bool allowManualResizeGrip = false;//true;    // I DON'T THINK THIS WORKS ANYMORE (TODO: Remove it)
            if (allowManualResizeGrip)  {
                ImRect resize_aabb(0,0,0,0);
                const ImGuiID resize_id = window->GetID("#RESIZE");
                float rounding=0; int rounding_corners=~0;
                float gripDist = 0;float gripSize = 8;
                bool hovered=false, held=false;ImVec2 newSize(-1,-1);
                const ImRect& aabb = window->Rect();
                const ImVec2& amin = aabb.Min;
                const ImVec2& amax = aabb.Max;
                switch (wd.dockPos) {
                case LEFT:
                    resize_aabb = ImRect(amax.x-gripDist-gripSize,
                                            amin.y+gripDist,
                                            amax.x-gripDist,
                                            amax.y-gripDist);
                    ButtonBehavior(resize_aabb, resize_id, &hovered, &held);//, true);
                    if (held) {
                        newSize = ImMax(window->SizeFull + g.IO.MouseDelta, style.WindowMinSize);
                        wd.length = newSize.x;
                    }
                    break;
                case RIGHT:
                    resize_aabb = ImRect(amin.x+gripDist,
                                            amin.y+gripDist,
                                            amin.x+gripDist+gripSize,
                                            amax.y-gripDist);
                    ButtonBehavior(resize_aabb, resize_id, &hovered, &held);//, true);
                    if (held) {
                        newSize = ImMax(window->SizeFull - g.IO.MouseDelta, style.WindowMinSize);
                        wd.length = newSize.x;
                    }
                    break;
                case TOP:
                    resize_aabb = ImRect(amin.x+gripDist,
                                            amax.y-gripDist-gripSize,
                                            amax.x-gripDist,
                                            amax.y-gripDist);
                    ButtonBehavior(resize_aabb, resize_id, &hovered, &held);//, true);
                    if (held) {
                        newSize = ImMax(window->SizeFull + g.IO.MouseDelta, style.WindowMinSize);
                        wd.length = newSize.y;
                    }
                    break;
                case BOTTOM:
                    resize_aabb = ImRect(amin.x+gripDist,
                                            amin.y+gripDist,
                                            amax.x-gripDist,
                                            amin.y+gripDist+gripSize);
                    ButtonBehavior(resize_aabb, resize_id, &hovered, &held);//, true);
                    if (held) {
                        newSize = ImMax(window->SizeFull - g.IO.MouseDelta, style.WindowMinSize);
                        wd.length = newSize.y;
                    }
                    break;
                default:
                    IM_ASSERT("Error");
                    break;
                }

                if (hovered || held)    {
                    const ImGuiCol resize_col = GetColorU32(held ? ImGuiCol_ResizeGripActive : hovered ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip);
                    window->DrawList->AddRectFilled(resize_aabb.Min, resize_aabb.Max, resize_col, rounding, rounding_corners);
                }
                if (held) wd.persistFocus = true;

            }

            if (sizeToUse!=oldSize) {
                dirty = true;
                // Better clamp size now if it's a "selected" window
                if (selected)   {
                    if (wd.dockPos<TOP) {
                        const float delta = mgr.innerQuadSize.x + oldSize;
                        if (sizeToUse>delta)  sizeToUse = delta;
                        const float minWidth = 2.f * ImGui::GetStyle().WindowMinSize.x; // greater than "ImGui::GetStyle().WindowMinSize.x", otherwise the title bar is not accessible
                        if (sizeToUse<minWidth) sizeToUse = minWidth;
                        curSize.x = sizeToUse;
                    }
                    else {
                        const float delta = mgr.innerQuadSize.y + oldSize;
                        if (sizeToUse>delta)  sizeToUse = delta;
                        curSize.y = sizeToUse;
                    }
                }
            }

        }
        ImGui::DockWindowEnd();
    }
    //================================================================================================
    if (open) {
        if (hovered) {
            persistHoverFocus = false;
        }
        //else if (togglableAndVisible) button->isDown = false;
        else if (selected) {
            pane.bar.setSelectedButtonIndex(-1);
            persistHoverFocus = false;  // optional line (when commented out leaves a selected window in hover move: we must press the close button another time to hide it)
	    dirty = true;mgr.updateSizes();  // Needed to update the innerQuadSize
	    dirty = true; // Needed so next time the hover window sizes are set
        }
    }
    else if (togglableAndVisible) button->isDown = false;
    if (!wd.isToggleWindow) {
        if (!undocked && hovered)   {
            pane.bar.setSelectedButtonIndex(winAndBarIndex);
            dirty = true;
        }
        else if (undocked && selected)   {
            pane.bar.setSelectedButtonIndex(-1);
            persistHoverFocus = true;  // optional line (when commented out leaves a selected window in hover move: we must press the close button another time to hide it)
            dirty = true; mgr.updateSizes();  // Needed to update the innerQuadSize
            dirty = true; // Needed so next time the hover window sizes are set
        }
    }
}


size_t ImGui::PanelManager::Pane::addButtonAndWindow(const ImGui::Toolbutton &button, const ImGui::PanelManager::Pane::AssociatedWindow &window, int insertPosition) {
    int numButtons = (int) bar.getNumButtons();
    if (numButtons>0 && insertPosition>=0 && insertPosition<numButtons)    {
        bar.addButton(bar.buttons[numButtons-1]);
        windows.push_back(windows[numButtons-1]);
        if (bar.selectedButtonIndex==numButtons-1) bar.selectedButtonIndex=numButtons;
        if (bar.hoverButtonIndex==numButtons-1) bar.hoverButtonIndex=numButtons;
        for (int i=numButtons-1;i>=insertPosition;i--)    {
            bar.buttons[i+1]=bar.buttons[i];
            windows[i+1]=windows[i];
            if (bar.selectedButtonIndex==i) bar.selectedButtonIndex=i+1;
            if (bar.hoverButtonIndex==i) bar.hoverButtonIndex=i+1;
        }
        bar.buttons[insertPosition]=button;
        windows[insertPosition]=window;
    }
    else {
        bar.addButton(button);
	windows.push_back(window);
	/*const int wsz = (int) windows.size();
	windows.resize(wsz+1);
	windows[wsz] = window;*/
    }
    bar.updatePositionAndSize();
    return getSize();
}


size_t ImGui::PanelManager::Pane::addSeparator(float pixels, int insertPosition)  {
    return addButtonOnly(Toolbutton("",NULL,ImVec2(0,0),ImVec2(0,0),ImVec2(pixels,pixels)),insertPosition);
}

size_t ImGui::PanelManager::Pane::addButtonOnly(const Toolbutton& button, int insertPosition)  {
    return addButtonAndWindow(button,AssociatedWindow(),insertPosition);
}


size_t ImGui::PanelManager::Pane::addClonedButtonAndWindow(const ImGui::PanelManager::Pane &sourcePane, const size_t sourcePaneEntryIndex, bool flipButtonHorizontally)   {
    IM_ASSERT(&sourcePane!=this);
    IM_ASSERT(sourcePane.getSize()>sourcePaneEntryIndex);
    const Toolbutton& sourceButton = *sourcePane.bar.getButton(sourcePaneEntryIndex);
    const Pane::AssociatedWindow& sourceWindow = sourcePane.windows[sourcePaneEntryIndex];
    IM_ASSERT(!sourceButton.isToggleButton);    // cloning toggle buttons is NOT supported
    if (sourceButton.isToggleButton) return getSize();
    size_t size = getSize();
    bar.addButton(sourceButton);
    windows.push_back(sourceWindow);
    if (flipButtonHorizontally) {
        Toolbutton& b = *bar.getButton(size);
        const float tmp = b.uv0.x;b.uv0.x=b.uv1.x;b.uv1.x=tmp;
    }
    bar.updatePositionAndSize();
    return size+1;
}


size_t ImGui::PanelManager::Pane::addClonedPane(const ImGui::PanelManager::Pane &sourcePane, bool flipButtonHorizontally, int sourcePaneEntryStartIndex, int sourcePaneEntryEndIndex)  {
    const int sourcePaneSize = (int)sourcePane.getSize();
    if (sourcePaneSize==0) return getSize();
    sourcePaneEntryEndIndex = (sourcePaneEntryEndIndex<0 || sourcePaneEntryEndIndex>=sourcePaneSize) ? (sourcePaneSize-1) : sourcePaneEntryEndIndex;
    sourcePaneEntryStartIndex = sourcePaneEntryStartIndex<0 ? 0 : sourcePaneEntryStartIndex>sourcePaneEntryEndIndex ? sourcePaneEntryEndIndex : sourcePaneEntryStartIndex;
    for (int i=sourcePaneEntryStartIndex;i<=sourcePaneEntryEndIndex;i++) {
        if (sourcePane.bar.getButton(i)->isToggleButton) continue;   // Not supported
        addClonedButtonAndWindow(sourcePane,i,flipButtonHorizontally);
    }
    return getSize();
}

size_t ImGui::PanelManager::Pane::deleteButtonAt(int index)   {
    int numButtons = (int) bar.getNumButtons();
    if (numButtons==0 || index<0 || index>=numButtons) return getSize();
    if (bar.selectedButtonIndex==index) bar.selectedButtonIndex=-1;
    if (bar.hoverButtonIndex==index) bar.hoverButtonIndex=-1;
    for (int i=index;i<numButtons-1;i++)    {
        bar.buttons[i]=bar.buttons[i+1];
        windows[i]=windows[i+1];
        if (bar.selectedButtonIndex==i+1) bar.selectedButtonIndex=i;
        if (bar.hoverButtonIndex==i+1) bar.hoverButtonIndex=i;
    }
    --numButtons;
    bar.buttons[numButtons].~Button();
    bar.buttons.resize(numButtons);
    windows.resize(numButtons);
    bar.updatePositionAndSize();
    return getSize();
}


size_t ImGui::PanelManager::Pane::getSize() const {
    IM_ASSERT((int)bar.getNumButtons()==windows.size());
    return windows.size();
}

void ImGui::PanelManager::Pane::setToolbarProperties(bool _keepAButtonSelected, bool _lightAllBarWhenHovered, const ImVec2 &_hvAlignmentsIn01, const ImVec2 &_opacityOffAndOn, const ImVec4 &_bg_col, const ImVec4 &_displayPortion) {
    bool _vertical = pos < TOP;
    ImVec2 al = _hvAlignmentsIn01;
    if (al.x==-1000)    al.x = (pos>=TOP) ? 0.5f : pos == RIGHT ? 1.0f : 0.f;
    if (al.y==-1000)    al.y = (pos<TOP) ? 0.0f : pos == BOTTOM ? 1.0f : 0.f;
    bar.setProperties(_keepAButtonSelected,_vertical,_lightAllBarWhenHovered,al,_opacityOffAndOn,_bg_col,_displayPortion);
}

void ImGui::PanelManager::Pane::setDisplayProperties(const ImVec2 &_opacityOffAndOn, const ImVec4 &_bg_col) {
    bar.setDisplayProperties(_opacityOffAndOn,_bg_col);
}

bool ImGui::PanelManager::Pane::isButtonPressed(int index) const   {
    const Toolbutton* button = bar.getButton(index);
    if (!button) return false;
    if (button->isToggleButton) return button->isDown;
    const int selectedIndex = bar.getSelectedButtonIndex();
    return (selectedIndex==index);
}

bool ImGui::PanelManager::Pane::setButtonPressed(int index, bool flag)   {
    Toolbutton* button = bar.getButton(index);
    if (!button) return false;
    AssociatedWindow& wnd = windows[index];
    if (!button->isToggleButton) {
        if (!bar.keepAButtonSelected) return false;
        if (bar.getSelectedButtonIndex()!=index) bar.setSelectedButtonIndex(index);
        if (wnd.isValid()) wnd.dirty = true;    // Must we include this in the "if" above ?
    }
    else {
        if (button->isDown != flag) {
            button->isDown = flag;
            //if (wnd.isValid()) {wnd.}   //  Must we do something here ?
        }
    }
    return true;
}

const char *ImGui::PanelManager::Pane::getWindowName(int index) const  {
    if (index<0 || index>=(int)windows.size()) return NULL;
    return windows[index].windowName;
}

int ImGui::PanelManager::Pane::findWindowIndex(const char *windowName) const    {
    if (!windowName) return -1;
    for (size_t i=0,isz=windows.size();i<isz;i++) {
        const char* wn = windows[i].windowName;
        if (strcmp(windowName,wn)==0) return (int)i;
    }
    return -1;
}

void ImGui::PanelManager::Pane::getButtonAndWindow(size_t index, ImGui::Toolbutton **pToolbutton, ImGui::PanelManager::Pane::AssociatedWindow **pAssociatedWindow)  // Returns references
{
    if (pToolbutton) *pToolbutton=bar.getButton(index);
    if (pAssociatedWindow)  {
        if ((int)index<windows.size()) *pAssociatedWindow=NULL;
        else *pAssociatedWindow=&windows[index];
    }
}

void ImGui::PanelManager::Pane::getButtonAndWindow(size_t index, const ImGui::Toolbutton **pToolbutton, const ImGui::PanelManager::Pane::AssociatedWindow **pAssociatedWindow) const // Returns references
{
    if (pToolbutton) *pToolbutton=bar.getButton(index);
    if (pAssociatedWindow)  {
        if ((int)index<windows.size()) *pAssociatedWindow=NULL;
        else *pAssociatedWindow=&windows[index];
    }
}

void ImGui::PanelManager::Pane::getButtonAndWindow(const char *windowName, ImGui::Toolbutton **pToolbutton, ImGui::PanelManager::Pane::AssociatedWindow **pAssociatedWindow)  // Returns references
{
    const int index = findWindowIndex(windowName);
    if (index==-1) {
        if (pToolbutton) *pToolbutton=NULL;
        if (pAssociatedWindow) *pAssociatedWindow=NULL;
    }
    else getButtonAndWindow((size_t)index,pToolbutton,pAssociatedWindow);
}

void ImGui::PanelManager::Pane::getButtonAndWindow(const char *windowName, const ImGui::Toolbutton **pToolbutton, const ImGui::PanelManager::Pane::AssociatedWindow **pAssociatedWindow) const // Returns references
{
    const int index = findWindowIndex(windowName);
    if (index==-1) {
        if (pToolbutton) *pToolbutton=NULL;
        if (pAssociatedWindow) *pAssociatedWindow=NULL;
    }
    else getButtonAndWindow((size_t)index,pToolbutton,pAssociatedWindow);
}


ImGui::PanelManager::Pane *ImGui::PanelManager::addPane(ImGui::PanelManager::Position pos, const char *toolbarName) {
    const size_t sz = panes.size();
    for (size_t i=0;i<sz;i++)   {
        IM_ASSERT(panes[i].pos!=pos);   // ONLY ONE PANE PER SIDE IS SUPPORTED
        if (panes[i].pos==pos) return NULL;
    }
    panes.push_back(Pane(pos,toolbarName));
    Pane& pane = panes[sz];
    switch (pos)    {
    case LEFT:      paneLeft = &pane;break;
    case RIGHT:     paneRight = &pane;break;
    case TOP:       paneTop = &pane;break;
    case BOTTOM:    paneBottom = &pane;break;
    default:
        IM_ASSERT(false);   //Unknown pos
        break;
    }
    pane.setToolbarProperties();
    return &pane;
}


bool ImGui::PanelManager::render(Pane** pPanePressedOut, int *pPaneToolbuttonPressedIndexOut) const   {
    if (!visible) return false;

    const ImVec2 oldInnerQuadPos = innerQuadPos;
    const ImVec2 oldInnerQuadSize = innerQuadSize;

    if (pPanePressedOut) *pPanePressedOut=NULL;
    if (pPaneToolbuttonPressedIndexOut) *pPaneToolbuttonPressedIndexOut=-1;

    // Hack to update sizes at the beginning:
    if (innerBarQuadSize.x==ImGui::GetIO().DisplaySize.x && innerBarQuadSize.y==ImGui::GetIO().DisplaySize.y) {
        if (!isEmpty()) {
            calculateInnerBarQuadPlacement();
            updateSizes();
        }
    }

    bool mustUpdateSizes = false;

    // Fill mustUpdateSizes:
    for (size_t paneIndex=0,panesSize=(size_t)panes.size();paneIndex<panesSize;paneIndex++) {
        const Pane& pane = panes[paneIndex];
        if (!pane.visible) continue;
        const Toolbar& bar = pane.bar;

        const int oldHoverButtonIndex = bar.getHoverButtonIndex();
        const int oldSelectedButtonIndex = bar.getSelectedButtonIndex();
        const int pressed = bar.render(true);   // "true" uses ImGui::IsItemHoveringRect()
        int selectedButtonIndex = bar.getSelectedButtonIndex();
        int& hoverButtonIndex = bar.hoverButtonIndex;

        bool selectedButtonIndexChanged = (oldSelectedButtonIndex!=selectedButtonIndex);
        mustUpdateSizes|=selectedButtonIndexChanged;

        bool hoverButtonIndexChanged = (oldHoverButtonIndex!=hoverButtonIndex);

        // new selected toolbuttons without valid windows should act like manual button (i.e. not take selection):
        if (selectedButtonIndexChanged && selectedButtonIndex>=0 && selectedButtonIndex<(int)bar.getNumButtons()) {
            const Pane::AssociatedWindow& wnd = pane.windows[selectedButtonIndex];
            if (!wnd.isValid()) {
                bar.selectedButtonIndex = selectedButtonIndex = oldSelectedButtonIndex;
                selectedButtonIndexChanged = false;
                if (selectedButtonIndex!=-1) updateSizes();
                //fprintf(stderr,"Pressed NULL window\n");
            }
        }

        // Optional hack: more comfortable hovering from button to window without easily losing focus
        if (hoverButtonIndexChanged && !selectedButtonIndexChanged && hoverButtonIndex==-1) {
            if (oldHoverButtonIndex<(int)bar.getNumButtons())    {
                const Pane::AssociatedWindow& window = pane.windows[oldHoverButtonIndex];
                if (window.isValid())
                {
                    if (!window.persistHoverFocus && pane.hoverReleaseIndex!=oldHoverButtonIndex) {
                        pane.hoverReleaseIndex=oldHoverButtonIndex;
                        pane.hoverReleaseTimer=ImGui::GetTime();
                        //fprintf(stderr,"Starting timer\n");

                        hoverButtonIndex = oldHoverButtonIndex;
                        hoverButtonIndexChanged = false;
                        window.persistHoverFocus = true;
                    }
                    else if (pane.hoverReleaseIndex == oldHoverButtonIndex)
                    {
                        if (ImGui::GetTime()-pane.hoverReleaseTimer<0.25f)   {
                            hoverButtonIndex = oldHoverButtonIndex;
                            hoverButtonIndexChanged = false;
                            window.persistHoverFocus = true;
                            //fprintf(stderr,"Keeping focus\n");
                        }
                        else {
                            pane.hoverReleaseIndex = -1;
                            pane.hoverReleaseTimer = -1.f;
                            //fprintf(stderr,"Hovering timer elapsed\n");
                        }
                    }
                }
            }
        }

        if (pressed>=0)    {
            // Fill Optional arguments
            if (pPanePressedOut) *pPanePressedOut=const_cast<Pane*> (&pane);
            if (pPaneToolbuttonPressedIndexOut) *pPaneToolbuttonPressedIndexOut=(int) pressed;
        }

        // hack (without this block when clicking to a selected toolbutton, the hover window does not enlarge its size (like it does when clicking on its titlebar button)
        if (selectedButtonIndexChanged && selectedButtonIndex==-1 && hoverButtonIndex>=0 && hoverButtonIndex==oldSelectedButtonIndex)   {
            const Toolbutton* button = bar.getButton((size_t)hoverButtonIndex);
            if (!button->isToggleButton)    {
                pane.windows[hoverButtonIndex].dirty = true;
            }
        }

        // Cross-pane window handling (optional):
        {
            if (selectedButtonIndexChanged && selectedButtonIndex>=0)   {
                const char* windowName = pane.windows[selectedButtonIndex].windowName;
                if (!windowName) continue;
                // We must turn off the newly selected windows from other panes if present:
                for (size_t pi=0,panesSize=(size_t)panes.size();pi<panesSize;pi++) {
                    if (pi==paneIndex) continue;    // this pane
                    const Pane& pn = panes[pi];
                    const int si = pn.getSelectedIndex();
                    if (si>=0)  {
                        const char* cmpName = pn.windows[si].windowName;
                        if (cmpName && strcmp(windowName,cmpName)==0)   pn.bar.setSelectedButtonIndex(-1);
                    }
                }

            }
            else if (hoverButtonIndexChanged && hoverButtonIndex>=0 && hoverButtonIndex!=selectedButtonIndex)    {
                const Toolbutton* button = bar.getButton((size_t)hoverButtonIndex);
                if (!button->isToggleButton)    {
                    const char* windowName = pane.windows[hoverButtonIndex].windowName;
                    // we must avoid hovering if "windowName" is selected on another pane
                    if (windowName) {
                        for (size_t pi=0,panesSize=(size_t)panes.size();pi<panesSize;pi++) {
                            if (pi==paneIndex) continue;    // this pane
                            const Pane& pn = panes[pi];
                            const int si = pn.getSelectedIndex();
                            if (si>=0)  {
                                const char* cmpName = pn.windows[si].windowName;
                                if (cmpName && strcmp(windowName,cmpName)==0)   pane.bar.hoverButtonIndex = -1;
                            }
                        }
                    }
                }
            }
        }

    }

    if (mustUpdateSizes) this->updateSizes();

    // Draw windows:
    for (size_t paneIndex=0,panesSize=(size_t)panes.size();paneIndex<panesSize;paneIndex++) {
        const Pane& pane = panes[paneIndex];
        if (!pane.visible) continue;
        for (size_t windowIndex=0,paneWindowsSize=(size_t)pane.windows.size();windowIndex<paneWindowsSize;windowIndex++)    {
            const Pane::AssociatedWindow& window = pane.windows[windowIndex];
            window.draw(*this,pane,windowIndex);
        }
    }


    const bool innerQuadChanged = (innerQuadPos.x!=oldInnerQuadPos.x || innerQuadPos.y!=oldInnerQuadPos.y ||
                                      innerQuadSize.x!=oldInnerQuadSize.x || innerQuadSize.y!=oldInnerQuadSize.y);
    if (innerQuadChanged)    {
        if (innerQuadChangedTimer<0) innerQuadChangedTimer = ImGui::GetTime();
    }
    else if (innerQuadChangedTimer>0 && ImGui::GetTime()-innerQuadChangedTimer>1.f)    {
        innerQuadChangedTimer = -1.f;
        return true;
    }

    return false;
}


size_t ImGui::PanelManager::getNumPanes() const {return panes.size();}

const ImGui::PanelManager::Pane *ImGui::PanelManager::getPane(ImGui::PanelManager::Position pos) const {
    for (size_t i=0,isz=panes.size();i<isz;i++) {
        if (panes[i].pos==pos) return &panes[i];
    }
    return NULL;
}

ImGui::PanelManager::Pane *ImGui::PanelManager::getPane(ImGui::PanelManager::Position pos)  {
    for (size_t i=0,isz=panes.size();i<isz;i++) {
        if (panes[i].pos==pos) return &panes[i];
    }
    return NULL;
}


void ImGui::PanelManager::setToolbarsScaling(float scalingX, float scalingY) {
    for (size_t i=0,isz=panes.size();i<isz;i++) panes[i].setToolbarScaling(scalingX,scalingY);
    calculateInnerBarQuadPlacement();
    updateSizes();

    // Set all windows "dirty" and clamp visible "toggle-windows" to the display size (although in the latter case "win.curSize" is not used at all [ImGui::GetWindowSize(name) does not exist ATM])
    for (size_t pi=0,pisz=panes.size();pi<pisz;pi++) {
        const Pane& pane = panes[pi];
        for (size_t i=0,isz=pane.windows.size();i<isz;i++) {
            const Pane::AssociatedWindow& win = pane.windows[i];
            if (win.isValid()) {
                win.dirty=true;

                const Toolbutton& but = pane.bar.buttons[i];
                if (but.isToggleButton) {
                    // simply clamp it
                    if (win.curPos.x+win.curSize.x>innerBarQuadPos.x+innerQuadSize.x)   win.curPos.x=innerBarQuadPos.x+innerQuadSize.x-win.curSize.x;
                    if (win.curPos.y+win.curSize.y>innerBarQuadPos.y+innerQuadSize.y)   win.curPos.y=innerBarQuadPos.y+innerQuadSize.y-win.curSize.y;
                    if (win.curPos.x<innerBarQuadPos.x)     win.curPos.x=innerBarQuadPos.x;
                    if (win.curPos.y<innerBarQuadPos.y)     win.curPos.y=innerBarQuadPos.y;
                }
            }
        }
    }
}

void ImGui::PanelManager::overrideAllExistingPanesDisplayProperties(const ImVec2 &_opacityOffAndOn, const ImVec4 &_bg_col)  {
    for (size_t pi=0,pisz=panes.size();pi<pisz;pi++) {
        Pane& pane = panes[pi];
        pane.setDisplayProperties(_opacityOffAndOn,_bg_col);
    }
}

void ImGui::PanelManager::updateSizes() const  {
    if (innerBarQuadSize.x<=0 || innerBarQuadSize.y<=0) calculateInnerBarQuadPlacement();
    //
    //
    innerQuadPos = innerBarQuadPos;
    innerQuadSize = innerBarQuadSize;
    const ImVec2& winMinSize = ImGui::GetStyle().WindowMinSize;

    static const float DefaultDockSizePortion = 0.25f;
    if (paneTop)    {
        const Pane& pane = *paneTop;
        const int selectedButtonIndex = pane.bar.getSelectedButtonIndex();
        //
        if (selectedButtonIndex>=0) {
            const Pane::AssociatedWindow& win = pane.windows[selectedButtonIndex];
            if (win.isValid())  {
                //
                win.curPos.x = innerBarQuadPos.x;
                win.curPos.y = innerBarQuadPos.y;
                win.curSize.x = innerBarQuadSize.x;
                if (win.size<0) win.size = ImGui::GetIO().DisplaySize.y * DefaultDockSizePortion;
                else if (win.size<winMinSize.y) win.size=winMinSize.y;
                if (win.size>innerBarQuadSize.y)  win.size = innerBarQuadSize.y;
                win.curSize.y = win.size;
                //
                innerQuadPos.y+=win.curSize.y;
                innerQuadSize.y-=win.curSize.y;
                //
            }
            win.dirty = false;
        }
    }
    if (paneLeft)    {
        const Pane& pane = *paneLeft;
        const int selectedButtonIndex = pane.bar.getSelectedButtonIndex();
        //
        if (selectedButtonIndex>=0) {
            const Pane::AssociatedWindow& win = pane.windows[selectedButtonIndex];
            if (win.isValid())  {
                //
                win.curPos.x = innerBarQuadPos.x;
                win.curPos.y = innerQuadPos.y;
                if (win.size<0) win.size = ImGui::GetIO().DisplaySize.x * DefaultDockSizePortion;
                else if (win.size<winMinSize.x) win.size=winMinSize.x;
                if (win.size>innerBarQuadSize.x)  win.size = innerBarQuadSize.x;
                win.curSize.x = win.size;
                win.curSize.y = innerQuadSize.y;

                //
                innerQuadPos.x+=win.curSize.x;
                innerQuadSize.x-=win.curSize.x;
                //
            }
            win.dirty = false;
        }
    }
    if (paneRight)    {
        const Pane& pane = *paneRight;
        const int selectedButtonIndex = pane.bar.getSelectedButtonIndex();
        //
        if (selectedButtonIndex>=0) {
            const Pane::AssociatedWindow& win = pane.windows[selectedButtonIndex];
            if (win.isValid())  {
                //
                win.curPos.y = innerQuadPos.y;
                win.curSize.y = innerQuadSize.y;
                if (win.size<0) win.size = ImGui::GetIO().DisplaySize.x * DefaultDockSizePortion;
                else if (win.size<winMinSize.x) win.size=winMinSize.x;
                if (win.size>innerQuadSize.x)  win.size = innerQuadSize.x;
                win.curSize.x = win.size;
                win.curPos.x = innerBarQuadPos.x+innerBarQuadSize.x-win.curSize.x;
                //
                innerQuadSize.x-=win.curSize.x;
                //
            }
            win.dirty = false;
        }
    }
    if (paneBottom)    {
        const Pane& pane = *paneBottom;
        const int selectedButtonIndex = pane.bar.getSelectedButtonIndex();
        //
        if (selectedButtonIndex>=0) {
            const Pane::AssociatedWindow& win = pane.windows[selectedButtonIndex];
            if (win.isValid())  {
                //
                win.curPos.x = innerQuadPos.x;
                win.curSize.x = innerQuadSize.x;
                if (win.size<0) win.size = ImGui::GetIO().DisplaySize.y * DefaultDockSizePortion;
                else if (win.size<winMinSize.y) win.size=winMinSize.y;
                if (win.size>innerQuadSize.y)  win.size = innerQuadSize.y;
                win.curSize.y = win.size;
                win.curPos.y = innerQuadPos.y+innerQuadSize.y-win.curSize.y;
                //
                innerQuadSize.y-=win.curSize.y;
                //
            }
            win.dirty = false;
        }
    }

    //fprintf(stderr,"innerBarQuad  (%1.1f,%1.1f,%1.1f,%1.1f)\n",innerBarQuadPos.x,innerBarQuadPos.y,innerBarQuadSize.x,innerBarQuadSize.y);
    //fprintf(stderr,"innerQuad     (%1.1f,%1.1f,%1.1f,%1.1f)\n",innerQuadPos.x,innerQuadPos.y,innerQuadSize.x,innerQuadSize.y);

}

void ImGui::PanelManager::closeHoverWindow() {
    for (int i=0,isz=getNumPanes();i<isz;i++)   {
        Pane& pane = panes[i];
        Toolbar& bar = pane.bar;
        for (int w=0,wsz=bar.buttons.size();w<wsz;w++)  {
            Pane::AssociatedWindow& window = pane.windows[w];
            if (w == bar.hoverButtonIndex)    {
                // The next two lines were outside the loop
                window.dirty = true;
                window.updateSizeInHoverMode(*this,pane,w);
		window.dirty = true;
                window.persistHoverFocus = false;
                bar.hoverButtonIndex=-1;
            }
        }
    }
}

const ImVec2& ImGui::PanelManager::getCentralQuadPosition() const   {
    return innerQuadPos;
}
const ImVec2& ImGui::PanelManager::getCentralQuadSize() const   {
    return innerQuadSize;
}

const ImVec2& ImGui::PanelManager::getToolbarCentralQuadPosition() const    {
    return innerBarQuadPos;
}
const ImVec2& ImGui::PanelManager::getToolbarCentralQuadSize() const    {
    return innerBarQuadSize;
}

void ImGui::PanelManager::setDisplayPortion(const ImVec4 &_displayPortion) {
    for (size_t i=0,isz=panes.size();i<isz;i++) {
        Pane& p = panes[i];
        p.bar.setDisplayPortion(_displayPortion);
    }
    innerBarQuadSize.x=innerBarQuadSize.y=0;    // forces update
    updateSizes();
}



void ImGui::PanelManager::calculateInnerBarQuadPlacement() const {
    // Calculates "innerBarQuadPos" and "innerBarQuadSize" (this changes only after a screen resize)
    innerBarQuadPos = ImVec2(0,0);
    innerBarQuadSize = ImGui::GetIO().DisplaySize;
    const float deltaPixels = 2;
    if (paneTop)    {
        const Pane& pane = *paneTop;
        const ImVec2& pos  = pane.bar.toolbarWindowPos;
        const ImVec2& size = pane.bar.toolbarWindowSize;
        innerBarQuadPos.y = pos.y + size.y - deltaPixels;
        innerBarQuadSize.y-=innerBarQuadPos.y;
    }
    if (paneBottom)    {
        const Pane& pane = *paneBottom;
        const ImVec2& size = pane.bar.toolbarWindowSize;
        innerBarQuadSize.y-=size.y;innerBarQuadSize.y+=deltaPixels;
    }
    if (paneLeft)    {
        const Pane& pane = *paneLeft;
        const ImVec2& pos  = pane.bar.toolbarWindowPos;
        const ImVec2& size = pane.bar.toolbarWindowSize;
        innerBarQuadPos.x = pos.x + size.x - deltaPixels;
        innerBarQuadSize.x-=innerBarQuadPos.x;
    }
    if (paneRight)    {
        const Pane& pane = *paneRight;
        const ImVec2& size = pane.bar.toolbarWindowSize;
        innerBarQuadSize.x-=size.x;innerBarQuadSize.x+=deltaPixels;
    }
    //fprintf(stderr,"innerBarQuad  (%1.1f,%1.1f,%1.1f,%1.1f)\n",innerBarQuadPos.x,innerBarQuadPos.y,innerBarQuadSize.x,innerBarQuadSize.y);
}



#if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
#ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
#include "../imguihelper/imguihelper.h"
bool ImGui::PanelManager::Save(const PanelManager &mgr, ImGuiHelper::Serializer& s) {
    if (!s.isValid()) return false;

    const ImVec2& displaySize = ImGui::GetIO().DisplaySize;
    IM_ASSERT(displaySize.x>=0 && displaySize.y>=0);

    int mwf = (int) mgr.getDockedWindowsExtraFlags();s.save(ImGui::FT_ENUM,&mwf,"managerExtraWindowFlags");
    int sz = mgr.panes.size();s.save(&sz,"numPanes");
    for (int ip=0,ipsz=mgr.panes.size();ip<ipsz;ip++)    {
        const ImGui::PanelManager::Pane& pane = mgr.panes[ip];
        int pos = (int)pane.pos;s.save(ImGui::FT_ENUM,&pos,"pos");
        s.save(&pane.bar.selectedButtonIndex,"selectedButtonIndex");
        IM_ASSERT(pane.bar.buttons.size()==pane.windows.size());
        sz = pane.bar.buttons.size();s.save(&sz,"numPaneButtons");
        for (int ib=0,ibsz=pane.bar.buttons.size();ib<ibsz;ib++)    {
            const ImGui::Toolbar::Button& button = pane.bar.buttons[ib];
            const ImGui::PanelManager::Pane::AssociatedWindow& window = pane.windows[ib];
            int isPresent = window.isValid() ? 1:0;s.save(&isPresent,"isWindowPresent");
            if (isPresent)  {
                float tmp = window.size<0?window.size:(window.size/(pos<ImGui::PanelManager::TOP?displaySize.x:displaySize.y));
                s.save(&tmp,"sizeFactor");
                tmp = window.sizeHoverMode<0?window.sizeHoverMode:(window.sizeHoverMode/(pos<ImGui::PanelManager::TOP?displaySize.x:displaySize.y));
                s.save(&tmp,"hoverModeSizeFactor");
                int wf = (int) window.extraWindowFlags;s.save(ImGui::FT_ENUM,&wf,"extraWindowFlags");
            }
            int down = (button.isDown && button.isToggleButton) ? 1 : 0;
            s.save(&down,"toggleButtonIsPressed");
        }
    }
    static const char* endText="End";s.saveTextLines(endText,"endPanelManager");
    return true;
}
#endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
#include "../imguihelper/imguihelper.h"
struct ImGuiPanelManagerParserStruct {
    ImVec2 displaySize;
    ImGui::PanelManager* mgr;
    ImGui::PanelManager::Pane* curPane;
    ImGui::PanelManager::Pane::AssociatedWindow* curWindow;
    int numPanes,curPaneNum;
    int numButtons,curButtonNum;
};
static bool ImGuiPanelManagerParser(ImGuiHelper::FieldType ft,int /*numArrayElements*/,void* pValue,const char* name,void* userPtr)    {

    ImGuiPanelManagerParserStruct& ps = *((ImGuiPanelManagerParserStruct*) userPtr);
    ImGui::PanelManager& mgr = *ps.mgr;

    switch (ft) {
    case ImGui::FT_FLOAT:   {
        if (strcmp(name,"sizeFactor")==0)			{
            if (ps.curWindow && ps.curPane) {
                ps.curWindow->size = *((float*)pValue);
                if(ps.curWindow->size>0) ps.curWindow->size*=(ps.curPane->pos<ImGui::PanelManager::TOP?ps.displaySize.x:ps.displaySize.y);
            }
            break;
        }
        else if (strcmp(name,"hoverModeSizeFactor")==0)	{
            if (ps.curWindow && ps.curPane) {
                ps.curWindow->sizeHoverMode = *((float*)pValue);
                if (ps.curWindow->sizeHoverMode>0) ps.curWindow->sizeHoverMode*=(ps.curPane->pos<ImGui::PanelManager::TOP?ps.displaySize.x:ps.displaySize.y);
            }
            break;
        }
    }
        break;
    case ImGui::FT_ENUM:    {
        if (strcmp(name,"pos")==0)	{
            ++ps.curPaneNum;ps.curPane = NULL;ps.curButtonNum=-1;ps.numButtons=0;ps.curWindow=NULL;
            if (ps.curPaneNum<ps.numPanes && ps.curPaneNum<(int)mgr.getNumPanes() && mgr.getPaneFromIndex(ps.curPaneNum)->pos==*((int*)pValue)) ps.curPane=mgr.getPaneFromIndex(ps.curPaneNum);
            //else fprintf(stderr,"Invalid pane\n");
            break;
        }
        else if (strcmp(name,"extraWindowFlags")==0)	{
            if (ps.curWindow) {
                ps.curWindow->extraWindowFlags=*((int*)pValue);
            }
            break;
        }
        else if (strcmp(name,"managerExtraWindowFlags")==0)	{
            ImGuiWindowFlags flags = *((int*)pValue);
            //mgr.dockedWindowsExtraFlags = flags;
            mgr.setDockedWindowsBorder(flags&ImGuiWindowFlags_ShowBorders);
            mgr.setDockedWindowsNoTitleBar(flags&ImGuiWindowFlags_NoTitleBar);
            break;
        }
    }
        break;
    case ImGui::FT_INT: {
        if (strcmp(name,"numPanes")==0)	{
            ps.numPanes=*((int*)pValue);ps.curPaneNum = -1;ps.curPane=NULL;
            ps.numButtons = 0;ps.curButtonNum=-1;ps.curWindow=NULL;
            break;
        }
        else if (strcmp(name,"selectedButtonIndex")==0)	{
            if (ps.curPane) {
                ps.curPane->bar.setSelectedButtonIndex(*((int*)pValue));
            }
            break;
        }
        else if (strcmp(name,"numPaneButtons")==0)	{
            ps.numButtons=*((int*)pValue);ps.curButtonNum=-1;ps.curWindow=NULL;
            if (ps.curPane && (int)ps.curPane->bar.getNumButtons()!=ps.numButtons)	{
                ps.curPane = NULL;  // Skip if number does not match
            }
            if (ps.curPane) {
                IM_ASSERT((int)ps.curPane->windows.size()==(int)ps.curPane->bar.getNumButtons());
            }
            break;
        }
        if (strcmp(name,"isWindowPresent")==0)		{
            ++ps.curButtonNum;
            if (ps.curPane && ps.curButtonNum<ps.numButtons && ps.curButtonNum<(int)ps.curPane->bar.getNumButtons())  ps.curWindow=&ps.curPane->windows[ps.curButtonNum];
            else {ps.curPane = NULL;ps.curWindow = NULL;}
            //fprintf(stderr,"pane:%d/%d == %s button:%d/%d window == %s\n",ps.curPaneNum,ps.numPanes,ps.curPane?"OK":"NULL",ps.curButtonNum,ps.numButtons,ps.curWindow? (ps.curWindow->windowName?ps.curWindow->windowName:"OK"):"NULL");
            //bool isWindowPresent = *((bool*)pValue) ? true : false;
            break;
        }
        else if (strcmp(name,"toggleButtonIsPressed")==0)	{
            bool down = *((int*)pValue) ? true : false;
            if (ps.curPane && ps.curButtonNum<ps.numButtons && (int)ps.curPane->bar.getNumButtons()>ps.curButtonNum)	{
                ImGui::Toolbar::Button& btn = *(ps.curPane->bar.getButton(ps.curButtonNum));
                if (btn.isToggleButton) btn.isDown = down;
            }
            break;
        }
    }
        break;
    case ImGui::FT_TEXTLINE:
        if (strcmp(name,"endPanelManager")==0) return true;
        break;
    default:
        break;
    }
    return false;
}
bool ImGui::PanelManager::Load(PanelManager &mgr,ImGuiHelper::Deserializer& d, const char ** pOptionalBufferStart)  {
    if (!d.isValid()) return false;
    ImGuiPanelManagerParserStruct pmps;
    pmps.mgr = &mgr;pmps.numButtons=pmps.curButtonNum=pmps.numPanes=pmps.curPaneNum=0;pmps.curPane=NULL;pmps.curWindow=NULL;
    pmps.displaySize = ImGui::GetIO().DisplaySize;
    IM_ASSERT(pmps.displaySize.x>=0 && pmps.displaySize.y>=0);

    const char* amount = pOptionalBufferStart ? (*pOptionalBufferStart) : 0;
    amount = d.parse(ImGuiPanelManagerParser,(void*)&pmps,amount);
    if (pOptionalBufferStart) *pOptionalBufferStart=amount;

    mgr.recalculatePositionAndSizes();
    mgr.closeHoverWindow();

    return true;
}
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#endif //NO_IMGUIHELPER_SERIALIZATION
