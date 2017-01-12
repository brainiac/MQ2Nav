#include "imguivariouscontrols.h"

#include <imgui_internal.h> // intellisense

#ifndef NO_IMGUIVARIOUSCONTROLS_ANIMATEDIMAGE
#ifndef IMGUI_USE_AUTO_BINDING
#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../imguibindings/stb_image.h"
#endif //STBI_INCLUDE_STB_IMAGE_H
#endif //IMGUI_USE_AUTO_BINDING
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
struct gif_result : stbi__gif {
    unsigned char *data;
    struct gif_result *next;
};
#ifdef __cplusplus
}
#endif //__cplusplus
//#define DEBUG_OUT_TEXTURE
#ifdef DEBUG_OUT_TEXTURE
#ifndef STBI_INCLUDE_STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "./addons/imguibindings/stb_image_write.h"
#endif //DEBUG_OUT_TEXTURE
#endif //STBI_INCLUDE_STB_IMAGE_WRITE_H
#endif //NO_IMGUIVARIOUSCONTROLS_ANIMATEDIMAGE


namespace ImGui {
static float GetWindowFontScale() {
    //ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    return window->FontWindowScale;
}


float ProgressBar(const char *optionalPrefixText, float value, const float minValue, const float maxValue, const char *format, const ImVec2 &sizeOfBarWithoutTextInPixels, const ImVec4 &colorLeft, const ImVec4 &colorRight, const ImVec4 &colorBorder)    {
    if (value<minValue) value=minValue;
    else if (value>maxValue) value = maxValue;
    const float valueFraction = (maxValue==minValue) ? 1.0f : ((value-minValue)/(maxValue-minValue));
    const bool needsPercConversion = strstr(format,"%%")!=NULL;

    ImVec2 size = sizeOfBarWithoutTextInPixels;
    if (size.x<=0) size.x = ImGui::GetWindowWidth()*0.25f;
    if (size.y<=0) size.y = ImGui::GetTextLineHeightWithSpacing(); // or without

    const ImFontAtlas* fontAtlas = ImGui::GetIO().Fonts;

    if (optionalPrefixText && strlen(optionalPrefixText)>0) {
        ImGui::AlignFirstTextHeightToWidgets();
        ImGui::Text("%s",optionalPrefixText);
        ImGui::SameLine();
    }

    if (valueFraction>0)   {
        ImGui::Image(fontAtlas->TexID,ImVec2(size.x*valueFraction,size.y), fontAtlas->TexUvWhitePixel,fontAtlas->TexUvWhitePixel,colorLeft,colorBorder);
    }
    if (valueFraction<1)   {
        if (valueFraction>0) ImGui::SameLine(0,0);
        ImGui::Image(fontAtlas->TexID,ImVec2(size.x*(1.f-valueFraction),size.y), fontAtlas->TexUvWhitePixel,fontAtlas->TexUvWhitePixel,colorRight,colorBorder);
    }
    ImGui::SameLine();

    ImGui::Text(format,needsPercConversion ? (valueFraction*100.f+0.0001f) : value);
    return valueFraction;
}

void TestProgressBar()  {    
    const float time = ((float)(((unsigned int) (ImGui::GetTime()*1000.f))%50000)-25000.f)/25000.f;
    float progress=(time>0?time:-time);
    // No IDs needed for ProgressBars:
    ImGui::ProgressBar("ProgressBar",progress);
    ImGui::ProgressBar("ProgressBar",1.f-progress);
    ImGui::ProgressBar("",500+progress*1000,500,1500,"%4.0f (absolute value in [500,1500] and fixed bar size)",ImVec2(150,-1));
    ImGui::ProgressBar("",500+progress*1000,500,1500,"%3.0f%% (same as above, but with percentage and new colors)",ImVec2(150,-1),ImVec4(0.7,0.7,1,1),ImVec4(0.05,0.15,0.5,0.8),ImVec4(0.8,0.8,0,1));
    // This one has just been added to ImGui:
    //char txt[48]="";sprintf(txt,"%3d%% (ImGui default progress bar)",(int)(progress*100));
    //ImGui::ProgressBar(progress,ImVec2(0,0),txt);
}


int PopupMenuSimple(bool &open, const char **pEntries, int numEntries, const char *optionalTitle, int *pOptionalHoveredEntryOut, int startIndex, int endIndex, bool reverseItems, const char *scrollUpEntryText, const char *scrollDownEntryText)   {
    int selectedEntry = -1;
    if (pOptionalHoveredEntryOut) *pOptionalHoveredEntryOut=-1;
    if (!open) return selectedEntry;
    if (numEntries==0 || !pEntries) {
        open = false;
        return selectedEntry;
    }

    float fs = 1.f;
#   ifdef IMGUI_INCLUDE_IMGUI_USER_INL
    fs = ImGui::GetWindowFontScale();   // Internal to <imgui.cpp>
#   endif //   IMGUI_INCLUDE_IMGUI_USER_INL

    if (!open) return selectedEntry;
    ImGui::PushID(&open);   // or pEntries ??
    //ImGui::BeginPopup(&open);
    ImGui::OpenPopup("MyOwnPopupSimpleMenu");
    if (ImGui::BeginPopup("MyOwnPopupSimpleMenu"))  {
        if (optionalTitle) {ImGui::Text("%s",optionalTitle);ImGui::Separator();}
        if (startIndex<0) startIndex=0;
        if (endIndex<0) endIndex = numEntries-1;
        if (endIndex>=numEntries) endIndex = numEntries-1;
        const bool needsScrolling = (endIndex-startIndex+1)<numEntries;
        if (scrollUpEntryText && needsScrolling) {
            ImGui::SetWindowFontScale(fs*0.75f);
            if (reverseItems ? (endIndex+1<numEntries) : (startIndex>0))    {
                const int entryIndex = reverseItems ? -3 : -2;
                if (ImGui::Selectable(scrollUpEntryText, false))  {
                    selectedEntry = entryIndex;//open = false;    // Hide menu
                }
                else if (pOptionalHoveredEntryOut && ImGui::IsItemHovered()) *pOptionalHoveredEntryOut = entryIndex;
            }
            else ImGui::Text(" ");
            ImGui::SetWindowFontScale(fs);
        }
        if (!reverseItems)  {
            for (int i = startIndex; i <= endIndex; i++)    {
                const char* entry = pEntries[i];
                if (!entry || strlen(entry)==0) ImGui::Separator();
                else {
                    if (ImGui::Selectable(entry, false))  {
                        selectedEntry = i;open = false;    // Hide menu
                    }
                    else if (pOptionalHoveredEntryOut && ImGui::IsItemHovered()) *pOptionalHoveredEntryOut = i;
                }
            }
        }
        else {
            for (int i = endIndex; i >= startIndex; i--)    {
                const char* entry = pEntries[i];
                if (!entry || strlen(entry)==0) ImGui::Separator();
                else {
                    if (ImGui::Selectable(entry, false))  {
                        selectedEntry = i;open = false;    // Hide menu
                    }
                    else if (pOptionalHoveredEntryOut && ImGui::IsItemHovered()) *pOptionalHoveredEntryOut = i;
                }

            }
        }
        if (scrollDownEntryText && needsScrolling) {
            const float fs = ImGui::GetWindowFontScale();      // Internal to <imgui.cpp>
            ImGui::SetWindowFontScale(fs*0.75f);
            if (reverseItems ? (startIndex>0) : (endIndex+1<numEntries))    {
                const int entryIndex = reverseItems ? -2 : -3;
                if (ImGui::Selectable(scrollDownEntryText, false))  {
                    selectedEntry = entryIndex;//open = false;    // Hide menu
                }
                else if (pOptionalHoveredEntryOut && ImGui::IsItemHovered()) *pOptionalHoveredEntryOut = entryIndex;
            }
            else ImGui::Text(" ");
            ImGui::SetWindowFontScale(fs);
        }
        if (open)   // close menu when mouse goes away
        {
            const float d = 10;
            ImVec2 pos = ImGui::GetWindowPos();pos.x-=d;pos.y-=d;
            ImVec2 size = ImGui::GetWindowSize();size.x+=2.f*d;size.y+=2.f*d;
            const ImVec2& mousePos = ImGui::GetIO().MousePos;
            if (mousePos.x<pos.x || mousePos.y<pos.y || mousePos.x>pos.x+size.x || mousePos.y>pos.y+size.y) open = false;
        }
    }
    ImGui::EndPopup();
    ImGui::PopID();

    return selectedEntry;    
}

int PopupMenuSimpleCopyCutPasteOnLastItem(bool readOnly) {
    static bool open = false;
    static const char* entries[] = {"Copy","Cut","","Paste"};   // "" is separator
    //open|=ImGui::Button("Show Popup Menu Simple");                    // BUTTON
    open|= ImGui::GetIO().MouseClicked[1] && ImGui::IsItemHovered(); // RIGHT CLICK
    int selectedEntry = PopupMenuSimple(open,entries,readOnly?1:4);
    if (selectedEntry>2) selectedEntry = 2; // Normally separator takes out one space
    return selectedEntry;
    // About "open": when user exits popup-menu, "open" becomes "false". Please set it to "true" to display it again (we do it using open|=[...])
}


int PopupMenuSimple(PopupMenuSimpleParams &params, const char **pTotalEntries, int numTotalEntries, int numAllowedEntries, bool reverseItems, const char *optionalTitle, const char *scrollUpEntryText, const char *scrollDownEntryText)    {
    if (numAllowedEntries<1 || numTotalEntries==0) {params.open=false;return -1;}
    if (params.endIndex==-1) params.endIndex=reverseItems ? numTotalEntries-1 : numAllowedEntries-1;
    if (params.startIndex==-1) params.startIndex=params.endIndex-numAllowedEntries+1;

    const int oldHoveredEntry = params.hoveredEntry;
    params.selectedEntry = PopupMenuSimple(params.open,pTotalEntries,numTotalEntries,optionalTitle,&params.hoveredEntry,params.startIndex,params.endIndex,reverseItems,scrollUpEntryText,scrollDownEntryText);

    if (params.hoveredEntry<=-2 || params.selectedEntry<=-2)   {
        if (oldHoveredEntry!=params.hoveredEntry) params.scrollTimer = ImGui::GetTime();
        const float newTime = ImGui::GetTime();
        if (params.selectedEntry<=-2 || (newTime - params.scrollTimer > 0.4f))    {
            params.scrollTimer = newTime;
            if (params.hoveredEntry==-2 || params.selectedEntry==-2)   {if (params.startIndex>0) {--params.startIndex;--params.endIndex;}}
            else if (params.hoveredEntry==-3 || params.selectedEntry==-3) {if (params.endIndex<numTotalEntries-1) {++params.startIndex;++params.endIndex;}}

        }
    }
    if (!params.open && params.resetScrollingWhenRestart) {
        params.endIndex=reverseItems ? numTotalEntries-1 : numAllowedEntries-1;
        params.startIndex=params.endIndex-numAllowedEntries+1;
    }
    return params.selectedEntry;
}

void TestPopupMenuSimple(const char *scrollUpEntryText, const char *scrollDownEntryText) {
    // Recent Files-like menu
    static const char* recentFileList[] = {"filename01","filename02","filename03","filename04","filename05","filename06","filename07","filename08","filename09","filename10"};

    static PopupMenuSimpleParams pmsParams;
    pmsParams.open|= ImGui::GetIO().MouseClicked[1];// RIGHT CLICK
    const int selectedEntry = PopupMenuSimple(pmsParams,recentFileList,(int) sizeof(recentFileList)/sizeof(recentFileList[0]),5,true,"RECENT FILES",scrollUpEntryText,scrollDownEntryText);
    if (selectedEntry>=0) {
        // Do something: clicked entries[selectedEntry]
    }
}

inline static void ClampColor(ImVec4& color)    {
    float* pf;
    pf = &color.x;if (*pf<0) *pf=0;if (*pf>1) *pf=1;
    pf = &color.y;if (*pf<0) *pf=0;if (*pf>1) *pf=1;
    pf = &color.z;if (*pf<0) *pf=0;if (*pf>1) *pf=1;
    pf = &color.w;if (*pf<0) *pf=0;if (*pf>1) *pf=1;
}

// Based on the code from: https://github.com/benoitjacquier/imgui
inline static bool ColorChooserInternal(ImVec4 *pColorOut,bool supportsAlpha,bool showSliders,ImGuiWindowFlags extra_flags=0,bool* pisAnyItemActive=NULL,float windowWidth = 180)    {
    bool colorSelected = false;
    if (pisAnyItemActive) *pisAnyItemActive=false;
    const bool isCombo = (extra_flags&ImGuiWindowFlags_ComboBox);

    ImVec4 color = pColorOut ? *pColorOut : ImVec4(0,0,0,1);
    if (!supportsAlpha) color.w=1.f;

    ImGuiContext& g = *GImGui;
    const float smallWidth = windowWidth/9.f;

    static const ImU32 black = ColorConvertFloat4ToU32(ImVec4(0,0,0,1));
    static const ImU32 white = ColorConvertFloat4ToU32(ImVec4(1,1,1,1));
    static float hue, sat, val;

    ImGui::ColorConvertRGBtoHSV( color.x, color.y, color.z, hue, sat, val );


    ImGuiWindow* colorWindow = GetCurrentWindow();

    const float quadSize = windowWidth - smallWidth - colorWindow->WindowPadding.x*2;
    if (isCombo) ImGui::SetCursorPosX(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x);
    // Hue Saturation Value
    if (ImGui::BeginChild("ValueSaturationQuad##ValueSaturationQuadColorChooser", ImVec2(quadSize, quadSize), false,extra_flags ))
    //ImGui::BeginGroup();
    {
        const int step = 5;
        ImVec2 pos = ImVec2(0, 0);
        ImGuiWindow* window = GetCurrentWindow();

        ImVec4 c00(1, 1, 1, 1);
        ImVec4 c10(1, 1, 1, 1);
        ImVec4 c01(1, 1, 1, 1);
        ImVec4 c11(1, 1, 1, 1);
        for (int y = 0; y < step; y++) {
            for (int x = 0; x < step; x++) {
                float s0 = (float)x / (float)step;
                float s1 = (float)(x + 1) / (float)step;
                float v0 = 1.0 - (float)(y) / (float)step;
                float v1 = 1.0 - (float)(y + 1) / (float)step;


                ImGui::ColorConvertHSVtoRGB(hue, s0, v0, c00.x, c00.y, c00.z);
                ImGui::ColorConvertHSVtoRGB(hue, s1, v0, c10.x, c10.y, c10.z);
                ImGui::ColorConvertHSVtoRGB(hue, s0, v1, c01.x, c01.y, c01.z);
                ImGui::ColorConvertHSVtoRGB(hue, s1, v1, c11.x, c11.y, c11.z);

                window->DrawList->AddRectFilledMultiColor(window->Pos + pos, window->Pos + pos + ImVec2(quadSize / step, quadSize / step),
                                                          ImGui::ColorConvertFloat4ToU32(c00),
                                                          ImGui::ColorConvertFloat4ToU32(c10),
                                                          ImGui::ColorConvertFloat4ToU32(c11),
                                                          ImGui::ColorConvertFloat4ToU32(c01));

                pos.x += quadSize / step;
            }
            pos.x = 0;
            pos.y += quadSize / step;
        }

        window->DrawList->AddCircle(window->Pos + ImVec2(sat, 1-val)*quadSize, 4, val<0.5f?white:black, 4);

        const ImGuiID id = window->GetID("ValueSaturationQuad");
        ImRect bb(window->Pos, window->Pos + window->Size);
        bool hovered, held;
        /*bool pressed = */ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_NoKeyModifiers);///*false,*/ false);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
        if (held)   {
            ImVec2 pos = g.IO.MousePos - window->Pos;
            sat = ImSaturate(pos.x / (float)quadSize);
            val = 1 - ImSaturate(pos.y / (float)quadSize);
            ImGui::ColorConvertHSVtoRGB(hue, sat, val, color.x, color.y, color.z);
            colorSelected = true;
        }

    }
    ImGui::EndChild();	// ValueSaturationQuad
    //ImGui::EndGroup();

    ImGui::SameLine();

    if (isCombo) ImGui::SetCursorPosX(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x+quadSize);

    //Vertical tint
    if (ImGui::BeginChild("Tint##TintColorChooser", ImVec2(smallWidth, quadSize), false,extra_flags))
    //ImGui::BeginGroup();
    {
        const int step = 8;
        const int width = (int)smallWidth;
        ImGuiWindow* window = GetCurrentWindow();
        ImVec2 pos(0, 0);
        ImVec4 c0(1, 1, 1, 1);
        ImVec4 c1(1, 1, 1, 1);
        for (int y = 0; y < step; y++) {
            float tint0 = (float)(y) / (float)step;
            float tint1 = (float)(y + 1) / (float)step;
            ImGui::ColorConvertHSVtoRGB(tint0, 1.0, 1.0, c0.x, c0.y, c0.z);
            ImGui::ColorConvertHSVtoRGB(tint1, 1.0, 1.0, c1.x, c1.y, c1.z);

            window->DrawList->AddRectFilledMultiColor(window->Pos + pos, window->Pos + pos + ImVec2(width, quadSize / step),
                                                      ColorConvertFloat4ToU32(c0),
                                                      ColorConvertFloat4ToU32(c0),
                                                      ColorConvertFloat4ToU32(c1),
                                                      ColorConvertFloat4ToU32(c1));

            pos.y += quadSize / step;
        }

        window->DrawList->AddCircle(window->Pos + ImVec2(smallWidth*0.5f, hue*quadSize), 4, black, 4);
        //window->DrawList->AddLine(window->Pos + ImVec2(0, hue*quadSize), window->Pos + ImVec2(width, hue*quadSize), ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 1)));
        bool hovered, held;
        const ImGuiID id = window->GetID("Tint");
        ImRect bb(window->Pos, window->Pos + window->Size);
        /*bool pressed = */ButtonBehavior(bb, id, &hovered, &held,ImGuiButtonFlags_NoKeyModifiers);// /*false,*/ false);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
        if (held)
        {

            ImVec2 pos = g.IO.MousePos - window->Pos;
            hue = ImClamp( pos.y / (float)quadSize, 0.0f, 1.0f );
            ImGui::ColorConvertHSVtoRGB( hue, sat, val, color.x, color.y, color.z );
            colorSelected = true;
        }

    }
    ImGui::EndChild(); // "Tint"
    //ImGui::EndGroup();

    if (showSliders)
    {
        //Sliders
        //ImGui::PushItemHeight();
        if (isCombo) ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x+colorWindow->WindowPadding.x,ImGui::GetCursorPos().y+colorWindow->WindowPadding.y+quadSize));
        ImGui::AlignFirstTextHeightToWidgets();
        ImGui::Text("Sliders");
        static bool useHsvSliders = false;
        static const char* btnNames[2] = {"to HSV","to RGB"};
        const int index = useHsvSliders?1:0;
        ImGui::SameLine();
        if (ImGui::SmallButton(btnNames[index])) useHsvSliders=!useHsvSliders;

        ImGui::Separator();
        const ImVec2 sliderSize = isCombo ? ImVec2(-1,quadSize) : ImVec2(-1,-1);
        if (ImGui::BeginChild("Sliders##SliderColorChooser", sliderSize, false,extra_flags))
        {


            {
                int r = ImSaturate( useHsvSliders ? hue : color.x )*255.f;
                int g = ImSaturate( useHsvSliders ? sat : color.y )*255.f;
                int b = ImSaturate( useHsvSliders ? val : color.z )*255.f;
                int a = ImSaturate( color.w )*255.f;

                static const char* names[2][3]={{"R","G","B"},{"H","S","V"}};
                bool sliderMoved = false;
                sliderMoved|= ImGui::SliderInt(names[index][0], &r, 0, 255);
                sliderMoved|= ImGui::SliderInt(names[index][1], &g, 0, 255);
                sliderMoved|= ImGui::SliderInt(names[index][2], &b, 0, 255);
                sliderMoved|= (supportsAlpha && ImGui::SliderInt("A", &a, 0, 255));
                if (sliderMoved)
                {
                    colorSelected = true;
                    color.x = (float)r/255.f;
                    color.y = (float)g/255.f;
                    color.z = (float)b/255.f;
                    if (useHsvSliders)  ImGui::ColorConvertHSVtoRGB(color.x,color.y,color.z,color.x,color.y,color.z);
                    if (supportsAlpha) color.w = (float)a/255.f;
                }
                //ColorConvertRGBtoHSV(s_color.x, s_color.y, s_color.z, tint, sat, val);*/
                if (pisAnyItemActive) *pisAnyItemActive|=sliderMoved;
            }


        }
        ImGui::EndChild();
    }

    if (colorSelected && pColorOut) *pColorOut = color;

    return colorSelected;
}

// Based on the code from: https://github.com/benoitjacquier/imgui
bool ColorChooser(bool* open,ImVec4 *pColorOut,bool supportsAlpha)   {
    static bool lastOpen = false;
    static const ImVec2 windowSize(175,285);

    if (open && !*open) {lastOpen=false;return false;}
    if (open && *open && *open!=lastOpen) {
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
        ImGui::SetNextWindowSize(windowSize);
        lastOpen=*open;
    }

    //ImGui::OpenPopup("Color Chooser##myColorChoserPrivate");

    bool colorSelected = false;

    ImGuiWindowFlags WindowFlags = 0;
    //WindowFlags |= ImGuiWindowFlags_NoTitleBar;
    WindowFlags |= ImGuiWindowFlags_NoResize;
    //WindowFlags |= ImGuiWindowFlags_NoMove;
    WindowFlags |= ImGuiWindowFlags_NoScrollbar;
    WindowFlags |= ImGuiWindowFlags_NoCollapse;
    WindowFlags |= ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,2));

    if (open) ImGui::SetNextWindowFocus();
    //if (ImGui::BeginPopupModal("Color Chooser##myColorChoserPrivate",open,WindowFlags))
    if (ImGui::Begin("Color Chooser##myColorChoserPrivate",open,windowSize,-1.f,WindowFlags))
    {
        colorSelected = ColorChooserInternal(pColorOut,supportsAlpha,true);

        //ImGui::EndPopup();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    return colorSelected;

}

// Based on the code from: https://github.com/benoitjacquier/imgui
bool ColorCombo(const char* label,ImVec4 *pColorOut,bool supportsAlpha,float width,bool closeWhenMouseLeavesIt)    {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float itemWidth = width>=0 ? width : ImGui::CalcItemWidth();
    const ImVec2 label_size = ImGui::CalcTextSize(label);
    const float color_quad_size = (g.FontSize + style.FramePadding.x);
    const float arrow_size = (g.FontSize + style.FramePadding.x * 2.0f);
    ImVec2 totalSize = ImVec2(label_size.x+color_quad_size+arrow_size, label_size.y) + style.FramePadding*2.0f;
    if (totalSize.x < itemWidth) totalSize.x = itemWidth;
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + totalSize);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max);// + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, &id)) return false;
    const float windowWidth = frame_bb.Max.x - frame_bb.Min.x - style.FramePadding.x;


    ImVec4 color = pColorOut ? *pColorOut : ImVec4(0,0,0,1);
    if (!supportsAlpha) color.w=1.f;

    const bool hovered = IsHovered(frame_bb, id);

    const ImRect value_bb(frame_bb.Min, frame_bb.Max - ImVec2(arrow_size, 0.0f));
    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    RenderFrame(frame_bb.Min,ImVec2(frame_bb.Min.x+color_quad_size,frame_bb.Max.y), ImColor(style.Colors[ImGuiCol_Text]), true, style.FrameRounding);
    RenderFrame(ImVec2(frame_bb.Min.x+1,frame_bb.Min.y+1), ImVec2(frame_bb.Min.x+color_quad_size-1,frame_bb.Max.y-1), ImColor(color), true, style.FrameRounding);

    RenderFrame(ImVec2(frame_bb.Max.x-arrow_size, frame_bb.Min.y), frame_bb.Max, GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), true, style.FrameRounding); // FIXME-ROUNDING
    RenderCollapseTriangle(ImVec2(frame_bb.Max.x-arrow_size, frame_bb.Min.y) + style.FramePadding, true);

    RenderTextClipped(ImVec2(frame_bb.Min.x+color_quad_size,frame_bb.Min.y) + style.FramePadding, value_bb.Max, label, NULL, NULL);

    if (hovered)
    {
        SetHoveredID(id);
        if (g.IO.MouseClicked[0])
        {
            SetActiveID(0);
            if (IsPopupOpen(id))
            {
                ClosePopup(id);
            }
            else
            {
                FocusWindow(window);
                ImGui::OpenPopup(label);
            }
        }
        static ImVec4 copiedColor(1,1,1,1);
        static const ImVec4* pCopiedColor = NULL;
        if (g.IO.MouseClicked[1]) { // right-click (copy color)
            copiedColor = color;
            pCopiedColor = &copiedColor;
            //fprintf(stderr,"Copied\n");
        }
        else if (g.IO.MouseClicked[2] && pCopiedColor && pColorOut) { // middle-click (paste color)
            pColorOut->x = pCopiedColor->x;
            pColorOut->y = pCopiedColor->y;
            pColorOut->z = pCopiedColor->z;
            if (supportsAlpha) pColorOut->w = pCopiedColor->w;
            color = *pColorOut;
            //fprintf(stderr,"Pasted\n");
        }
    }

    bool value_changed = false;
    if (IsPopupOpen(id))
    {
        ImRect popup_rect(ImVec2(frame_bb.Min.x, frame_bb.Max.y), ImVec2(frame_bb.Max.x, frame_bb.Max.y));
        //popup_rect.Max.y = ImMin(popup_rect.Max.y, g.IO.DisplaySize.y - style.DisplaySafeAreaPadding.y); // Adhoc height limit for Combo. Ideally should be handled in Begin() along with other popups size, we want to have the possibility of moving the popup above as well.
        ImGui::SetNextWindowPos(popup_rect.Min);
        ImGui::SetNextWindowSize(ImVec2(windowWidth,-1));//popup_rect.GetSize());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,2));

        bool mustCloseCombo = false;
        const ImGuiWindowFlags flags =  ImGuiWindowFlags_ComboBox;
        if (BeginPopupEx(label, flags))
        {
            bool comboItemActive = false;
            value_changed = ColorChooserInternal(pColorOut,supportsAlpha,false,flags,&comboItemActive,windowWidth);
            if (closeWhenMouseLeavesIt && !comboItemActive)
            {
                const float distance = g.FontSize*1.75f;//1.3334f;//24;
                //fprintf(stderr,"%1.f",distance);
                ImVec2 pos = ImGui::GetWindowPos();pos.x-=distance;pos.y-=distance;
                ImVec2 size = ImGui::GetWindowSize();
                size.x+=2.f*distance;
                size.y+=2.f*distance+windowWidth*8.f/9.f;   // problem: is seems that ImGuiWindowFlags_ComboBox does not return the full window height
                const ImVec2& mousePos = ImGui::GetIO().MousePos;
                if (mousePos.x<pos.x || mousePos.y<pos.y || mousePos.x>pos.x+size.x || mousePos.y>pos.y+size.y) {
                    mustCloseCombo = true;
                    //fprintf(stderr,"Leaving ColorCombo: pos(%1f,%1f) size(%1f,%1f)\n",pos.x,pos.y,size.x,size.y);
                }
            }
            ImGui::EndPopup();
        }
        if (mustCloseCombo && IsPopupOpen(id)) ClosePopup(id);
        ImGui::PopStyleVar(3);
    }
    return value_changed;
}


// Based on the code from: https://github.com/Roflraging (see https://github.com/ocornut/imgui/issues/383)
struct MultilineScrollState {
    // Input.
    float scrollRegionX;
    float scrollX;
    ImGuiStorage *storage;
    const char* textToPasteInto;
    int actionToPerformCopyCutSelectAllFrom1To3;

    // Output.
    bool newScrollPositionAvailable;
    float newScrollX;
    int CursorPos;
    int SelectionStart; //                                      // Read (== to SelectionEnd when no selection)
    int SelectionEnd;   //                                      // Read
};
// Based on the code from: https://github.com/Roflraging (see https://github.com/ocornut/imgui/issues/383)
static int MultilineScrollCallback(ImGuiTextEditCallbackData *data) {
    //static int cnt=0;fprintf(stderr,"MultilineScrollCallback (%d)\n",++cnt);
    MultilineScrollState *scrollState = (MultilineScrollState *)data->UserData;

    ImGuiID cursorId = ImGui::GetID("cursor");
    int oldCursorIndex = scrollState->storage->GetInt(cursorId, 0);

    if (oldCursorIndex != data->CursorPos)  {
        int begin = data->CursorPos;

        while ((begin > 0) && (data->Buf[begin - 1] != '\n'))   {
            --begin;
        }

        float cursorOffset = ImGui::CalcTextSize(data->Buf + begin, data->Buf + data->CursorPos).x;
        float SCROLL_INCREMENT = scrollState->scrollRegionX * 0.25f;

        if (cursorOffset < scrollState->scrollX)    {
            scrollState->newScrollPositionAvailable = true;
            scrollState->newScrollX = cursorOffset - SCROLL_INCREMENT; if (scrollState->newScrollX<0) scrollState->newScrollX=0;
        }
        else if ((cursorOffset - scrollState->scrollRegionX) >= scrollState->scrollX)   {
            scrollState->newScrollPositionAvailable = true;
            scrollState->newScrollX = cursorOffset - scrollState->scrollRegionX + SCROLL_INCREMENT;
        }
    }

    scrollState->storage->SetInt(cursorId, data->CursorPos);

    scrollState->CursorPos = data->CursorPos;
    if (data->SelectionStart<=data->SelectionEnd) {scrollState->SelectionStart = data->SelectionStart;scrollState->SelectionEnd = data->SelectionEnd;}
    else {scrollState->SelectionStart = data->SelectionEnd;scrollState->SelectionEnd = data->SelectionStart;}

    return 0;
}
// Based on the code from: https://github.com/Roflraging (see https://github.com/ocornut/imgui/issues/383)
bool InputTextMultilineWithHorizontalScrolling(const char* label, char* buf, size_t buf_size, float height, ImGuiInputTextFlags flags, bool* pOptionalIsHoveredOut, int *pOptionalCursorPosOut, int *pOptionalSelectionStartOut, int *pOptionalSelectionEndOut,float SCROLL_WIDTH)  {
    float scrollbarSize = ImGui::GetStyle().ScrollbarSize;
    //float labelWidth = ImGui::CalcTextSize(label).x + scrollbarSize;
    MultilineScrollState scrollState = {};

    // Set up child region for horizontal scrolling of the text box.
    ImGui::BeginChild(label, ImVec2(0/*-labelWidth*/, height), false, ImGuiWindowFlags_HorizontalScrollbar);
    scrollState.scrollRegionX = ImGui::GetWindowWidth() - scrollbarSize; if (scrollState.scrollRegionX<0) scrollState.scrollRegionX = 0;
    scrollState.scrollX = ImGui::GetScrollX();
    scrollState.storage = ImGui::GetStateStorage();
    bool changed = ImGui::InputTextMultiline(label, buf, buf_size, ImVec2(SCROLL_WIDTH-scrollbarSize, (height - scrollbarSize)>0?(height - scrollbarSize):0),
                                             flags | ImGuiInputTextFlags_CallbackAlways, MultilineScrollCallback, &scrollState);
    if (pOptionalIsHoveredOut) *pOptionalIsHoveredOut = ImGui::IsItemHovered();

    if (scrollState.newScrollPositionAvailable) {
        ImGui::SetScrollX(scrollState.newScrollX);
    }

    ImGui::EndChild();
    //ImGui::SameLine();
    //ImGui::Text("%s",label);

    if (pOptionalCursorPosOut) *pOptionalCursorPosOut = scrollState.CursorPos;
    if (pOptionalSelectionStartOut) *pOptionalSelectionStartOut = scrollState.SelectionStart;
    if (pOptionalSelectionEndOut)   *pOptionalSelectionEndOut = scrollState.SelectionEnd;

    return changed;
}

// Based on the code from: https://github.com/Roflraging (see https://github.com/ocornut/imgui/issues/383)
bool InputTextMultilineWithHorizontalScrollingAndCopyCutPasteMenu(const char *label, char *buf, int buf_size, float height,bool& staticBoolVar,int *staticArrayOfThreeIntegersHere, ImGuiInputTextFlags flags, bool *pOptionalHoveredOut,float SCROLL_WIDTH, const char *copyName, const char *cutName, const char* pasteName)   {
    bool isHovered=false;
    int& cursorPos=staticArrayOfThreeIntegersHere[0];
    int& selectionStart=staticArrayOfThreeIntegersHere[1];
    int& selectionEnd=staticArrayOfThreeIntegersHere[2];
    bool& popup_open = staticBoolVar;
    const bool changed = InputTextMultilineWithHorizontalScrolling(label,buf,(size_t)buf_size,height,flags,&isHovered,popup_open ? NULL : &cursorPos,popup_open ? NULL : &selectionStart,popup_open ? NULL : &selectionEnd,SCROLL_WIDTH);
    if (pOptionalHoveredOut) *pOptionalHoveredOut=isHovered;
    // Popup Menu ------------------------------------------

    const bool readOnly = flags&ImGuiInputTextFlags_ReadOnly;       // "Cut","","Paste" not available
    const bool hasSelectedText = selectionStart != selectionEnd;	// "Copy","Cut" available

    if (hasSelectedText || !readOnly)	{
        const bool onlyPaste = !readOnly && !hasSelectedText;
        const char* clipboardText = ImGui::GetIO().GetClipboardTextFn(NULL);
        const bool canPaste = clipboardText && strlen(clipboardText)>0;
        if (onlyPaste && !canPaste) popup_open = false;
        else {
            static const char* entries[] = {"Copy","Cut","","Paste"};   // "" is separator
            const char* pEntries[4]={copyName?copyName:entries[0],cutName?cutName:entries[1],entries[2],pasteName?pasteName:entries[3]};
            popup_open|= ImGui::GetIO().MouseClicked[1] && isHovered; // RIGHT CLICK
            int sel = ImGui::PopupMenuSimple(popup_open,onlyPaste ? &pEntries[3] : pEntries,(readOnly||onlyPaste)?1:canPaste? 4:2);
            if (sel==3) sel = 2; // Normally separator takes out one space
            const bool mustCopy = sel==0 && !onlyPaste;
            const bool mustCut = !mustCopy && sel==1;
            const bool mustPaste = !mustCopy && !mustCut && (sel==2 || (sel==0 && onlyPaste));
            if (mustCopy || mustCut || (mustPaste && (selectionStart<selectionEnd))) {
                // Copy to clipboard
                if (!mustPaste)	{
                    const char tmp = buf[selectionEnd];buf[selectionEnd]='\0';
                    ImGui::GetIO().SetClipboardTextFn(NULL,&buf[selectionStart]);
                    buf[selectionEnd]=tmp;
                }
                // Delete chars
                if (!mustCopy) {
                    //if (mustPaste) {fprintf(stderr,"Deleting before pasting: %d  %d.\n",selectionStart,selectionEnd);}

		    //strncpy(&buf[selectionStart],&buf[selectionEnd],buf_size-selectionEnd);				// Valgrind complains here, but I KNOW that source and destination overlap: I just want to shift chars to the left!
		    for (int i=0,isz=buf_size-selectionEnd;i<isz;i++) buf[i+selectionStart]=buf[i+selectionEnd];// I do it manually, so Valgrind is happy

		    for (int i=selectionStart+buf_size-selectionEnd;i<buf_size;i++) buf[i]='\0';		// This is mandatory at the end
                }
                popup_open = false;
            }
            if (mustPaste)  {
                // This is VERY HARD to make it work as expected...
                const int cursorPosition = (selectionStart<selectionEnd) ? selectionStart : cursorPos;
                const int clipboardTextSize = strlen(clipboardText);
                int buf_len = strlen(buf);if (buf_len>buf_size) buf_len=buf_size;

                // Step 1- Shift [cursorPosition] to [cursorPosition+clipboardTextSize]
                const int numCharsToShiftRight = buf_len - cursorPosition;
                //fprintf(stderr,"Pasting: \"%s\"(%d) at %d. buf_len=%d buf_size=%d numCharsToShiftRight=%d\n",clipboardText,clipboardTextSize,cursorPosition,buf_len,buf_size,numCharsToShiftRight);

                for (int i=cursorPosition+numCharsToShiftRight>buf_size?buf_size-1:cursorPosition+numCharsToShiftRight-1;i>=cursorPosition;i--) {
                    if (i+clipboardTextSize<buf_size) {
                        //fprintf(stderr,"moving to the right char (%d): '%c' (%d)\n",i,buf[i],(int)buf[i]);
                        buf[i+clipboardTextSize] = buf[i];
                    }
                }
                // Step 2- Overwrite [cursorPosition] o [cursorPosition+clipboardTextSize]
                for (int i=cursorPosition,isz=cursorPosition+clipboardTextSize>=buf_size?buf_size:cursorPosition+clipboardTextSize;i<isz;i++) buf[i]=clipboardText[i-cursorPosition];

                popup_open = false;
            }
        }	 
    }
    else popup_open = false;
    //------------------------------------------------------------------
    return changed;
}


bool ImageButtonWithText(ImTextureID texId,const char* label,const ImVec2& imageSize, const ImVec2 &uv0, const ImVec2 &uv1, int frame_padding, const ImVec4 &bg_col, const ImVec4 &tint_col) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
    return false;

    ImVec2 size = imageSize;
    if (size.x<=0 && size.y<=0) {size.x=size.y=ImGui::GetTextLineHeightWithSpacing();}
    else {
        if (size.x<=0)          size.x=size.y;
        else if (size.y<=0)     size.y=size.x;
        size*=window->FontWindowScale*ImGui::GetIO().FontGlobalScale;
    }

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImGuiID id = window->GetID(label);
    const ImVec2 textSize = ImGui::CalcTextSize(label,NULL,true);
    const bool hasText = textSize.x>0;

    const float innerSpacing = hasText ? ((frame_padding >= 0) ? (float)frame_padding : (style.ItemInnerSpacing.x)) : 0.f;
    const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : style.FramePadding;
    const ImVec2 totalSizeWithoutPadding(size.x+innerSpacing+textSize.x,size.y>textSize.y ? size.y : textSize.y);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + totalSizeWithoutPadding + padding*2);
    ImVec2 start(0,0);
    start = window->DC.CursorPos + padding;if (size.y<textSize.y) start.y+=(textSize.y-size.y)*.5f;
    const ImRect image_bb(start, start + size);
    start = window->DC.CursorPos + padding;start.x+=size.x+innerSpacing;if (size.y>textSize.y) start.y+=(size.y-textSize.y)*.5f;
    ItemSize(bb);
    if (!ItemAdd(bb, &id))
    return false;

    bool hovered=false, held=false;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Render
    const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, style.FrameRounding));
    if (bg_col.w > 0.0f)
    window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));

    window->DrawList->AddImage(texId, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

    if (textSize.x>0) ImGui::RenderText(start,label);
    return pressed;
}

#ifndef NO_IMGUIVARIOUSCONTROLS_ANIMATEDIMAGE
struct AnimatedImageInternal {
    protected:

    int w,h,frames;
    ImVector<unsigned char> buffer;
    ImVector<float> delays;
    ImTextureID persistentTexId;              // This will be used when all frames can fit into a single texture (very good for performance and memory)
    int numFramesPerRowInPersistentTexture,numFramesPerColInPersistentTexture;
    bool hoverModeIfSupported;
    bool persistentTexIdIsNotOwned;
    mutable bool isAtLeastOneWidgetInHoverMode;  // internal

    mutable  int lastFrameNum;
    mutable float delay;
    mutable float timer;
    mutable ImTextureID texId;
    mutable ImVec2 uvFrame0,uvFrame1;   // used by persistentTexId
    mutable int lastImGuiFrameUpdate;

    inline void updateTexture() const   {
        // fix updateTexture() to use persistentTexID when necessary
        IM_ASSERT(AnimatedImage::GenerateOrUpdateTextureCb!=NULL);	// Please use ImGui::AnimatedGif::SetGenerateOrUpdateTextureCallback(...) before calling this method
        if (frames<=0) return;
        else if (frames==1) {
            if (!texId) AnimatedImage::GenerateOrUpdateTextureCb(texId,w,h,4,&buffer[0],false,false,false);
            return;
        }

        // These two lines sync animation in multiple items:
        if (texId && lastImGuiFrameUpdate==ImGui::GetFrameCount()) return;
        lastImGuiFrameUpdate=ImGui::GetFrameCount();
        if (hoverModeIfSupported && !isAtLeastOneWidgetInHoverMode) {
            // reset animation here:
            timer=-1.f;lastFrameNum=-1;delay=0;
            //calculateTexCoordsForFrame(0,uvFrame0,uvFrame1);
        }
        isAtLeastOneWidgetInHoverMode = false;

        float lastDelay = delay;
        if (timer>0) {
            delay = ImGui::GetTime()*100.f-timer;
            if (delay<0) timer = -1.f;
        }
        if (timer<0) {timer = ImGui::GetTime()*100.f;delay=0.f;}

        const int imageSz = 4 * w * h;
        IM_ASSERT(sizeof(unsigned short)==2*sizeof(unsigned char));
        bool changed = false;
        float frameTime=0.f;
        bool forceUpdate = false;
        if (lastFrameNum<0) {forceUpdate=true;lastFrameNum=0;}
        for (int i=lastFrameNum;i<frames;i++)   {
            frameTime = delays[i];
            //fprintf(stderr,"%d/%d) %1.2f\n",i,frames,frameTime);
            if (delay <= lastDelay+frameTime) {
                changed = (i!=lastFrameNum || !texId);
                lastFrameNum = i;
                if (changed || forceUpdate)    {
                    if (!persistentTexId) AnimatedImage::GenerateOrUpdateTextureCb(texId,w,h,4,&buffer[imageSz*i],false,false,false);
                    else {
                        texId = persistentTexId;
                        // calculate uvFrame0 and uvFrame1 here based on 'i' and numFramesPerRowInPersistentTexture,numFramesPerColInPersistentTexture
                        calculateTexCoordsForFrame(i,uvFrame0,uvFrame1);
                    }
                }
                //fprintf(stderr,"%d/%d) %1.2f %1.2f %1.2f\n",i,frames,frameTime,delay,lastDelay);
                delay = lastDelay;
                return;
            }
            lastDelay+=frameTime;
            if (i==frames-1) i=-1;
        }

    }

    public:
    AnimatedImageInternal()  {persistentTexIdIsNotOwned=false;texId=persistentTexId=NULL;clear();}
    ~AnimatedImageInternal()  {texId=persistentTexId=NULL;clear();persistentTexIdIsNotOwned=false;}
    AnimatedImageInternal(char const *filename,bool useHoverModeIfSupported=false)  {persistentTexIdIsNotOwned = false;texId=persistentTexId=NULL;load(filename,useHoverModeIfSupported);}
    AnimatedImageInternal(ImTextureID myTexId,int animationImageWidth,int animationImageHeight,int numFrames,int numFramesPerRowInTexture,int numFramesPerColumnInTexture,float delayDetweenFramesInCs,bool useHoverMode=false) {
        persistentTexIdIsNotOwned = false;texId=persistentTexId=NULL;
        create(myTexId,animationImageWidth,animationImageHeight,numFrames,numFramesPerRowInTexture,numFramesPerColumnInTexture,delayDetweenFramesInCs,useHoverMode);
    }
    void clear() {
        w=h=frames=lastFrameNum=0;delay=0.f;timer=-1.f;buffer.clear();delays.clear();
        numFramesPerRowInPersistentTexture = numFramesPerColInPersistentTexture = 0;
        uvFrame0.x=uvFrame0.y=0;uvFrame1.x=uvFrame1.y=1;
        lastImGuiFrameUpdate = -1;hoverModeIfSupported=isAtLeastOneWidgetInHoverMode = false;
        if (texId || persistentTexId) IM_ASSERT(AnimatedImage::FreeTextureCb!=NULL);   // Please use ImGui::AnimatedGif::SetFreeTextureCallback(...)
        if (texId) {if (texId!=persistentTexId) AnimatedImage::FreeTextureCb(texId);texId=NULL;}
        if (persistentTexId)  {if (!persistentTexIdIsNotOwned) AnimatedImage::FreeTextureCb(persistentTexId);persistentTexId=NULL;}
    }

    bool load(char const *filename,bool useHoverModeIfSupported=false)  {
        ImGui::AnimatedImageInternal& ag = *this;

        // Code based on:
        // https://gist.github.com/urraka/685d9a6340b26b830d49

        FILE *f;
        stbi__context s;
        ag.clear();
        ag.persistentTexIdIsNotOwned = false;
        bool ok = false;

        if (!(f = stbi__fopen(filename, "rb"))) {
            stbi__errpuc("can't fopen", "Unable to open file");
            return false;
        }

        stbi__start_file(&s, f);

        if (stbi__gif_test(&s))
        {
            ok =true;
            int c;
            stbi__gif g;
            gif_result head;
            gif_result *prev = 0, *gr = &head;

            memset(&g, 0, sizeof(g));
            memset(&head, 0, sizeof(head));

            ag.frames = 0;

            while ((gr->data = stbi__gif_load_next(&s, &g, &c, 4)))
            {
		if (gr->data == (unsigned char*)&s)
                {
                    gr->data = 0;
                    break;
                }

                if (prev) prev->next = gr;
                gr->delay = g.delay;
                prev = gr;
                gr = (gif_result*) stbi__malloc(sizeof(gif_result));
                memset(gr, 0, sizeof(gif_result));
                ++ag.frames;
            }

	    STBI_FREE(g.out);
            if (gr != &head)    {
                STBI_FREE(gr);
            }

            if (ag.frames > 0)
            {
                ag.w = g.w;
                ag.h = g.h;
            }

            if (ag.frames==1) {
                ag.buffer.resize(ag.w*ag.h*4);
                memcpy(&ag.buffer[0],head.data,ag.buffer.size());
                STBI_FREE(head.data);
            }


            if (ag.frames > 1)
            {
                unsigned int size = 4 * g.w * g.h;
                unsigned char *p = 0;
                float *pd = 0;

                ag.buffer.resize(ag.frames * size);//(size + 2));
                ag.delays.resize(ag.frames);
                gr = &head;
                p = &ag.buffer[0];
                pd = &ag.delays[0];

                IM_ASSERT(sizeof(unsigned short)==2*sizeof(unsigned char));	// Not sure that this is necessary
                unsigned short tmp = 0;
                unsigned char* pTmp = (unsigned char*) &tmp;
                while (gr)
                {
                    prev = gr;
                    memcpy(p, gr->data, size);
                    p += size;
                    tmp = 0;
                    // We should invert these two lines for big-endian machines:
                    pTmp[0] = gr->delay & 0xFF;
                    pTmp[1] = (gr->delay & 0xFF00) >> 8;
                    *pd++ = (float) tmp;
                    gr = gr->next;

                    STBI_FREE(prev->data);
                    if (prev != &head) STBI_FREE(prev);
                }

                if (AnimatedImage::MaxPersistentTextureSize.x>0 && AnimatedImage::MaxPersistentTextureSize.y>0)	{
                    // code path that checks 'MaxPersistentTextureSize' and puts all into a single texture (rearranging the buffer)
                    ImVec2 textureSize = AnimatedImage::MaxPersistentTextureSize;
                    int maxNumFramesPerRow = (int)textureSize.x/(int)ag.w;
                    int maxNumFramesPerCol = (int)textureSize.y/(int)ag.h;
                    int maxNumFramesInATexture = maxNumFramesPerRow * maxNumFramesPerCol;
                    int cnt = 0;
                    ImVec2 lastValidTextureSize(0,0);
                    while (maxNumFramesInATexture>=ag.frames)	{
                        // Here we just halve the 'textureSize', so that, if it fits, we save further texture space
                        lastValidTextureSize = textureSize;
                        if (cnt%2==0) textureSize.y = textureSize.y/2;
                        else textureSize.x = textureSize.x/2;
                        maxNumFramesPerRow = (int)textureSize.x/(int)ag.w;
                        maxNumFramesPerCol = (int)textureSize.y/(int)ag.h;
                        maxNumFramesInATexture = maxNumFramesPerRow * maxNumFramesPerCol;
                        ++cnt;
                    }
                    if (cnt>0)  {
                        textureSize=lastValidTextureSize;
                        maxNumFramesPerRow = (int)textureSize.x/(int)ag.w;
                        maxNumFramesPerCol = (int)textureSize.y/(int)ag.h;
                        maxNumFramesInATexture = maxNumFramesPerRow * maxNumFramesPerCol;
                    }
                    if (maxNumFramesInATexture>=ag.frames)	{
                        numFramesPerRowInPersistentTexture = maxNumFramesPerRow;
                        numFramesPerColInPersistentTexture = maxNumFramesPerCol;

                        rearrangeBufferForPersistentTexture();

                        // generate persistentTexture,delete buffer
                        IM_ASSERT(AnimatedImage::GenerateOrUpdateTextureCb!=NULL);	// Please use ImGui::AnimatedGif::SetGenerateOrUpdateTextureCallback(...) before calling this method
                        AnimatedImage::GenerateOrUpdateTextureCb(persistentTexId,ag.w*maxNumFramesPerRow,ag.h*maxNumFramesPerCol,4,&buffer[0],false,false,false);
                        buffer.clear();

                        hoverModeIfSupported = useHoverModeIfSupported;
                        //fprintf(stderr,"%d x %d (%d x %d)\n",numFramesPerRowInPersistentTexture,numFramesPerColInPersistentTexture,(int)textureSize.x,(int)textureSize.y);

                    }
                }
            }
        }
        else
        {
            ok = false;
            // TODO: Here we could load other image formats...
        }

        fclose(f);
        return ok;
    }
    bool create(ImTextureID myTexId,int animationImageWidth,int animationImageHeight,int numFrames,int numFramesPerRowInTexture,int numFramesPerColumnInTexture,float delayDetweenFramesInCs,bool useHoverMode=false)   {
        clear();
        persistentTexIdIsNotOwned = false;
        IM_ASSERT(myTexId);
        IM_ASSERT(animationImageWidth>0 && animationImageHeight>0);
        IM_ASSERT(numFrames>0);
        IM_ASSERT(delayDetweenFramesInCs>0);
        IM_ASSERT(numFramesPerRowInTexture*numFramesPerColumnInTexture>=numFrames);
        if (!myTexId || animationImageWidth<=0 || animationImageHeight<=0
                || numFrames<=0 || delayDetweenFramesInCs<=0 || (numFramesPerRowInTexture*numFramesPerColumnInTexture<numFrames))
            return false;
        persistentTexId = myTexId;
        persistentTexIdIsNotOwned = true;
        w = animationImageWidth;
        h = animationImageHeight;
        frames = numFrames;
        numFramesPerRowInPersistentTexture = numFramesPerRowInTexture;
        numFramesPerColInPersistentTexture = numFramesPerColumnInTexture;
        delays.resize(frames);
        for (int i=0;i<frames;i++) delays[i] = delayDetweenFramesInCs;
        hoverModeIfSupported = useHoverMode;
        return true;
    }

    inline bool areAllFramesInASingleTexture() const {return persistentTexId!=NULL;}
    void render(ImVec2 size=ImVec2(0,0), const ImVec2& uv0=ImVec2(0,0), const ImVec2& uv1=ImVec2(1,1), const ImVec4& tint_col=ImVec4(1,1,1,1), const ImVec4& border_col=ImVec4(0,0,0,0)) const  {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;
        if (size.x==0) size.x=w;
        else if (size.x<0) size.x=-size.x*w;
        if (size.y==0) size.y=h;
        else if (size.y<0) size.y=-size.y*h;
        size*=window->FontWindowScale*ImGui::GetIO().FontGlobalScale;

        ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        if (border_col.w > 0.0f)
            bb.Max += ImVec2(2,2);
        ItemSize(bb);
        if (!ItemAdd(bb, NULL))
            return;

        updateTexture();

        ImVec2 uv_0 = uv0;
        ImVec2 uv_1 = uv1;
        if (persistentTexId) {
            bool hovered = true;	// to fall back when useHoverModeIfSupported == false;
            if (hoverModeIfSupported) {
                hovered = ImGui::IsItemHovered();
                if (hovered) isAtLeastOneWidgetInHoverMode = true;
            }
            if (hovered)	{
                const ImVec2 uvFrameDelta = uvFrame1 - uvFrame0;
                uv_0 = uvFrame0 + uv0*uvFrameDelta;
                uv_1 = uvFrame0 + uv1*uvFrameDelta;
            }
            else {
                // We must use frame zero here:
                ImVec2 uvFrame0,uvFrame1;
                calculateTexCoordsForFrame(0,uvFrame0,uvFrame1);
                const ImVec2 uvFrameDelta = uvFrame1 - uvFrame0;
                uv_0 = uvFrame0 + uv0*uvFrameDelta;
                uv_1 = uvFrame0 + uv1*uvFrameDelta;
            }
        }
        if (border_col.w > 0.0f)
        {
            window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(border_col), 0.0f);
            window->DrawList->AddImage(texId, bb.Min+ImVec2(1,1), bb.Max-ImVec2(1,1), uv_0, uv_1, GetColorU32(tint_col));
        }
        else
        {
            window->DrawList->AddImage(texId, bb.Min, bb.Max, uv_0, uv_1, GetColorU32(tint_col));
        }
    }
    bool renderAsButton(const char* label,ImVec2 size=ImVec2(0,0), const ImVec2& uv0 = ImVec2(0,0),  const ImVec2& uv1 = ImVec2(1,1), int frame_padding = -1, const ImVec4& bg_col = ImVec4(0,0,0,0), const ImVec4& tint_col = ImVec4(1,1,1,1)) const {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        if (size.x==0) size.x=w;
        else if (size.x<0) size.x=-size.x*w;
        if (size.y==0) size.y=h;
        else if (size.y<0) size.y=-size.y*h;
        size*=window->FontWindowScale*ImGui::GetIO().FontGlobalScale;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        // Default to using texture ID as ID. User can still push string/integer prefixes.
        // We could hash the size/uv to create a unique ID but that would prevent the user from animating UV.
        ImGui::PushID((void *)this);
        const ImGuiID id = window->GetID(label);
        ImGui::PopID();

        const ImVec2 textSize = ImGui::CalcTextSize(label,NULL,true);
        const bool hasText = textSize.x>0;

        const float innerSpacing = hasText ? ((frame_padding >= 0) ? (float)frame_padding : (style.ItemInnerSpacing.x)) : 0.f;
        const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : style.FramePadding;
        const ImVec2 totalSizeWithoutPadding(size.x+innerSpacing+textSize.x,size.y>textSize.y ? size.y : textSize.y);
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + totalSizeWithoutPadding + padding*2);
        ImVec2 start(0,0);
        start = window->DC.CursorPos + padding;if (size.y<textSize.y) start.y+=(textSize.y-size.y)*.5f;
        const ImRect image_bb(start, start + size);
        start = window->DC.CursorPos + padding;start.x+=size.x+innerSpacing;if (size.y>textSize.y) start.y+=(size.y-textSize.y)*.5f;
        ItemSize(bb);
        if (!ItemAdd(bb, &id))
            return false;

        bool hovered=false, held=false;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        updateTexture();

        ImVec2 uv_0 = uv0;
        ImVec2 uv_1 = uv1;
        if (hovered && hoverModeIfSupported) isAtLeastOneWidgetInHoverMode = true;
        if ((persistentTexId && hoverModeIfSupported && hovered) || !persistentTexId || !hoverModeIfSupported) {
            const ImVec2 uvFrameDelta = uvFrame1 - uvFrame0;
            uv_0 = uvFrame0 + uv0*uvFrameDelta;
            uv_1 = uvFrame0 + uv1*uvFrameDelta;
        }
        else {
            // We must use frame zero here:
            ImVec2 uvFrame0,uvFrame1;
            calculateTexCoordsForFrame(0,uvFrame0,uvFrame1);
            const ImVec2 uvFrameDelta = uvFrame1 - uvFrame0;
            uv_0 = uvFrame0 + uv0*uvFrameDelta;
            uv_1 = uvFrame0 + uv1*uvFrameDelta;
        }


        // Render
        const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, style.FrameRounding));
        if (bg_col.w > 0.0f)
            window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));

        window->DrawList->AddImage(texId, image_bb.Min, image_bb.Max, uv_0, uv_1, GetColorU32(tint_col));

        if (hasText) ImGui::RenderText(start,label);
        return pressed;
    }


    inline int getWidth() const {return w;}
    inline int getHeight() const {return h;}
    inline int getNumFrames() const {return frames;}
    inline ImTextureID getTexture() const {return texId;}

    private:
    AnimatedImageInternal(const AnimatedImageInternal& ) {}
    void operator=(const AnimatedImageInternal& ) {}
    void rearrangeBufferForPersistentTexture()  {
        const int newBufferSize = w*numFramesPerRowInPersistentTexture*h*numFramesPerColInPersistentTexture*4;

        // BUFFER: frames images one below each other: size: 4*w x (h*frames)
        // TMP:    frames images numFramesPerRowInPersistentTexture * (4*w) x (h*numFramesPerColInPersistentTexture)

        const int strideSz = w*4;
        const int frameSz = strideSz*h;
        ImVector<unsigned char> tmp;tmp.resize(newBufferSize);

        unsigned char* pw=&tmp[0];
        const unsigned char* pr=&buffer[0];

        int frm=0,colSz=0;
        while (frm<frames)	{
            for (int y = 0; y<h;y++)    {
                pr=&buffer[frm*frameSz + y*strideSz];
                colSz = numFramesPerRowInPersistentTexture>(frames-frm)?(frames-frm):numFramesPerRowInPersistentTexture;
                for (int col = 0; col<colSz;col++)    {
                    memcpy(pw,pr,strideSz);
                    pr+=frameSz;
                    pw+=strideSz;
                }
                if (colSz<numFramesPerRowInPersistentTexture) {
                    for (int col = colSz;col<numFramesPerRowInPersistentTexture;col++)    {
                        memset(pw,0,strideSz);
                        pw+=strideSz;
                    }
                }
            }
            frm+=colSz;
        }

        //-----------------------------------------------------------------------
        buffer.swap(tmp);

#       ifdef DEBUG_OUT_TEXTURE
        stbi_write_png("testOutputPng.png", w*numFramesPerRowInPersistentTexture,h*numFramesPerColInPersistentTexture, 4, &buffer[0], w*numFramesPerRowInPersistentTexture*4);
#       undef DEBUG_OUT_TEXTURE
#       endif //DEBUG_OUT_TEXTURE
    }
    void calculateTexCoordsForFrame(int frm,ImVec2& uv0Out,ImVec2& uv1Out) const    {
        uv0Out=ImVec2((float)(frm%numFramesPerRowInPersistentTexture)/(float)numFramesPerRowInPersistentTexture,(float)(frm/numFramesPerRowInPersistentTexture)/(float)numFramesPerColInPersistentTexture);
        uv1Out=ImVec2(uv0Out.x+1.f/(float)numFramesPerRowInPersistentTexture,uv0Out.y+1.f/(float)numFramesPerColInPersistentTexture);
    }

};

AnimatedImage::FreeTextureDelegate AnimatedImage::FreeTextureCb =
#ifdef IMGUI_USE_AUTO_BINDING
    &ImImpl_FreeTexture;
#else //IMGUI_USE_AUTO_BINDING
    NULL;
#endif //IMGUI_USE_AUTO_BINDING
AnimatedImage::GenerateOrUpdateTextureDelegate AnimatedImage::GenerateOrUpdateTextureCb =
#ifdef IMGUI_USE_AUTO_BINDING
    &ImImpl_GenerateOrUpdateTexture;
#else //IMGUI_USE_AUTO_BINDING
    NULL;
#endif //IMGUI_USE_AUTO_BINDING

ImVec2 AnimatedImage::MaxPersistentTextureSize(2048,2048);

AnimatedImage::AnimatedImage(const char *filename, bool useHoverModeIfSupported)    {
    ptr = (AnimatedImageInternal*) ImGui::MemAlloc(sizeof(AnimatedImageInternal));
    IM_PLACEMENT_NEW(ptr) AnimatedImageInternal(filename,useHoverModeIfSupported);
}
AnimatedImage::AnimatedImage(ImTextureID myTexId, int animationImageWidth, int animationImageHeight, int numFrames, int numFramesPerRowInTexture, int numFramesPerColumnInTexture, float delayBetweenFramesInCs, bool useHoverMode) {
    ptr = (AnimatedImageInternal*) ImGui::MemAlloc(sizeof(AnimatedImageInternal));
    IM_PLACEMENT_NEW(ptr) AnimatedImageInternal(myTexId,animationImageWidth,animationImageHeight,numFrames,numFramesPerRowInTexture,numFramesPerColumnInTexture,delayBetweenFramesInCs,useHoverMode);
}
AnimatedImage::AnimatedImage()  {
    ptr = (AnimatedImageInternal*) ImGui::MemAlloc(sizeof(AnimatedImageInternal));
    IM_PLACEMENT_NEW(ptr) AnimatedImageInternal();
}
AnimatedImage::~AnimatedImage() {
    clear();
    ptr->~AnimatedImageInternal();
    ImGui::MemFree(ptr);ptr=NULL;
}
void AnimatedImage::clear() {ptr->clear();}
void AnimatedImage::render(ImVec2 size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &tint_col, const ImVec4 &border_col) const   {ptr->render(size,uv0,uv1,tint_col,border_col);}
bool AnimatedImage::renderAsButton(const char *label, ImVec2 size, const ImVec2 &uv0, const ImVec2 &uv1, int frame_padding, const ImVec4 &bg_col, const ImVec4 &tint_col)   {return ptr->renderAsButton(label,size,uv0,uv1,frame_padding,bg_col,tint_col);}
bool AnimatedImage::load(const char *filename, bool useHoverModeIfSupported)    {return ptr->load(filename,useHoverModeIfSupported);}
bool AnimatedImage::create(ImTextureID myTexId, int animationImageWidth, int animationImageHeight, int numFrames, int numFramesPerRowInTexture, int numFramesPerColumnInTexture, float delayBetweenFramesInCs, bool useHoverMode)   {return ptr->create(myTexId,animationImageWidth,animationImageHeight,numFrames,numFramesPerRowInTexture,numFramesPerColumnInTexture,delayBetweenFramesInCs,useHoverMode);}
int AnimatedImage::getWidth() const {return ptr->getWidth();}
int AnimatedImage::getHeight() const    {return ptr->getHeight();}
int AnimatedImage::getNumFrames() const {return ptr->getNumFrames();}
bool AnimatedImage::areAllFramesInASingleTexture() const    {return ptr->areAllFramesInASingleTexture();}
#endif //NO_IMGUIVARIOUSCONTROLS_ANIMATEDIMAGE


/*
    inline ImVec2 mouseToPdfRelativeCoords(const ImVec2 &mp) const {
       return ImVec2((mp.x+cursorPosAtStart.x-startPos.x)*(uv1.x-uv0.x)/zoomedImageSize.x+uv0.x,
               (mp.y+cursorPosAtStart.y-startPos.y)*(uv1.y-uv0.y)/zoomedImageSize.y+uv0.y);
    }
    inline ImVec2 pdfRelativeToMouseCoords(const ImVec2 &mp) const {
        return ImVec2((mp.x-uv0.x)*(zoomedImageSize.x)/(uv1.x-uv0.x)+startPos.x-cursorPosAtStart.x,(mp.y-uv0.y)*(zoomedImageSize.y)/(uv1.y-uv0.y)+startPos.y-cursorPosAtStart.y);
    }
*/
bool ImageZoomAndPan(ImTextureID user_texture_id, const ImVec2& size,float aspectRatio,float& zoom,ImVec2& zoomCenter,int panMouseButtonDrag,int resetZoomAndPanMouseButton,const ImVec2& zoomMaxAndZoomStep)
{
    bool rv = false;
    ImGuiWindow* window = GetCurrentWindow();
    if (!window || window->SkipItems) return rv;
    ImVec2 curPos = ImGui::GetCursorPos();
    const ImVec2 wndSz(size.x>0 ? size.x : ImGui::GetWindowSize().x-curPos.x,size.y>0 ? size.y : ImGui::GetWindowSize().y-curPos.y);

    IM_ASSERT(wndSz.x!=0 && wndSz.y!=0 && zoom!=0);

    // Here we use the whole size (although it can be partially empty)
    ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + wndSz.x,window->DC.CursorPos.y + wndSz.y));
    ItemSize(bb);
    if (!ItemAdd(bb, NULL)) return rv;

    ImVec2 imageSz = wndSz;
    ImVec2 remainingWndSize(0,0);
    if (aspectRatio!=0) {
        const float wndAspectRatio = wndSz.x/wndSz.y;
        if (aspectRatio >= wndAspectRatio) {imageSz.y = imageSz.x/aspectRatio;remainingWndSize.y = wndSz.y - imageSz.y;}
        else {imageSz.x = imageSz.y*aspectRatio;remainingWndSize.x = wndSz.x - imageSz.x;}
    }

    if (ImGui::IsItemHovered()) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.MouseWheel!=0) {
            if (io.KeyCtrl) {
                const float zoomStep = zoomMaxAndZoomStep.y;
                const float zoomMin = 1.f;
                const float zoomMax = zoomMaxAndZoomStep.x;
                if (io.MouseWheel < 0) {zoom/=zoomStep;if (zoom<zoomMin) zoom=zoomMin;}
                else {zoom*=zoomStep;if (zoom>zoomMax) zoom=zoomMax;}
                rv = true;
                /*if (io.FontAllowUserScaling) {
                    // invert effect:
                    // Zoom / Scale window
                    ImGuiContext& g = *GImGui;
                    ImGuiWindow* window = g.HoveredWindow;
                    float new_font_scale = ImClamp(window->FontWindowScale - g.IO.MouseWheel * 0.10f, 0.50f, 2.50f);
                    float scale = new_font_scale / window->FontWindowScale;
                    window->FontWindowScale = new_font_scale;

                    const ImVec2 offset = window->Size * (1.0f - scale) * (g.IO.MousePos - window->Pos) / window->Size;
                    window->Pos += offset;
                    window->PosFloat += offset;
                    window->Size *= scale;
                    window->SizeFull *= scale;
                }*/
            }
            else  {
                const bool scrollDown = io.MouseWheel <= 0;
                const float zoomFactor = .5/zoom;
                if ((!scrollDown && zoomCenter.y > zoomFactor) || (scrollDown && zoomCenter.y <  1.f - zoomFactor))  {
                    const float slideFactor = zoomMaxAndZoomStep.y*0.1f*zoomFactor;
                    if (scrollDown) {
                        zoomCenter.y+=slideFactor;///(imageSz.y*zoom);
                        if (zoomCenter.y >  1.f - zoomFactor) zoomCenter.y =  1.f - zoomFactor;
                    }
                    else {
                        zoomCenter.y-=slideFactor;///(imageSz.y*zoom);
                        if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
                    }
                    rv = true;
                }
            }
        }
        if (io.MouseClicked[resetZoomAndPanMouseButton]) {zoom=1.f;zoomCenter.x=zoomCenter.y=.5f;rv = true;}
        if (ImGui::IsMouseDragging(panMouseButtonDrag,1.f))   {
            zoomCenter.x-=io.MouseDelta.x/(imageSz.x*zoom);
            zoomCenter.y-=io.MouseDelta.y/(imageSz.y*zoom);
            rv = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
        }
    }

    const float zoomFactor = .5/zoom;
    if (rv) {
        if (zoomCenter.x < zoomFactor) zoomCenter.x = zoomFactor;
        else if (zoomCenter.x > 1.f - zoomFactor) zoomCenter.x = 1.f - zoomFactor;
        if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
        else if (zoomCenter.y > 1.f - zoomFactor) zoomCenter.y = 1.f - zoomFactor;
    }

    ImVec2 uvExtension(2.f*zoomFactor,2.f*zoomFactor);
    if (remainingWndSize.x > 0) {
        const float remainingSizeInUVSpace = 2.f*zoomFactor*(remainingWndSize.x/imageSz.x);
        const float deltaUV = uvExtension.x;
        const float remainingUV = 1.f-deltaUV;
        if (deltaUV<1) {
            float adder = (remainingUV < remainingSizeInUVSpace ? remainingUV : remainingSizeInUVSpace);
            uvExtension.x+=adder;
            remainingWndSize.x-= adder * zoom * imageSz.x;
            imageSz.x+=adder * zoom * imageSz.x;

            if (zoomCenter.x < uvExtension.x*.5f) zoomCenter.x = uvExtension.x*.5f;
            else if (zoomCenter.x > 1.f - uvExtension.x*.5f) zoomCenter.x = 1.f - uvExtension.x*.5f;
        }
    }
    if (remainingWndSize.y > 0) {
        const float remainingSizeInUVSpace = 2.f*zoomFactor*(remainingWndSize.y/imageSz.y);
        const float deltaUV = uvExtension.y;
        const float remainingUV = 1.f-deltaUV;
        if (deltaUV<1) {
            float adder = (remainingUV < remainingSizeInUVSpace ? remainingUV : remainingSizeInUVSpace);
            uvExtension.y+=adder;
            remainingWndSize.y-= adder * zoom * imageSz.y;
            imageSz.y+=adder * zoom * imageSz.y;

            if (zoomCenter.y < uvExtension.y*.5f) zoomCenter.y = uvExtension.y*.5f;
            else if (zoomCenter.y > 1.f - uvExtension.y*.5f) zoomCenter.y = 1.f - uvExtension.y*.5f;
        }
    }

    ImVec2 uv0((zoomCenter.x-uvExtension.x*.5f),(zoomCenter.y-uvExtension.y*.5f));
    ImVec2 uv1((zoomCenter.x+uvExtension.x*.5f),(zoomCenter.y+uvExtension.y*.5f));


    /* // Here we use just the window size, but then ImGui::IsItemHovered() should be moved below this block. How to do it?
    ImVec2 startPos=window->DC.CursorPos;
    startPos.x+= remainingWndSize.x*.5f;
    startPos.y+= remainingWndSize.y*.5f;
    ImVec2 endPos(startPos.x+imageSz.x,startPos.y+imageSz.y);
    ImRect bb(startPos, endPos);
    ItemSize(bb);
    if (!ItemAdd(bb, NULL)) return rv;*/

    ImVec2 startPos=bb.Min,endPos=bb.Max;
    startPos.x+= remainingWndSize.x*.5f;
    startPos.y+= remainingWndSize.y*.5f;
    endPos.x = startPos.x + imageSz.x;
    endPos.y = startPos.y + imageSz.y;

    window->DrawList->AddImage(user_texture_id, startPos, endPos, uv0, uv1);

    return rv;
}

inline static bool GlyphButton(ImGuiID id, const ImVec2& pos,const ImVec2& halfSize,const char* text,bool* toggleButtonState=NULL,bool *pHovered = NULL)    {
    ImGuiWindow* window = GetCurrentWindow();

    const ImRect bb(pos - halfSize, pos + halfSize);

    bool hovered=false, held=false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held,ImGuiButtonFlags_PressedOnRelease);
    if (pHovered) *pHovered = hovered;
    const bool isACheckedToggleButton = (toggleButtonState && *toggleButtonState);
    const bool useNormalButtonStyle = (text && text[0]!='\0' && !isACheckedToggleButton);   // Otherwise use CloseButtonStyle

    // Render    
    ImU32 col = GetColorU32((held && hovered) ? (useNormalButtonStyle ? ImGuiCol_ButtonActive : ImGuiCol_CloseButtonActive) : hovered ? (useNormalButtonStyle ? ImGuiCol_ButtonHovered : ImGuiCol_CloseButtonHovered) : (useNormalButtonStyle ? ImGuiCol_Button : ImGuiCol_CloseButton));
    ImU32 textCol = GetColorU32(ImGuiCol_Text);
    if (!hovered) {
        col = (((col>>24)/2)<<24)|(col&0x00FFFFFF);
        textCol = (((textCol>>24)/2)<<24)|(textCol&0x00FFFFFF);
    }
    window->DrawList->AddRectFilled(bb.GetTL(),bb.GetBR(), col, text ? 2 : 6);

    if (text) {
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        window->DrawList->AddText(
                    ImVec2(bb.GetTL().x+(bb.GetWidth()-textSize.x)*0.5f,bb.GetTL().y+(bb.GetHeight()-textSize.y)*0.5f),
                    textCol,
                    text);
    }
    else {  // fallback to the close button
        const ImVec2 center = bb.GetCenter();
        const float cross_extent = ((halfSize.x * 0.7071f) - 1.0f)*0.75f;
        window->DrawList->AddLine(center + ImVec2(+cross_extent,+cross_extent), center + ImVec2(-cross_extent,-cross_extent), textCol ,2.f);
        window->DrawList->AddLine(center + ImVec2(+cross_extent,-cross_extent), center + ImVec2(-cross_extent,+cross_extent), textCol, 2.f);
    }

    if (toggleButtonState && pressed) *toggleButtonState=!(*toggleButtonState);
    return (pressed);
}

static int AppendTreeNodeHeaderButtonsV(const void* ptr_id,float startWindowCursorXForClipping,int numButtons,va_list args)   {
    if (!ImGui::IsItemVisible()) return -1; // We don't reset non-togglable buttons to false for performance reasons

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(ptr_id);

    ImGuiContext& g = *GImGui;
    const float buttonSz = g.FontSize;
    const ImVec2 glyphHalfSize(buttonSz*0.5f,buttonSz*0.5f);
    ImVec2 pos(ImMin(window->DC.LastItemRect.Max.x, window->ClipRect.Max.x) - g.Style.FramePadding.x -buttonSz*0.5f, window->DC.LastItemRect.Min.y + g.Style.FramePadding.y+buttonSz*0.5f);
    bool pressed = false,hovered = false;

    bool* pPressed = NULL;
    const char* tooltip = NULL;
    const char* glyph = NULL;
    int isToggleButton = 0, rv = -1;

    for (int i=0;i<numButtons;i++)  {
        pPressed = va_arg(args, bool*);tooltip = va_arg(args, const char*);
        glyph = va_arg(args, const char*);isToggleButton = va_arg(args, int);

        if (pPressed)	{
            //fprintf(stderr,"btn:%d pos.x=%1.0f startWindowCursorXForClipping=%1.0f\n",i,pos.x,startWindowCursorXForClipping);
            if (pos.x>startWindowCursorXForClipping+(4.0f*buttonSz))   {
                id = window->GetID((void*)(intptr_t)(id+1));
                if ((pressed = ImGui::GlyphButton(id, pos, glyphHalfSize,glyph,isToggleButton ? pPressed : NULL,&hovered))) rv = i;
                if (!isToggleButton) *pPressed = pressed;
                pos.x-=buttonSz-2;
                if (tooltip && hovered && strlen(tooltip)>0) ImGui::SetTooltip("%s",tooltip);
                if (hovered && rv==-1) rv = numButtons+i;
            }
            else {
                pressed = false;
                if (!isToggleButton) *pPressed = false; // Just in case we pass a static bool
            }
            //if (pressed) fprintf(stderr,"Pressed button %d: '%s'\n",i,glyph?glyph:"close"); // TO REMOVE
        }
        else pos.x-=buttonSz-2; // separator mode
    }
    return rv;
}

int AppendTreeNodeHeaderButtons(const void* ptr_id, float startWindowCursorXForClipping, int numButtons, ...) {
    va_list args;
    va_start(args, numButtons);
    const int rv = AppendTreeNodeHeaderButtonsV(ptr_id, startWindowCursorXForClipping, numButtons, args);
    va_end(args);
    return rv;
}

// Start PlotHistogram(...) implementation -------------------------------
struct ImGuiPlotMultiArrayGetterData    {
    const float** Values;int Stride;
    ImGuiPlotMultiArrayGetterData(const float** values, int stride) { Values = values; Stride = stride; }

    // Some static helper methods stuffed in this struct fo convenience
    inline static void GetVerticalGradientTopAndBottomColors(ImU32 c,float fillColorGradientDeltaIn0_05,ImU32& tc,ImU32& bc)  {
        if (fillColorGradientDeltaIn0_05==0) {tc=bc=c;return;}

        const bool negative = (fillColorGradientDeltaIn0_05<0);
        if (negative) fillColorGradientDeltaIn0_05=-fillColorGradientDeltaIn0_05;
        if (fillColorGradientDeltaIn0_05>0.5f) fillColorGradientDeltaIn0_05=0.5f;
        // Can we do it without the double conversion ImU32 -> ImVec4 -> ImU32 ?
        const ImVec4 cf = ColorConvertU32ToFloat4(c);
        ImVec4 tmp(cf.x+fillColorGradientDeltaIn0_05,cf.y+fillColorGradientDeltaIn0_05,cf.z+fillColorGradientDeltaIn0_05,cf.w+fillColorGradientDeltaIn0_05);
        if (tmp.x>1.f) tmp.x=1.f;if (tmp.y>1.f) tmp.y=1.f;if (tmp.z>1.f) tmp.z=1.f;if (tmp.w>1.f) tmp.w=1.f;
        if (negative) bc = ColorConvertFloat4ToU32(tmp); else tc = ColorConvertFloat4ToU32(tmp);
        tmp=ImVec4(cf.x-fillColorGradientDeltaIn0_05,cf.y-fillColorGradientDeltaIn0_05,cf.z-fillColorGradientDeltaIn0_05,cf.w-fillColorGradientDeltaIn0_05);
        if (tmp.x<0.f) tmp.x=0.f;if (tmp.y<0.f) tmp.y=0.f;if (tmp.z<0.f) tmp.z=0.f;if (tmp.w<0.f) tmp.w=0.f;
        if (negative) tc = ColorConvertFloat4ToU32(tmp); else bc = ColorConvertFloat4ToU32(tmp);
    }
    // Same as default ImSaturate, but overflowOut can be -1,0 or 1 in case of clamping:
    inline static float ImSaturate(float f,short& overflowOut)	{
        if (f < 0.0f) {overflowOut=-1;return 0.0f;}
        if (f > 1.0f) {overflowOut=1;return 1.0f;}
        overflowOut=0;return f;
    }
    // Same as IsMouseHoveringRect, but only for the X axis (we already know if the whole item has been hovered or not)
    inline static bool IsMouseBetweenXValues(float x_min, float x_max,float *pOptionalXDeltaOut=NULL, bool clip=true, bool expandForTouchInput=false)   {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindowRead();

        if (clip) {
            if (x_min < window->ClipRect.Min.x) x_min = window->ClipRect.Min.x;
            if (x_max > window->ClipRect.Max.x) x_max = window->ClipRect.Max.x;
        }
        if (expandForTouchInput)    {x_min-=g.Style.TouchExtraPadding.x;x_max+=g.Style.TouchExtraPadding.x;}

        if (pOptionalXDeltaOut) *pOptionalXDeltaOut = g.IO.MousePos.x - x_min;
        return (g.IO.MousePos.x >= x_min && g.IO.MousePos.x < x_max);
    }
};
static float Plot_MultiArrayGetter(void* data, int idx,int histogramIdx)    {
    ImGuiPlotMultiArrayGetterData* plot_data = (ImGuiPlotMultiArrayGetterData*)data;
    const float v = *(float*)(void*)((unsigned char*)(&plot_data->Values[histogramIdx][0]) + (size_t)idx * plot_data->Stride);
    return v;
}
int PlotHistogram(const char* label, const float** values,int num_histograms,int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride,float histogramGroupSpacingInPixels,int* pOptionalHoveredHistogramIndexOut,float fillColorGradientDeltaIn0_05,const ImU32* pColorsOverride,int numColorsOverride)   {
    ImGuiPlotMultiArrayGetterData data(values, stride);
    return PlotHistogram(label, &Plot_MultiArrayGetter, (void*)&data, num_histograms, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size,histogramGroupSpacingInPixels,pOptionalHoveredHistogramIndexOut,fillColorGradientDeltaIn0_05,pColorsOverride,numColorsOverride);
}
int PlotHistogram(const char* label, float (*values_getter)(void* data, int idx,int histogramIdx), void* data,int num_histograms, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size,float histogramGroupSpacingInPixels,int* pOptionalHoveredHistogramIndexOut,float fillColorGradientDeltaIn0_05,const ImU32* pColorsOverride,int numColorsOverride)  {
    ImGuiWindow* window = GetCurrentWindow();
    if (pOptionalHoveredHistogramIndexOut) *pOptionalHoveredHistogramIndexOut=-1;
    if (window->SkipItems) return -1;

    static const float minSingleHistogramWidth = 5.f;   // in pixels
    static const float maxSingleHistogramWidth = 100.f;   // in pixels

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (graph_size.x == 0.0f) graph_size.x = CalcItemWidth();
    if (graph_size.y == 0.0f) graph_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, NULL)) return -1;

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX || scale_min==scale_max)   {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < values_count; i++)  {
            for (int h=0;h<num_histograms;h++)  {
                const float v = values_getter(data, (i + values_offset) % values_count, h);
                v_min = ImMin(v_min, v);
                v_max = ImMax(v_max, v);
            }
        }
        if (scale_min == FLT_MAX  || scale_min>=scale_max) scale_min = v_min;
        if (scale_max == FLT_MAX  || scale_min>=scale_max) scale_max = v_max;
    }
    if (scale_min>scale_max) {float tmp=scale_min;scale_min=scale_max;scale_max=tmp;}

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    static ImU32 col_base_embedded[] = {0,IM_COL32(100,100,225,255),IM_COL32(100,225,100,255),IM_COL32(225,100,100,255),IM_COL32(150,75,225,255)};
    static const int num_colors_embedded = sizeof(col_base_embedded)/sizeof(col_base_embedded[0]);


    int v_hovered = -1, h_hovered = -1;
    if (values_count > 0 && num_histograms > 0)
    {
        const bool mustOverrideColors = (pColorsOverride && numColorsOverride>0);
        const ImU32* col_base = mustOverrideColors ? pColorsOverride : col_base_embedded;
        const int num_colors = mustOverrideColors ? numColorsOverride : num_colors_embedded;

        const int total_histograms = values_count * num_histograms;

        const bool isItemHovered = IsHovered(inner_bb, 0);

        if (!mustOverrideColors) col_base_embedded[0] = GetColorU32(ImGuiCol_PlotHistogram);
        const ImU32 lineCol = GetColorU32(ImGuiCol_WindowBg);
        const float lineThick = 1.f;
        const ImU32 overflowCol = lineCol;

        const ImVec2 inner_bb_extension(inner_bb.Max.x-inner_bb.Min.x,inner_bb.Max.y-inner_bb.Min.y);
        const float scale_extension = scale_max - scale_min;
        const bool hasXAxis = scale_max>=0 && scale_min<=0;
        const float xAxisSat = ImSaturate((0.0f - scale_min) / (scale_max - scale_min));
        const float xAxisSatComp = 1.f-xAxisSat;
        const float posXAxis = inner_bb.Max.y-inner_bb_extension.y*xAxisSat-0.5f;
        const bool isAlwaysNegative = scale_max<0 && scale_min<0;

        float t_step = (inner_bb.Max.x-inner_bb.Min.x-(float)(values_count-1)*histogramGroupSpacingInPixels)/(float)total_histograms;
        if (t_step<minSingleHistogramWidth) t_step = minSingleHistogramWidth;
        else if (t_step>maxSingleHistogramWidth) t_step = maxSingleHistogramWidth;

        float t1 = 0.f;
        float posY=0.f;ImVec2 pos0(0.f,0.f),pos1(0.f,0.f);
        ImU32 rectCol=0,topRectCol=0,bottomRectCol=0;float gradient = 0.f;
        short overflow = 0;bool mustSkipBorder = false;int iWithOffset=0;
        for (int i = 0; i < values_count; i++)  {
            if (i!=0) t1+=histogramGroupSpacingInPixels;
            if (t1>inner_bb.Max.x) break;
            iWithOffset = (i + values_offset) % values_count;
            for (int h=0;h<num_histograms;h++)  {
                const float v1 = values_getter(data, iWithOffset, h);

                pos0.x = inner_bb.Min.x+t1;
                pos1.x = pos0.x+t_step;

                const int col_num = h%num_colors;
                rectCol = col_base[col_num];
                if (isItemHovered &&
                //ImGui::IsMouseHoveringRect(ImVec2(pos0.x,inner_bb.Min.y),ImVec2(pos1.x,inner_bb.Max.y))
                ImGuiPlotMultiArrayGetterData::IsMouseBetweenXValues(pos0.x,pos1.x)
                ) {
                    h_hovered = h;
                    v_hovered = iWithOffset; // iWithOffset or just i ?
                    if (pOptionalHoveredHistogramIndexOut) *pOptionalHoveredHistogramIndexOut=h_hovered;
                    SetTooltip("%d: %8.4g", i, v1); // Tooltip on hover

                    if (h==0 && !mustOverrideColors) rectCol = GetColorU32(ImGuiCol_PlotHistogramHovered); // because: col_base[0] = GetColorU32(ImGuiCol_PlotHistogram);
                    else {
                        // We don't have any hover color ready, but we can calculate it based on the same logic used between ImGuiCol_PlotHistogramHovered and ImGuiCol_PlotHistogram.
                        // Note that this code is executed only once, when a histogram is hovered.
                        const ImGuiStyle& style = ImGui::GetStyle();
                        ImVec4 diff = style.Colors[ImGuiCol_PlotHistogramHovered];
                        ImVec4 base = style.Colors[ImGuiCol_PlotHistogram];
                        diff.x-=base.x;diff.y-=base.y;diff.z-=base.z;diff.w-=base.w;
                        base = ImGui::ColorConvertU32ToFloat4(rectCol);
                        if (style.Alpha!=0.f) base.w /= style.Alpha;	// See GetColorU32(...) for this
                        base.x+=diff.x;base.y+=diff.y;base.z+=diff.z;base.w+=diff.w;
                        base = ImVec4(ImSaturate(base.x),ImSaturate(base.y),ImSaturate(base.z),ImSaturate(base.w));
                        rectCol = GetColorU32(base);
                    }
                }
                gradient = fillColorGradientDeltaIn0_05;
                mustSkipBorder = false;

                if (!hasXAxis) {
                    if (isAlwaysNegative)   {
                        posY = inner_bb_extension.y*ImGuiPlotMultiArrayGetterData::ImSaturate((scale_max - v1) / scale_extension,overflow);
                        overflow=-overflow;

                        pos0.y = inner_bb.Min.y;
                        pos1.y = inner_bb.Min.y+posY;

                        gradient = -fillColorGradientDeltaIn0_05;
                        mustSkipBorder = (overflow==1);
                    }
                    else {
                        posY = inner_bb_extension.y*ImGuiPlotMultiArrayGetterData::ImSaturate((v1 - scale_min) / scale_extension,overflow);

                        pos0.y = inner_bb.Max.y-posY;
                        pos1.y = inner_bb.Max.y;
                        mustSkipBorder = (overflow==-1);
                    }
                }
                else if (v1>=0){
                    posY = ImGuiPlotMultiArrayGetterData::ImSaturate(v1/scale_max,overflow);
                    pos1.y = posXAxis;
                    pos0.y = posXAxis-inner_bb_extension.y*xAxisSatComp*posY;
                }
                else {
                    posY = ImGuiPlotMultiArrayGetterData::ImSaturate(v1/scale_min,overflow);
                    overflow = -overflow;	// Probably redundant
                    pos0.y = posXAxis;
                    pos1.y = posXAxis+inner_bb_extension.y*xAxisSat*posY;
                    gradient = -fillColorGradientDeltaIn0_05;
                }

                ImGuiPlotMultiArrayGetterData::GetVerticalGradientTopAndBottomColors(rectCol,gradient,topRectCol,bottomRectCol);

                window->DrawList->AddRectFilledMultiColor(pos0, pos1,topRectCol,topRectCol,bottomRectCol,bottomRectCol); // Gradient for free!

                if (overflow!=0)    {
                    // Here we draw the small arrow that indicates that the histogram is out of scale
                    const float spacing = lineThick+1;
                    const float height = inner_bb_extension.y*0.075f;
                    // Using CW order here... but I'm not sure this is correct in Dear ImGui (we should enable GL_CULL_FACE and see if it's the same output)
                    if (overflow>0)	    window->DrawList->AddTriangleFilled(ImVec2((pos0.x+pos1.x)*0.5f,inner_bb.Min.y+spacing),ImVec2(pos1.x-spacing,inner_bb.Min.y+spacing+height), ImVec2(pos0.x+spacing,inner_bb.Min.y+spacing+height),overflowCol);
                    else if (overflow<0)    window->DrawList->AddTriangleFilled(ImVec2((pos0.x+pos1.x)*0.5f,inner_bb.Max.y-spacing),ImVec2(pos0.x+spacing,inner_bb.Max.y-spacing-height), ImVec2(pos1.x-spacing,inner_bb.Max.y-spacing-height),overflowCol);
                }

                if (!mustSkipBorder) window->DrawList->AddRect(pos0, pos1,lineCol,0.f,0,lineThick);

                t1+=t_step; if (t1>inner_bb.Max.x) break;
            }
        }

        if (hasXAxis) {
            // Draw x Axis:
            const ImU32 axisCol = GetColorU32(ImGuiCol_Text);
            const float axisThick = 1.f;
            window->DrawList->AddLine(ImVec2(inner_bb.Min.x,posXAxis),ImVec2(inner_bb.Max.x,posXAxis),axisCol,axisThick);
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f,0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    return v_hovered;
}
int PlotHistogram2(const char* label, const float* values,int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride,float fillColorGradientDeltaIn0_05,const ImU32* pColorsOverride,int numColorsOverride) {
    const float* pValues[1] = {values};
    return PlotHistogram(label,pValues,1,values_count,values_offset,overlay_text,scale_min,scale_max,graph_size,stride,0.f,NULL,fillColorGradientDeltaIn0_05,pColorsOverride,numColorsOverride);
}
// End PlotHistogram(...) implementation ----------------------------------
// Start PlotCurve(...) implementation ------------------------------------
int PlotCurve(const char* label, float (*values_getter)(void* data, float x,int numCurve), void* data,int num_curves,const char* overlay_text,const ImVec2 rangeY,const ImVec2 rangeX, ImVec2 graph_size,ImVec2* pOptionalHoveredValueOut,float precisionInPixels,float numGridLinesHint,const ImU32* pColorsOverride,int numColorsOverride)  {
    ImGuiWindow* window = GetCurrentWindow();
    if (pOptionalHoveredValueOut) *pOptionalHoveredValueOut=ImVec2(0,0);    // Not sure how to initialize this...
    if (window->SkipItems || !values_getter) return -1;

    if (precisionInPixels<=1.f) precisionInPixels = 1.f;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (graph_size.x == 0.0f) graph_size.x = CalcItemWidth();
    if (graph_size.y == 0.0f) graph_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, NULL)) return -1;
    const ImVec2 inner_bb_extension(inner_bb.Max.x-inner_bb.Min.x,inner_bb.Max.y-inner_bb.Min.y);

    ImVec2 xRange = rangeX,yRange = rangeY,rangeExtension(0,0);

    if (yRange.x>yRange.y) {float tmp=yRange.x;yRange.x=yRange.y;yRange.y=tmp;}
    else if (yRange.x==yRange.y) {yRange.x-=5.f;yRange.y+=5.f;}
    rangeExtension.y = yRange.y-yRange.x;

    if (xRange.x>xRange.y) {float tmp=xRange.x;xRange.x=xRange.y;xRange.y=tmp;}
    else if (xRange.x==xRange.y) xRange.x-=1.f;
    if (xRange.y==FLT_MAX || xRange.x==xRange.y) xRange.y = xRange.x + (rangeExtension.y/inner_bb_extension.y)*inner_bb_extension.x;
    rangeExtension.x = xRange.y-xRange.x;

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    static ImU32 col_base_embedded[] = {0,IM_COL32(150,150,225,255),IM_COL32(150,225,150,255),IM_COL32(225,150,150,255),IM_COL32(150,100,225,255)};
    static const int num_colors_embedded = sizeof(col_base_embedded)/sizeof(col_base_embedded[0]);


    int v_hovered=-1;int h_hovered = -1;ImVec2 hoveredValue(FLT_MAX,FLT_MAX);
    {
        const bool mustOverrideColors = (pColorsOverride && numColorsOverride>0);
        const ImU32* col_base = mustOverrideColors ? pColorsOverride : col_base_embedded;
        const int num_colors = mustOverrideColors ? numColorsOverride : num_colors_embedded;

        const bool isItemHovered = IsHovered(inner_bb, 0);

        if (!mustOverrideColors) col_base_embedded[0] = GetColorU32(ImGuiCol_PlotLines);

        const bool hasXAxis = yRange.y>=0 && yRange.x<=0;
        const float xAxisSat = ImSaturate((0.0f - yRange.x) / rangeExtension.y);
        const float posXAxis = inner_bb.Max.y-inner_bb_extension.y*xAxisSat;

        const bool hasYAxis = xRange.y>=0 && xRange.x<=0;
        const float yAxisSat = ImSaturate((0.0f-xRange.x) / rangeExtension.x);
        const float posYAxis = inner_bb.Min.x+inner_bb_extension.x*yAxisSat;

        // Draw additinal horizontal and vertical lines: TODO: Fix this it's wrong
        if (numGridLinesHint>=1.f)   {
            ImU32 lineCol = GetColorU32(ImGuiCol_WindowBg);
            lineCol = (((lineCol>>24)/2)<<24)|(lineCol&0x00FFFFFF);	// Halve alpha
            const float lineThick = 1.f;
            float lineSpacing = (rangeExtension.x < rangeExtension.y) ? rangeExtension.x : rangeExtension.y;
            lineSpacing/=numGridLinesHint;      // We draw 'numGridLinesHint' lines (or so) on the shortest axis
            lineSpacing = floor(lineSpacing);   // We keep integral delta
            if (lineSpacing>0)  {
                float pos=0.f;
                // Draw horizontal lines:
                float rangeCoord = floor(yRange.x);if (rangeCoord<yRange.x) rangeCoord+=lineSpacing;
                while (rangeCoord<=yRange.y) {
                    pos = inner_bb.Max.y-inner_bb_extension.y*(yRange.y-rangeCoord)/rangeExtension.y;
                    window->DrawList->AddLine(ImVec2(inner_bb.Min.x,pos),ImVec2(inner_bb.Max.x,pos),lineCol,lineThick);
                    rangeCoord+=lineSpacing;
                }
                // Draw vertical lines:
                rangeCoord = floor(xRange.x);if (rangeCoord<xRange.x) rangeCoord+=lineSpacing;
                while (rangeCoord<=xRange.y) {
                    pos = inner_bb.Max.x-inner_bb_extension.x*(xRange.y-rangeCoord)/rangeExtension.x;
                    window->DrawList->AddLine(ImVec2(pos,inner_bb.Min.y),ImVec2(pos,inner_bb.Max.y),lineCol,lineThick);
                    rangeCoord+=lineSpacing;
                }
            }
        }

        const ImU32 axisCol = GetColorU32(ImGuiCol_Text);
        const float axisThick = 1.f;
        if (hasXAxis) {
            // Draw x Axis:
            window->DrawList->AddLine(ImVec2(inner_bb.Min.x,posXAxis),ImVec2(inner_bb.Max.x,posXAxis),axisCol,axisThick);
        }
        if (hasYAxis) {
            // Draw y Axis:
            window->DrawList->AddLine(ImVec2(posYAxis,inner_bb.Min.y),ImVec2(posYAxis,inner_bb.Max.y),axisCol,axisThick);
        }

        float mouseHoverDeltaX = 0.f;float minDistanceValue=FLT_MAX;float hoveredValueXInBBCoords=-1.f;
        ImU32 curveCol=0;const float curveThick=2.f;
        ImVec2 pos0(0.f,0.f),pos1(0.f,0.f);
        float yValue=0.f,lastYValue=0.f;
        const float t_step_xScale = precisionInPixels*rangeExtension.x/inner_bb_extension.x;
        const float t1Start = xRange.x+t_step_xScale;
        const float t1Max = xRange.x + rangeExtension.x;
        for (int h=0;h<num_curves;h++)  {
            const int col_num = h%num_colors;
            curveCol = col_base[col_num];
            lastYValue = values_getter(data, xRange.x, h);
            pos0.x = inner_bb.Min.x;
            pos0.y = inner_bb.Max.y - inner_bb_extension.y*((lastYValue-yRange.x)/rangeExtension.y);

            int v_idx = 0;
            for (float t1 = t1Start; t1 < t1Max; t1+=t_step_xScale)  {
                yValue = values_getter(data, t1, h);

                pos1.x = pos0.x+precisionInPixels;
                pos1.y = inner_bb.Max.y - inner_bb_extension.y*((yValue-yRange.x)/rangeExtension.y);

                if (pos0.y>=inner_bb.Min.y && pos0.y<=inner_bb.Max.y && pos1.y>=inner_bb.Min.y && pos1.y<=inner_bb.Max.y)
                {
                    // Draw this curve segment
                    window->DrawList->AddLine(pos0, pos1,curveCol,curveThick);
                }

                if (isItemHovered && h==0 && v_hovered==-1 && ImGuiPlotMultiArrayGetterData::IsMouseBetweenXValues(pos0.x,pos1.x,&mouseHoverDeltaX)) {
                    v_hovered = v_idx;
                    hoveredValueXInBBCoords = pos0.x+mouseHoverDeltaX;
                    hoveredValue.x = xRange.x+(hoveredValueXInBBCoords - inner_bb.Min.x)*rangeExtension.x/inner_bb_extension.x;
                }

                if (v_hovered == v_idx) {
                    const float value = values_getter(data, hoveredValue.x, h);
                    float deltaYMouse = inner_bb.Min.y + (yRange.y-value)*inner_bb_extension.y/rangeExtension.y - ImGui::GetIO().MousePos.y;
                    if (deltaYMouse<0) deltaYMouse=-deltaYMouse;
                    if (deltaYMouse<minDistanceValue) {
                        minDistanceValue = deltaYMouse;

                        h_hovered = h;
                        hoveredValue.y = value;
                    }
                }


                // Mandatory
                lastYValue = yValue;
                pos0 = pos1;
                ++v_idx;
            }
        }


        if (v_hovered>=0 && h_hovered>=0)   {
            if (pOptionalHoveredValueOut) *pOptionalHoveredValueOut=hoveredValue;

	    // Tooltip on hover
	    if (num_curves==1)	SetTooltip("P (%1.4f , %1.4f)", hoveredValue.x, hoveredValue.y);
	    else		SetTooltip("P%d (%1.4f , %1.4f)",h_hovered, hoveredValue.x, hoveredValue.y);

            const float circleRadius = curveThick*3.f;
            const float hoveredValueYInBBCoords = inner_bb.Min.y+(yRange.y-hoveredValue.y)*inner_bb_extension.y/rangeExtension.y;

            // We must draw a circle in (hoveredValueXInBBCoords,hoveredValueYInBBCoords)
	    if (hoveredValueYInBBCoords>=inner_bb.Min.y && hoveredValueYInBBCoords<=inner_bb.Max.y) {
                const int col_num = h_hovered%num_colors;
                curveCol = col_base[col_num];

                window->DrawList->AddCircle(ImVec2(hoveredValueXInBBCoords,hoveredValueYInBBCoords),circleRadius,curveCol,12,curveThick);
            }

        }

    }



    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f,0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    return h_hovered;
}
// End PlotCurve(...) implementation --------------------------------------

}   // ImGui namespace


// Tree view stuff starts here ==============================================================
#include <stdlib.h> // qsort (Maybe we could add a define to exclude sorting...)
namespace ImGui {

struct MyTreeViewHelperStruct {
    TreeView& parentTreeView;
    bool mustDrawAllNodesAsDisabled;
    ImGuiWindow* window;
    float windowWidth;
    float arrowOffset;
    TreeViewNode::Event& event;
    bool hasCbGlyphs,hasArrowGlyphs;
    struct ContextMenuDataStruct {
        TreeViewNode* activeNode;
        TreeView* parentTreeView;
        bool activeNodeChanged;
        ContextMenuDataStruct() : activeNode(NULL),parentTreeView(NULL),activeNodeChanged(false) {}
        void reset() {*this = ContextMenuDataStruct();}
    };
    static ContextMenuDataStruct ContextMenuData;
    struct NameEditingStruct {
        TreeViewNode* editingNode;
        char textInput[256];
        bool mustFocusInputText;
        NameEditingStruct(TreeViewNode* n=NULL,const char* startingText=NULL) : editingNode(n) {
            textInput[0]='\0';
            if (startingText) {
                const int len = strlen(startingText);
                if (len<255) strcpy(&textInput[0],startingText);
                else {
                    strncpy(&textInput[0],startingText,255);
                    textInput[255]='\0';
                }
            }
            mustFocusInputText = (editingNode!=NULL);
        }
        void reset() {*this = NameEditingStruct();}
    };
    static NameEditingStruct NameEditing;
    MyTreeViewHelperStruct(TreeView& parent,TreeViewNode::Event& _event) : parentTreeView(parent),mustDrawAllNodesAsDisabled(false),event(_event) {
        window = ImGui::GetCurrentWindow();
        windowWidth = ImGui::GetWindowWidth();
        hasCbGlyphs=TreeView::FontCheckBoxGlyphs[0][0]!='\0';
        hasArrowGlyphs=TreeView::FontArrowGlyphs[0][0]!='\0';
        arrowOffset = hasArrowGlyphs ? (ImGui::CalcTextSize(&TreeView::FontArrowGlyphs[0][0]).x+GImGui->Style.ItemSpacing.x) : (GImGui->FontSize+GImGui->Style.ItemSpacing.x);
    }
    void fillEvent(TreeViewNode* _eventNode=NULL,TreeViewNode::State _eventFlag=TreeViewNode::STATE_NONE,bool _eventFlagRemoved=false) {
        event.node = _eventNode;
        event.state = _eventFlag;
        event.wasStateRemoved = _eventFlagRemoved;
        if (event.state!=TreeViewNode::STATE_NONE) event.type = TreeViewNode::EVENT_STATE_CHANGED;
    }
    // Sorters
    inline static int SorterByDisplayName(const void *pn1, const void *pn2)  {
        const char* s1 = (*((const TreeViewNode**)pn1))->data.displayName;
        const char* s2 = (*((const TreeViewNode**)pn2))->data.displayName;
        return strcmp(s1,s2);   // Hp) displayName can't be NULL
    }
    inline static int SorterByDisplayNameReverseOrder(const void *pn1, const void *pn2)  {
        const char* s2 = (*((const TreeViewNode**)pn1))->data.displayName;
        const char* s1 = (*((const TreeViewNode**)pn2))->data.displayName;
        return strcmp(s1,s2);   // Hp) displayName can't be NULL
    }
    inline static int SorterByTooltip(const void *pn1, const void *pn2)  {
        const char* s1 = (*((const TreeViewNode**)pn1))->data.tooltip;
        const char* s2 = (*((const TreeViewNode**)pn2))->data.tooltip;
        return (s1 && s2) ? (strcmp(s1,s2)) : (s1 ? -1 : (s2 ? 1 : -1));
    }
    inline static int SorterByTooltipReverseOrder(const void *pn1, const void *pn2)  {
        const char* s2 = (*((const TreeViewNode**)pn1))->data.tooltip;
        const char* s1 = (*((const TreeViewNode**)pn2))->data.tooltip;
        return (s1 && s2) ? (strcmp(s1,s2)) : (s1 ? -1 : (s2 ? 1 : -1));
    }
    inline static int SorterByUserText(const void *pn1, const void *pn2)  {
        const char* s1 = (*((const TreeViewNode**)pn1))->data.userText;
        const char* s2 = (*((const TreeViewNode**)pn2))->data.userText;
        return (s1 && s2) ? (strcmp(s1,s2)) : (s1 ? -1 : (s2 ? 1 : -1));
    }
    inline static int SorterByUserTextReverseOrder(const void *pn1, const void *pn2)  {
        const char* s2 = (*((const TreeViewNode**)pn1))->data.userText;
        const char* s1 = (*((const TreeViewNode**)pn2))->data.userText;
        return (s1 && s2) ? (strcmp(s1,s2)) : (s1 ? -1 : (s2 ? 1 : -1));
    }
    inline static int SorterByUserId(const void *pn1, const void *pn2)  {
        const TreeViewNode* n1 = *((const TreeViewNode**)pn1);
        const TreeViewNode* n2 = *((const TreeViewNode**)pn2);
        return n1->data.userId-n2->data.userId;
    }
    inline static int SorterByUserIdReverseOrder(const void *pn1, const void *pn2)  {
        const TreeViewNode* n2 = *((const TreeViewNode**)pn1);
        const TreeViewNode* n1 = *((const TreeViewNode**)pn2);
        return n1->data.userId-n2->data.userId;
    }
    // Serialization
#if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
#ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
    static void Serialize(ImGuiHelper::Serializer& s,TreeViewNode* n) {
        if (n->parentNode)  {
            s.saveTextLines(n->data.displayName,"name");
            if (n->data.tooltip) s.saveTextLines(n->data.tooltip,"tooltip");
            if (n->data.userText) s.saveTextLines(n->data.userText,"userText");
            if (n->data.userId) s.save(&n->data.userId,"userId");
            s.save(&n->state,"state");
        }
        else {
            // It's a TreeView: we must save its own data instead
            ImGui::TreeView& tv = *(static_cast<ImGui::TreeView*>(n));
            s.save(ImGui::FT_COLOR,&tv.stateColors[0].x,"stateColors0",4);
            s.save(ImGui::FT_COLOR,&tv.stateColors[1].x,"stateColors1",4);
            s.save(ImGui::FT_COLOR,&tv.stateColors[2].x,"stateColors2",4);
            s.save(ImGui::FT_COLOR,&tv.stateColors[3].x,"stateColors3",4);
            s.save(ImGui::FT_COLOR,&tv.stateColors[4].x,"stateColors4",4);
            s.save(ImGui::FT_COLOR,&tv.stateColors[5].x,"stateColors5",4);
            int tmp = (int) tv.selectionMode;s.save(&tmp,"selectionMode");
            s.save(&tv.allowMultipleSelection,"allowMultipleSelection");
            tmp = (int) tv.checkboxMode;s.save(&tmp,"checkboxMode");
            s.save(&tv.allowAutoCheckboxBehaviour,"allowAutoCheckboxBehaviour");
            s.save(&tv.collapseToLeafNodesAtNodeDepth,"collapseToLeafNodesAtNodeDepth");
            s.save(&tv.inheritDisabledLook,"inheritDisabledLook");
        }
        const int numChildNodes = n->childNodes ? n->childNodes->size() : -1;
        s.save(&numChildNodes,"numChildNodes");

        if (n->childNodes) {
            for (int i=0,isz=n->childNodes->size();i<isz;i++) {
                TreeViewNode* node = (*(n->childNodes))[i];
                Serialize(s,node);
            }
        }
    }
#endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
    struct ParseCallbackStruct {
        TreeViewNode* node;
        int numChildNodes;
    };
    static bool ParseCallback(ImGuiHelper::FieldType /*ft*/,int /*numArrayElements*/,void* pValue,const char* name,void* userPtr)    {
        ParseCallbackStruct& cbs = *((ParseCallbackStruct*)userPtr);
        TreeViewNode* n = cbs.node;
        if (strcmp(name,"name")==0)             ImGuiHelper::StringAppend(n->data.displayName,(const char*)pValue,false);
        else if (strcmp(name,"tooltip")==0)     ImGuiHelper::StringAppend(n->data.tooltip,(const char*)pValue,true);
        else if (strcmp(name,"userText")==0)    ImGuiHelper::StringAppend(n->data.userText,(const char*)pValue,true);
        else if (strcmp(name,"userId")==0)      n->getUserId() = *((int*)pValue);
        else if (strcmp(name,"state")==0)       n->state = *((int*)pValue);
        else if (strcmp(name,"numChildNodes")==0) {cbs.numChildNodes = *((int*)pValue);return true;}
        return false;
    }
    static bool ParseTreeViewDataCallback(ImGuiHelper::FieldType /*ft*/,int /*numArrayElements*/,void* pValue,const char* name,void* userPtr)    {
        TreeView& tv = *((TreeView*)userPtr);
        if      (strcmp(name,"stateColors0")==0)                tv.stateColors[0] = *((ImVec4*)pValue);
        else if (strcmp(name,"stateColors1")==0)                tv.stateColors[1] = *((ImVec4*)pValue);
        else if (strcmp(name,"stateColors2")==0)                tv.stateColors[2] = *((ImVec4*)pValue);
        else if (strcmp(name,"stateColors3")==0)                tv.stateColors[3] = *((ImVec4*)pValue);
        else if (strcmp(name,"stateColors4")==0)                tv.stateColors[4] = *((ImVec4*)pValue);
        else if (strcmp(name,"stateColors5")==0)                tv.stateColors[5] = *((ImVec4*)pValue);
        else if (strcmp(name,"selectionMode")==0)               tv.selectionMode = *((int*)pValue);
        else if (strcmp(name,"allowMultipleSelection")==0)      tv.allowMultipleSelection = *((bool*)pValue);
        else if (strcmp(name,"checkboxMode")==0)                tv.checkboxMode = *((int*)pValue);
        else if (strcmp(name,"allowAutoCheckboxBehaviour")==0)  tv.allowAutoCheckboxBehaviour = *((bool*)pValue);
        else if (strcmp(name,"collapseToLeafNodesAtNodeDepth")==0) tv.collapseToLeafNodesAtNodeDepth = *((int*)pValue);
        else if (strcmp(name,"inheritDisabledLook")==0)         {tv.inheritDisabledLook = *((bool*)pValue);return true;}
        return false;
    }
    static void Deserialize(ImGuiHelper::Deserializer& d,TreeViewNode* n,const char*& amount,TreeView* tv,TreeView::TreeViewNodeCreationDelationCallback callback=NULL)  {
        if (!n->parentNode) {
            // It's a TreeView: we must load its own data
            ImGui::TreeView& tv = *(static_cast<ImGui::TreeView*>(n));
            amount = d.parse(ParseTreeViewDataCallback,(void*)&tv);
        }
        ParseCallbackStruct cbs;cbs.node=n;cbs.numChildNodes=0;
        amount = d.parse(ParseCallback,(void*)&cbs,amount);
        if (cbs.numChildNodes>=0)   {
            if (cbs.numChildNodes==0) n->addEmptyChildNodeVector();
            else {
                for (int i=0;i<cbs.numChildNodes;i++) {
                    TreeViewNode* node = TreeViewNode::CreateNode(TreeViewNode::Data(),n);
                    Deserialize(d,node,amount,tv,callback);
                    if (callback) callback(node,*tv,false,tv->treeViewNodeCreationDelationCbUserPtr);
                }
            }
        }
    }
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#endif //NO_IMGUIHELPER_SERIALIZATION


    // Default event callbacks
    static void TreeViewNodePopupMenuProvider(TreeViewNode* n,TreeView& tv,void*) {

        // Actually all the nodes can be set to default or be enabled/disabled, but it's just a test...
        const bool canBeSetToDefault = n->isRootNode() && n->isStateMissing(TreeViewNode::STATE_DEFAULT);
        const bool canBeEnabledOrDisabled = true;
        const bool itsChildNodesAreSortable = n->childNodes && n->childNodes->size()>1;
        const bool canBeRenamed = !n->isInRenamingMode();
        const int nodeIndex = n->getNodeIndex();
        const int numSiblingsWithMe = n->getNumSiblings(true);
        const bool canMoveUp = numSiblingsWithMe>1 && nodeIndex>0;
        const bool canMoveDown = numSiblingsWithMe>1 && nodeIndex<(numSiblingsWithMe-1);
        const bool canAddSiblings = true;
        const bool canAddChildNodes = true;
        const bool canDeleteNode = true;
	const bool canForceCheckboxVisibility = tv.checkboxMode!=TreeViewNode::MODE_ALL;
	const bool canBeHidden = true;

        if ((canBeSetToDefault || canBeEnabledOrDisabled || itsChildNodesAreSortable
             || canBeRenamed || canMoveUp || canMoveDown
	     || canAddSiblings || canAddChildNodes || canDeleteNode
	     || canForceCheckboxVisibility || canBeHidden)
                && ImGui::BeginPopup(TreeView::GetTreeViewNodePopupMenuName()))   {
            ImGui::PushID(n);
            if (canBeSetToDefault && ImGui::MenuItem("Set as default root item"))  {
                tv.removeStateFromAllDescendants(TreeViewNode::STATE_DEFAULT);  // allow (at most) one default node
                n->addState(TreeViewNode::STATE_DEFAULT);
            }
            if (canBeEnabledOrDisabled) {
                if (n->isStatePresent(TreeViewNode::STATE_DISABLED))  {
                    if (ImGui::MenuItem("Enable item")) n->removeState(TreeViewNode::STATE_DISABLED);
                }
                else if (ImGui::MenuItem("Disable item")) n->addState(TreeViewNode::STATE_DISABLED);
            }
            if (itsChildNodesAreSortable)   {
                if (ImGui::MenuItem("Sort child nodes in acending order")) n->sortChildNodesByDisplayName(false,false);
                if (ImGui::MenuItem("Sort child nodes in descending order")) n->sortChildNodesByDisplayName(false,true);
            }
            if (canBeRenamed && ImGui::MenuItem("Rename")) {n->startRenamingMode();}
            if (canMoveUp && ImGui::MenuItem("Move up")) n->moveNodeTo(nodeIndex-1);
            if (canMoveDown && ImGui::MenuItem("Move down")) n->moveNodeTo(nodeIndex+1);
            if (canAddSiblings && ImGui::MenuItem("Create new sibling node")) {
                TreeViewNode* n2 = n->addSiblingNode(TreeViewNode::Data("New node"));
                n2->startRenamingMode();
            }
            if (canAddChildNodes && ImGui::MenuItem("Create new child node")) {
                n->addState(TreeViewNode::STATE_OPEN);
                TreeViewNode* n2 = n->addChildNode(TreeViewNode::Data("New node"));
                n2->startRenamingMode();
            }
            if (canDeleteNode && ImGui::MenuItem("Delete")) {TreeViewNode::DeleteNode(n);n=NULL;}
	    if (canForceCheckboxVisibility && ImGui::MenuItem("Toggle force checkbox")) n->toggleState(TreeViewNode::STATE_FORCE_CHECKBOX);
	    if (canBeHidden && ImGui::MenuItem("Hide")) n->addState(TreeViewNode::STATE_HIDDEN);
	    ImGui::PopID();
            ImGui::EndPopup();
        }

    }
};
MyTreeViewHelperStruct::ContextMenuDataStruct MyTreeViewHelperStruct::ContextMenuData;
MyTreeViewHelperStruct::NameEditingStruct MyTreeViewHelperStruct::NameEditing;

TreeViewNode::~TreeViewNode() {
    if (this == MyTreeViewHelperStruct::ContextMenuData.activeNode) MyTreeViewHelperStruct::ContextMenuData.reset();
    if (this == MyTreeViewHelperStruct::NameEditing.editingNode) MyTreeViewHelperStruct::NameEditing.reset();
    //dbgDisplay();
    if (childNodes) {
        // delete child nodes
        //for (int i=0,isz=childNodes->size();i<isz;i++)    // Wrong !!
        while (childNodes->size()>0)                        // Right !!
        {
	    TreeViewNode* n = (*childNodes)[0];
            if (n) {
		n->~TreeViewNode();
                ImGui::MemFree(n);
            }
        }
        childNodes->clear();
        childNodes->~ImVector<TreeViewNode*>();
        ImGui::MemFree(childNodes);
        childNodes=NULL;
    }
    if (parentNode && parentNode->childNodes) {
        // remove this node from parentNode->childNodes
        for (int i=0,isz=parentNode->childNodes->size();i<isz;i++) {
	    TreeViewNode* n = (*(parentNode->childNodes))[i];
            if (n==this) {
                for (int j=i+1;j<isz;j++) (*(parentNode->childNodes))[j-1] = (*(parentNode->childNodes))[j];
                parentNode->childNodes->pop_back();
                break;
            }
        }
    }
    if (parentNode) {
        TreeView& tv = getTreeView();
        if (tv.treeViewNodeCreationDelationCb) tv.treeViewNodeCreationDelationCb(this,tv,true,tv.treeViewNodeCreationDelationCbUserPtr);
    }
    parentNode = NULL;
}

TreeViewNode::TreeViewNode(const Data& _data, TreeViewNode *_parentNode, int nodeIndex, bool addEmptyChildNodeVector) : userPtr(NULL),data(_data) {
    parentNode = NULL;
    childNodes = NULL;
    state = 0;
    parentNode = _parentNode;
    if (parentNode) {
        //We must add this node to parentNode->childNodes
        if (!parentNode->childNodes)    {
            parentNode->childNodes = (ImVector<TreeViewNode*>*) ImGui::MemAlloc(sizeof(ImVector<TreeViewNode*>));
            IM_PLACEMENT_NEW(parentNode->childNodes) ImVector<TreeViewNode*>();
            parentNode->childNodes->push_back(this);
        }
        else if (nodeIndex<0 || nodeIndex>=parentNode->childNodes->size()) {
            // append at the end
            parentNode->childNodes->push_back(this);
        }
        else {
            // insert "this" at "nodeIndex"
            const int isz = parentNode->childNodes->size();
            parentNode->childNodes->resize(parentNode->childNodes->size()+1);
            for (int j=isz;j>nodeIndex;j--) (*(parentNode->childNodes))[j] = (*(parentNode->childNodes))[j-1];
            (*(parentNode->childNodes))[nodeIndex] = this;
        }
    }
    if (addEmptyChildNodeVector) {
        childNodes = (ImVector<TreeViewNode*>*) ImGui::MemAlloc(sizeof(ImVector<TreeViewNode*>));
        IM_PLACEMENT_NEW(childNodes) ImVector<TreeViewNode*>();
    }
    if (parentNode) {
        TreeView& tv = getTreeView();
        if (tv.treeViewNodeCreationDelationCb) tv.treeViewNodeCreationDelationCb(this,tv,false,tv.treeViewNodeCreationDelationCbUserPtr);
    }
}


TreeViewNode *TreeViewNode::CreateNode(const Data& _data, TreeViewNode *_parentNode, int nodeIndex, bool addEmptyChildNodeVector) {
    TreeViewNode* n = (TreeViewNode*) ImGui::MemAlloc(sizeof(TreeViewNode));
    IM_PLACEMENT_NEW(n) TreeViewNode(_data,_parentNode,nodeIndex,addEmptyChildNodeVector);
    return n;
}

void TreeViewNode::DeleteNode(TreeViewNode *n) {
    if (n)  {
        n->~TreeViewNode();
        ImGui::MemFree(n);
    }
}

TreeView &TreeViewNode::getTreeView() {
    TreeViewNode* n = this;
    while (n->parentNode) n=n->parentNode;
    IM_ASSERT(n);
    return *(static_cast < TreeView* > (n));
}

const TreeView &TreeViewNode::getTreeView() const {
    const TreeViewNode* n = this;
    while (n->parentNode) n=n->parentNode;
    IM_ASSERT(n);
    return *(static_cast < const TreeView* > (n));
}

TreeViewNode *TreeViewNode::getParentNode() {return (parentNode && parentNode->parentNode) ? parentNode : NULL;}

const TreeViewNode *TreeViewNode::getParentNode() const {return (parentNode && parentNode->parentNode) ? parentNode : NULL;}

int TreeViewNode::getNodeIndex() const   {
    if (!parentNode || !parentNode->childNodes) return 0;
    for (int i=0,isz=parentNode->childNodes->size();i<isz;i++)  {
        if ( (*parentNode->childNodes)[i] == this ) return i;
    }
    return 0;
}

void TreeViewNode::moveNodeTo(int nodeIndex)   {
    if (!parentNode || !parentNode->childNodes) return;
    const int isz = parentNode->childNodes->size();
    if (isz<2) return;
    if (nodeIndex<0 || nodeIndex>=isz) nodeIndex = isz-1;
    const int curNodeIndex = getNodeIndex();
    if (curNodeIndex==nodeIndex) return;
    if (curNodeIndex<nodeIndex) {
        for (int i=curNodeIndex;i<nodeIndex;i++)  (*parentNode->childNodes)[i] = (*parentNode->childNodes)[i+1];
    }
    else {
        for (int i=curNodeIndex;i>nodeIndex;i--)  (*parentNode->childNodes)[i] = (*parentNode->childNodes)[i-1];
    }
    (*parentNode->childNodes)[nodeIndex] = this;
}
void TreeViewNode::deleteAllChildNodes(bool leaveEmptyChildNodeVector)  {
    if (childNodes && childNodes->size()>0) {
        while (childNodes->size()>0)    DeleteNode((*childNodes)[0]);
        childNodes->clear();
    }
    if (!childNodes) {
        if (leaveEmptyChildNodeVector) addEmptyChildNodeVector();
    }
    else if (childNodes->size()==0 && !leaveEmptyChildNodeVector) removeEmptyChildNodeVector();
}
void TreeViewNode::addEmptyChildNodeVector()    {
    if (!childNodes) {
        childNodes = (ImVector<TreeViewNode*>*) ImGui::MemAlloc(sizeof(ImVector<TreeViewNode*>));
        IM_PLACEMENT_NEW(childNodes) ImVector<TreeViewNode*>();
    }
}
void TreeViewNode::removeEmptyChildNodeVector() {
    if (childNodes && childNodes->size()==0)    {
        childNodes->~ImVector<TreeViewNode*>();
        ImGui::MemFree(childNodes);
        childNodes=NULL;
    }
}
int TreeViewNode::getNumSiblings(bool includeMe) const	{
    if (!parentNode) return (includeMe ? 1 : 0);
    const int num = parentNode->getNumChildNodes();
    return (includeMe ? num : (num-1));
}

TreeViewNode *TreeViewNode::getSiblingNode(int nodeIndexInParentHierarchy)  {
    if (!parentNode || !parentNode->childNodes || nodeIndexInParentHierarchy<0 || nodeIndexInParentHierarchy>=parentNode->childNodes->size()) return NULL;
    return (*parentNode->childNodes)[nodeIndexInParentHierarchy];
}
const TreeViewNode *TreeViewNode::getSiblingNode(int nodeIndexInParentHierarchy) const	{
    if (!parentNode || !parentNode->childNodes || nodeIndexInParentHierarchy<0 || nodeIndexInParentHierarchy>=parentNode->childNodes->size()) return NULL;
    return (*parentNode->childNodes)[nodeIndexInParentHierarchy];
}
int TreeViewNode::getDepth() const  {
    const TreeViewNode* n = this;int depth = -1;
    while ((n = n->parentNode)) ++depth;
    return depth;
}

void TreeViewNode::Swap(TreeViewNode *&n1, TreeViewNode *&n2) {
    // To test
    if (!n1 || !n2) return;
    {TreeViewNode* tmp = n1->parentNode;n1->parentNode=n2->parentNode;n2->parentNode = tmp;}
    {ImVector<TreeViewNode*>* tmp = n1->childNodes;n1->childNodes=n2->childNodes;n2->childNodes=tmp;}
}

void TreeViewNode::sortChildNodes(bool recursive,int (*comp)(const void *, const void *)) {
    if (childNodes && childNodes->size()>1)  {
        if (recursive) {
            for (int i=0,isz=childNodes->size();i<isz;i++) (*childNodes)[i]->sortChildNodes(recursive,comp);
        }
        qsort(&(*childNodes)[0],childNodes->size(),sizeof(TreeViewNode*),comp);
    }
}
void TreeViewNode::sortChildNodesByDisplayName(bool recursive, bool reverseOrder)    {
    sortChildNodes(recursive,reverseOrder ? MyTreeViewHelperStruct::SorterByDisplayNameReverseOrder : &MyTreeViewHelperStruct::SorterByDisplayName);
}
void TreeViewNode::sortChildNodesByTooltip(bool recursive,bool reverseOrder)  {
    sortChildNodes(recursive,reverseOrder ? MyTreeViewHelperStruct::SorterByTooltipReverseOrder : MyTreeViewHelperStruct::SorterByTooltip);
}
void TreeViewNode::sortChildNodesByUserText(bool recursive, bool reverseOrder) {
    sortChildNodes(recursive,reverseOrder ? MyTreeViewHelperStruct::SorterByUserTextReverseOrder : MyTreeViewHelperStruct::SorterByUserText);
}
void TreeViewNode::sortChildNodesByUserId(bool recursive,bool reverseOrder)   {
    sortChildNodes(recursive,reverseOrder ? MyTreeViewHelperStruct::SorterByUserIdReverseOrder : MyTreeViewHelperStruct::SorterByUserId);
}

void TreeViewNode::addStateToAllChildNodes(int stateFlag,bool recursive) const {
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (recursive) (*childNodes)[i]->addStateToAllChildNodes(stateFlag,recursive);
            (*childNodes)[i]->state|=stateFlag;
        }
    }
}
void TreeViewNode::removeStateFromAllChildNodes(int stateFlag,bool recursive) const   {
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (recursive) (*childNodes)[i]->removeStateFromAllChildNodes(stateFlag,recursive);
            (*childNodes)[i]->state&=(~stateFlag);
        }
    }
}

bool TreeViewNode::isStatePresentInAllChildNodes(int stateFlag) const {
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (((*childNodes)[i]->state&stateFlag)!=stateFlag) return false;
        }
    }
    return true;
}
bool TreeViewNode::isStateMissingInAllChildNodes(int stateFlag) const {
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (((*childNodes)[i]->state&stateFlag)==stateFlag) return false;
        }
    }
    return true;
}
bool TreeViewNode::isStatePresentInAllDescendants(int stateFlag) const{
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (((*childNodes)[i]->state&stateFlag)!=stateFlag || !(*childNodes)[i]->isStatePresentInAllDescendants(stateFlag)) return false;
        }
    }
    return true;
}
bool TreeViewNode::isStateMissingInAllDescendants(int stateFlag) const{
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)  {
            if (((*childNodes)[i]->state&stateFlag)==stateFlag || !(*childNodes)[i]->isStateMissingInAllDescendants(stateFlag)) return false;
        }
    }
    return true;
}
TreeViewNode *TreeViewNode::getFirstParentNodeWithState(int stateFlag,bool recursive)  {
    TreeViewNode* n = parentNode;
    while (n && n->parentNode) {if ((n->state&stateFlag)==stateFlag) return n;if (!recursive) break;n=n->parentNode;}
    return NULL;
}
const TreeViewNode *TreeViewNode::getFirstParentNodeWithState(int stateFlag,bool recursive) const  {
    const TreeViewNode* n = parentNode;
    while (n && n->parentNode) {if ((n->state&stateFlag)==stateFlag) return n;if (!recursive) break;n=n->parentNode;}
    return NULL;
}
TreeViewNode *TreeViewNode::getFirstParentNodeWithoutState(int stateFlag,bool recursive)  {
    TreeViewNode* n = parentNode;
    while (n && n->parentNode) {if ((n->state&stateFlag)!=stateFlag) return n;if (!recursive) break;n=n->parentNode;}
    return NULL;
}
const TreeViewNode *TreeViewNode::getFirstParentNodeWithoutState(int stateFlag,bool recursive) const  {
    const TreeViewNode* n = parentNode;
    while (n && n->parentNode) {if ((n->state&stateFlag)!=stateFlag) return n;if (!recursive) break;n=n->parentNode;}
    return NULL;
}
void TreeViewNode::getAllChildNodesWithState(ImVector<TreeViewNode *> &result, int stateFlag, bool recursive,bool returnOnlyLeafNodes, bool clearResultBeforeUsage) const  {
    if (clearResultBeforeUsage) result.clear();
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)   {
            const TreeViewNode* n = (*childNodes)[i];
            if ((n->state&stateFlag)==stateFlag && (!returnOnlyLeafNodes || n->isLeafNode())) result.push_back(const_cast<TreeViewNode*>(n));
            if (recursive) n->getAllChildNodesWithState(result,stateFlag,recursive,returnOnlyLeafNodes,false);
        }
    }
}
void TreeViewNode::getAllChildNodesWithoutState(ImVector<TreeViewNode *> &result, int stateFlag, bool recursive, bool returnOnlyLeafNodes, bool clearResultBeforeUsage) const   {
    if (clearResultBeforeUsage) result.clear();
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)   {
            const TreeViewNode* n = (*childNodes)[i];
            if ((n->state&stateFlag)!=stateFlag && (!returnOnlyLeafNodes || n->isLeafNode())) result.push_back(const_cast<TreeViewNode*>(n));
            if (recursive) n->getAllChildNodesWithState(result,stateFlag,recursive,returnOnlyLeafNodes,false);
        }
    }
}
void TreeViewNode::getAllChildNodes(ImVector<TreeViewNode *> &result, bool recursive, bool returnOnlyLeafNodes, bool clearResultBeforeUsage) const  {
    if (clearResultBeforeUsage) result.clear();
    if (childNodes) {
        for (int i=0,isz=childNodes->size();i<isz;i++)   {
            const TreeViewNode* n = (*childNodes)[i];
            if (!returnOnlyLeafNodes || n->isLeafNode()) result.push_back(const_cast<TreeViewNode*>(n));
            if (recursive) n->getAllChildNodes(result,recursive,returnOnlyLeafNodes,false);
        }
    }
}

void TreeViewNode::startRenamingMode()  {
    MyTreeViewHelperStruct::NameEditing = MyTreeViewHelperStruct::NameEditingStruct(this,this->getDisplayName());
}
bool TreeViewNode::isInRenamingMode() const {
    return (this==MyTreeViewHelperStruct::NameEditing.editingNode);
}


TreeView::TreeView(Mode _selectionMode, bool _allowMultipleSelection, Mode _checkboxMode, bool _allowAutoCheckboxBehaviour, bool _inheritDisabledLook) : TreeViewNode(Data(),NULL,-1,true) {
    treeViewNodePopupMenuDrawerCb = &MyTreeViewHelperStruct::TreeViewNodePopupMenuProvider;
    treeViewNodePopupMenuDrawerCbUserPtr = NULL;
    treeViewNodeDrawIconCb = NULL;
    treeViewNodeDrawIconCbUserPtr = NULL;
    treeViewNodeAfterDrawCb = NULL;
    treeViewNodeAfterDrawCbUserPtr = NULL;
    treeViewNodeCreationDelationCb = NULL;
    treeViewNodeCreationDelationCbUserPtr = NULL;
    inited = false;

    selectionMode = _selectionMode;
    allowMultipleSelection = _allowMultipleSelection;

    checkboxMode = _checkboxMode;
    allowAutoCheckboxBehaviour = _allowAutoCheckboxBehaviour;

    inheritDisabledLook = _inheritDisabledLook;
    userPtr = NULL;

    collapseToLeafNodesAtNodeDepth = -1;

    stateColors[0] = ImVec4(1.f,0.f,0.f,1.f);stateColors[1] = stateColors[0];stateColors[1].w*=0.4f;
    stateColors[2] = ImVec4(0.f,1.f,0.f,1.f);stateColors[3] = stateColors[2];stateColors[3].w*=0.4f;
    stateColors[4] = ImVec4(0.f,0.f,1.f,1.f);stateColors[5] = stateColors[4];stateColors[5].w*=0.4f;

}

TreeView::~TreeView() {
    if (this == MyTreeViewHelperStruct::ContextMenuData.parentTreeView) MyTreeViewHelperStruct::ContextMenuData.reset();
}


void TreeViewNode::render(void* ptr,int numIndents)   {
    // basically it should be: numIndents == getDepth() + 1; AFAICS
    if (state&STATE_HIDDEN) return;
    MyTreeViewHelperStruct& tvhs = *((MyTreeViewHelperStruct*) ptr);
    TreeView& tv = tvhs.parentTreeView;

    bool mustShowMenu = false;
    bool isLeafNode = !childNodes;

    bool mustSkipToLeafNodes = !isLeafNode && tv.collapseToLeafNodesAtNodeDepth>=0 && numIndents-1>=tv.collapseToLeafNodesAtNodeDepth;

    if (!mustSkipToLeafNodes)   {
        bool mustTreePop = false;
        bool mustTriggerSelection = false;
        bool arrowHovered = false;
        bool itemHovered = false;

        const unsigned int mode = getMode();
        const bool allowCheckBox = (state&STATE_FORCE_CHECKBOX) || MatchMode(tv.checkboxMode,mode);
        const bool allowSelection = MatchMode(tv.selectionMode,mode);

        bool stateopen = (state&STATE_OPEN);
        bool stateselected = (state&STATE_SELECTED);
        bool mustdrawdisabled = (state&STATE_DISABLED) || tvhs.mustDrawAllNodesAsDisabled;

        int customColorState = (state&STATE_COLOR1) ? 1 : (state&STATE_COLOR2) ? 2 : (state&STATE_COLOR3) ? 3 : 0;

        ImGui::PushID(this);
        if (allowCheckBox && !tvhs.hasCbGlyphs) ImGui::AlignFirstTextHeightToWidgets();

        tvhs.window->DC.CursorPos.x+= tvhs.arrowOffset*(numIndents-(isLeafNode ? 0 : 1))+(tvhs.hasArrowGlyphs?(GImGui->Style.FramePadding.x*2):0.f);
        if (!isLeafNode) {
            if (!tvhs.hasArrowGlyphs)  {
                ImGui::SetNextTreeNodeOpen(stateopen,ImGuiSetCond_Always);
                mustTreePop = ImGui::TreeNode("","%s","");
            }
            else {
                ImGui::Text("%s",stateopen?&TreeView::FontArrowGlyphs[1][0]:&TreeView::FontArrowGlyphs[0][0]);
                arrowHovered=ImGui::IsItemHovered();
            }
            arrowHovered=ImGui::IsItemHovered();itemHovered|=arrowHovered;
            ImGui::SameLine();
        }

        if (allowCheckBox)  {
            bool statechecked = (state&STATE_CHECKED);

            bool checkedChanged = false;
            if (!tvhs.hasCbGlyphs) checkedChanged = ImGui::Checkbox("###chb",&statechecked);
            else {
                ImGui::Text("%s",statechecked?&TreeView::FontCheckBoxGlyphs[1][0]:&TreeView::FontCheckBoxGlyphs[0][0]);
                checkedChanged = ImGui::GetIO().MouseClicked[0] && ImGui::IsItemHovered();
                if (checkedChanged) statechecked=!statechecked;
            }

            if (checkedChanged)   {
                toggleState(STATE_CHECKED);
                tvhs.fillEvent(this,STATE_CHECKED,!(state&STATE_CHECKED));
                if (tv.allowAutoCheckboxBehaviour)  {
                    if (childNodes && childNodes->size()>0)    {
                        if (statechecked) addStateToAllDescendants(STATE_CHECKED);
                        else if (isStatePresentInAllDescendants(STATE_CHECKED)) removeStateFromAllDescendants(STATE_CHECKED);
                    }
                    TreeViewNode* p = parentNode;
                    while (p && p->parentNode!=NULL) {
                        if (!statechecked) p->removeState(STATE_CHECKED);
                        else if (!(p->state&STATE_CHECKED)) {
                            if (p->isStatePresentInAllDescendants(STATE_CHECKED)) p->addState(STATE_CHECKED);
                        }
                        p = p->parentNode;
                    }
                }
                //if (allowSelection) mustTriggerSelection = true;  // Nope, why? ... and actually I fire a single state event at a time, so I can't do it.
            }
            ImGui::SameLine();
        }

        if (tv.treeViewNodeDrawIconCb) itemHovered|=tv.treeViewNodeDrawIconCb(this,tv,tv.treeViewNodeDrawIconCbUserPtr);

        if (this!=MyTreeViewHelperStruct::NameEditing.editingNode)  {
            if (mustdrawdisabled) {
                if (!customColorState) ImGui::PushStyleColor(ImGuiCol_Text, GImGui->Style.Colors[ImGuiCol_TextDisabled]);
                else ImGui::PushStyleColor(ImGuiCol_Text, tv.stateColors[(customColorState-1)*2+1]);
            }
            if ((state&STATE_DEFAULT) || (allowSelection && stateselected)) {
                const ImVec4 textColor = (!customColorState || mustdrawdisabled) ? GImGui->Style.Colors[ImGuiCol_Text] : tv.stateColors[(customColorState-1)*2];
                ImVec2 shadowOffset(0,allowCheckBox ? (GImGui->Style.FramePadding.y*2) : 0);
                float textSizeY = 0.f;
                if (allowSelection && stateselected)	{
                    const ImVec2 textSize = ImGui::CalcTextSize(data.displayName);textSizeY=textSize.y;
                    const ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(textColor.x,textColor.y,textColor.z,textColor.w*0.1f));
                    tvhs.window->DrawList->AddRectFilled(tvhs.window->DC.CursorPos+shadowOffset,tvhs.window->DC.CursorPos+textSize,fillColor);
                    const ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(textColor.x,textColor.y,textColor.z,textColor.w*0.15f));
                    tvhs.window->DrawList->AddRect(tvhs.window->DC.CursorPos+shadowOffset,tvhs.window->DC.CursorPos+textSize,borderColor,0.0f,0x0F,textSize.y*0.1f);
                }
                if (state&STATE_DEFAULT)    {
                    shadowOffset.y*=0.5f;
                    shadowOffset.x = (textSizeY!=0.f ? textSizeY : ImGui::GetTextLineHeight())*0.05f;
                    if (shadowOffset.x<1) shadowOffset.x=1.f;
                    shadowOffset.y+=shadowOffset.x;
                    const ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(textColor.x,textColor.y,textColor.z,textColor.w*0.6f));
                    tvhs.window->DrawList->AddText(tvhs.window->DC.CursorPos+shadowOffset,shadowColor,data.displayName);
                }
            }

            if (!customColorState || mustdrawdisabled) ImGui::Text("%s",data.displayName);
            else ImGui::TextColored(tv.stateColors[(customColorState-1)*2],"%s",data.displayName);

            if (mustdrawdisabled) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                itemHovered=true;
                if (data.tooltip && strlen(data.tooltip)>0) ImGui::SetTooltip("%s",data.tooltip);
            }
        }
        else {
            char* ti = &MyTreeViewHelperStruct::NameEditing.textInput[0];
            bool& mustFocus = MyTreeViewHelperStruct::NameEditing.mustFocusInputText;
            if (mustFocus) {ImGui::SetKeyboardFocusHere();}
            if (ImGui::InputText("###EditingName",ti,255,ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (strlen(ti)>0) {
                    setDisplayName(ti);
                    tvhs.event.node = this;
                    tvhs.event.type = EVENT_RENAMED;
                }
                MyTreeViewHelperStruct::NameEditing.reset();
            }
            else {
                if (!mustFocus && !ImGui::IsItemActive()) MyTreeViewHelperStruct::NameEditing.reset();
                mustFocus = false;
            }
        }
        if (tv.treeViewNodeAfterDrawCb) tv.treeViewNodeAfterDrawCb(this,tv,tvhs.windowWidth,tv.treeViewNodeAfterDrawCbUserPtr);

        if (mustTreePop) ImGui::TreePop();
        ImGui::PopID();

        if (arrowHovered)   {
            if (ImGui::GetIO().MouseClicked[0] && childNodes) {
                toggleState(STATE_OPEN);
                tvhs.fillEvent(this,STATE_OPEN,!(state&STATE_OPEN));
            }
        }
        else if (itemHovered) {
            if (ImGui::GetIO().MouseDoubleClicked[0])		{
                if (allowSelection) mustTriggerSelection = true;
                tvhs.event.node = this;
                tvhs.event.type = EVENT_DOUBLE_CLICKED;
            }
            else if (ImGui::GetIO().MouseClicked[0])		{
                if (allowSelection) mustTriggerSelection = true;
                else if (childNodes) {
                    toggleState(STATE_OPEN);
                    tvhs.fillEvent(this,STATE_OPEN,!(state&STATE_OPEN));
                }
            }
            else if (ImGui::GetIO().MouseClicked[1])	{
                mustShowMenu = true;
                if (allowSelection) mustTriggerSelection = true;
            }
        }

        if (mustTriggerSelection) {
            if (!(ImGui::GetIO().KeyCtrl && tv.allowMultipleSelection)) tv.removeStateFromAllDescendants(STATE_SELECTED);
            toggleState(STATE_SELECTED);
            tvhs.fillEvent(this,STATE_SELECTED,!(state&STATE_SELECTED));
        }

    }

    if (isStatePresent(STATE_OPEN) || mustSkipToLeafNodes)  {
        //---------------------------------------------------
        for (int i=0,isz=childNodes->size();i<isz;i++)   {
            TreeViewNode* n = (*childNodes)[i];
            if (n) {
                const bool oldMustDrawAllNodesAsDisabled = tvhs.mustDrawAllNodesAsDisabled;
                if (tv.inheritDisabledLook && (state&STATE_DISABLED)) tvhs.mustDrawAllNodesAsDisabled = true;
                n->render(&tvhs,numIndents+(mustSkipToLeafNodes?0:1));
                tvhs.mustDrawAllNodesAsDisabled = oldMustDrawAllNodesAsDisabled;
            }
        }
        //---------------------------------------------------
    }


    if (mustShowMenu) {
        if (tv.treeViewNodePopupMenuDrawerCb)	{
            tvhs.ContextMenuData.activeNode = this;
            tvhs.ContextMenuData.parentTreeView = &tv;
            tvhs.ContextMenuData.activeNodeChanged = true;
        }
    }

}


bool TreeView::render() {
    inited = true;
    lastEvent.reset();
    if (!childNodes || childNodes->size()==0) return false;

    static int frameCnt = -1;
    ImGuiContext& g = *GImGui;
    if (frameCnt!=g.FrameCount) {
        frameCnt=g.FrameCount;
        MyTreeViewHelperStruct::ContextMenuDataStruct& cmd = MyTreeViewHelperStruct::ContextMenuData;
        // Display Tab Menu ------------------------------------------
        if (cmd.activeNode && cmd.parentTreeView && cmd.parentTreeView->treeViewNodePopupMenuDrawerCb) {
            if (cmd.activeNodeChanged) {
                cmd.activeNodeChanged = false;
                ImGuiContext& g = *GImGui; while (g.OpenPopupStack.size() > 0) g.OpenPopupStack.pop_back();   // Close all existing context-menus
                ImGui::OpenPopup(TreeView::GetTreeViewNodePopupMenuName());
            }
            cmd.parentTreeView->treeViewNodePopupMenuDrawerCb(cmd.activeNode,*cmd.parentTreeView,cmd.parentTreeView->treeViewNodePopupMenuDrawerCbUserPtr);
        }
        // -------------------------------------------------------------
    }

    MyTreeViewHelperStruct tvhs(*this,lastEvent);

    ImGui::BeginGroup();
    ImGui::PushID(this);
    for (int i=0,isz=childNodes->size();i<isz;i++)   {
        TreeViewNode* n = (*childNodes)[i];
        tvhs.mustDrawAllNodesAsDisabled = false;
        if (n) n->render(&tvhs,1);
    }
    ImGui::PopID();
    ImGui::EndGroup();

    // TODO: Move as much as event handling stuff from TreeViewNode::render() here

    return (lastEvent.node!=NULL);
}

void TreeView::clear() {TreeViewNode::deleteAllChildNodes(true);}

ImVec4 *TreeView::getTextColorForStateColor(int aStateColorFlag) const    {
    if (aStateColorFlag&STATE_COLOR1) return &stateColors[0];
    if (aStateColorFlag&STATE_COLOR2) return &stateColors[2];
    if (aStateColorFlag&STATE_COLOR3) return &stateColors[4];
    return NULL;
}
ImVec4 *TreeView::getTextDisabledColorForStateColor(int aStateColorFlag) const    {
    if (aStateColorFlag&STATE_COLOR1) return &stateColors[1];
    if (aStateColorFlag&STATE_COLOR2) return &stateColors[3];
    if (aStateColorFlag&STATE_COLOR3) return &stateColors[5];
    return NULL;
}

void TreeView::setTextColorForStateColor(int aStateColorFlag, const ImVec4 &textColor, float disabledTextColorAlphaFactor) const    {
    if (aStateColorFlag&STATE_COLOR1) {stateColors[0] = textColor; stateColors[1] = stateColors[0]; stateColors[1].w = stateColors[0].w * disabledTextColorAlphaFactor;}
    if (aStateColorFlag&STATE_COLOR2) {stateColors[2] = textColor; stateColors[3] = stateColors[2]; stateColors[3].w = stateColors[2].w * disabledTextColorAlphaFactor;}
    if (aStateColorFlag&STATE_COLOR3) {stateColors[4] = textColor; stateColors[5] = stateColors[4]; stateColors[5].w = stateColors[4].w * disabledTextColorAlphaFactor;}
}

//-------------------------------------------------------------------------------
#       if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
#       ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
        bool TreeView::save(ImGuiHelper::Serializer& s) {
            if (!s.isValid()) return false;
            MyTreeViewHelperStruct::Serialize(s,this);
            return true;
        }
        bool TreeView::save(const char* filename)   {
            ImGuiHelper::Serializer s(filename);
            return save(s);
        }
        bool TreeView::Save(const char* filename,TreeView** pTreeViews,int numTreeviews)    {
            IM_ASSERT(pTreeViews && numTreeviews>0);
            ImGuiHelper::Serializer s(filename);
            bool ok = true;
            for (int i=0;i<numTreeviews;i++)   {
                IM_ASSERT(pTreeViews[i]);
                ok|=pTreeViews[i]->save(s);
            }
            return ok;
        }
#       endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#       ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
        bool TreeView::load(ImGuiHelper::Deserializer& d, const char **pOptionalBufferStart)   {
            if (!d.isValid()) return false;
            clear();
            // We want to call the creation callbacks after Node::Data has been created
            // Our serialization code instead creates empty Nodes before filling Data
            // Here is our workaround/hack:
            TreeView::TreeViewNodeCreationDelationCallback oldCb = treeViewNodeCreationDelationCb;
            treeViewNodeCreationDelationCb = NULL;
            const char* amount = pOptionalBufferStart ? (*pOptionalBufferStart) : 0;
            MyTreeViewHelperStruct::Deserialize(d,this,amount,this,oldCb);
            if (pOptionalBufferStart) *pOptionalBufferStart = amount;
            treeViewNodeCreationDelationCb = oldCb;
            return true;
        }
        bool TreeView::load(const char* filename)   {
            ImGuiHelper::Deserializer d(filename);
            return load(d);
        }
        bool TreeView::Load(const char* filename,TreeView** pTreeViews,int numTreeviews) {
            IM_ASSERT(pTreeViews && numTreeviews>0);
            for (int i=0;i<numTreeviews;i++)   {
                IM_ASSERT(pTreeViews[i]);
                pTreeViews[i]->clear();
            }
            ImGuiHelper::Deserializer d(filename);
            const char* amount = 0; bool ok = true;
            for (int i=0;i<numTreeviews;i++)   {
                ok|=pTreeViews[i]->load(d,&amount);
            }
            return ok;
        }
#       endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
//--------------------------------------------------------------------------------

char TreeView::FontCheckBoxGlyphs[2][5]={{'\0','\0','\0','\0','\0'},{'\0','\0','\0','\0','\0'}};
char TreeView::FontArrowGlyphs[2][5]={{'\0','\0','\0','\0','\0'},{'\0','\0','\0','\0','\0'}};

void TreeView::SetFontCheckBoxGlyphs(const char *emptyState, const char *fillState) {
    if (emptyState && strlen(emptyState)>0 && strlen(emptyState)<5 && fillState && strlen(fillState)>0 && strlen(fillState)<5) {
        strcpy(&FontCheckBoxGlyphs[0][0],emptyState);
        strcpy(&FontCheckBoxGlyphs[1][0],fillState);
    }
    else FontCheckBoxGlyphs[0][0]=FontCheckBoxGlyphs[1][0]='\0';
}
void TreeView::SetFontArrowGlyphs(const char *leftArrow, const char *downArrow) {
    if (leftArrow && strlen(leftArrow)>0 && strlen(leftArrow)<5 && downArrow && strlen(downArrow)>0 && strlen(downArrow)<5) {
        strcpy(&FontArrowGlyphs[0][0],leftArrow);
        strcpy(&FontArrowGlyphs[1][0],downArrow);
    }
    else FontArrowGlyphs[0][0]=FontArrowGlyphs[1][0]='\0';
}


// Tree view stuff ends here ==============================================================





} // namespace ImGui
