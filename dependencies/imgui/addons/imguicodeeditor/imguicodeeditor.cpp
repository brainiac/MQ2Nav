/*
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 This permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/
#include "imguicodeeditor.h"

#define IMGUICODEEDITOR_USE_UTF8HELPER_H    // speed opt ?
#ifdef IMGUICODEEDITOR_USE_UTF8HELPER_H
#   include "utf8helper.h"         // not sure if it's necessary to count UTF8 chars
#endif //IMGUICODEEDITOR_USE_UTF8HELPER_H

#define IMGUI_NEW(type)         IM_PLACEMENT_NEW (ImGui::MemAlloc(sizeof(type) ) ) type
#define IMGUI_DELETE(type, obj) reinterpret_cast<type*>(obj)->~type(), ImGui::MemFree(obj)

namespace ImGui {

namespace CodeEditorDrawListHelper {
// Extensions to ImDrawList:
static void ImDrawListPathFillAndStroke(ImDrawList* dl,const ImU32& fillColor,const ImU32& strokeColor,bool strokeClosed=false, float strokeThickness = 1.0f, bool antiAliased = true)    {
    if (!dl) return;
    if ((fillColor & IM_COL32_A_MASK) != 0) dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor, antiAliased);
    if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness, antiAliased);
    dl->PathClear();
}
static void ImDrawListAddRect(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColor,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = ~0,float strokeThickness = 1.0f,bool antiAliased = true) {
    if (!dl || (((fillColor & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    //dl->AddRectFilled(a,b,fillColor,rounding,rounding_corners);
    //dl->AddRect(a,b,strokeColor,rounding,rounding_corners);
    dl->PathRect(a, b, rounding, rounding_corners);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
static void ImDrawListPathArcTo(ImDrawList* dl,const ImVec2& centre,const ImVec2& radii, float amin, float amax, int num_segments = 10)  {
    if (!dl) return;
    if (radii.x == 0.0f || radii.y==0) dl->_Path.push_back(centre);
    dl->_Path.reserve(dl->_Path.Size + (num_segments + 1));
    for (int i = 0; i <= num_segments; i++)
    {
        const float a = amin + ((float)i / (float)num_segments) * (amax - amin);
        dl->_Path.push_back(ImVec2(centre.x + cosf(a) * radii.x, centre.y + sinf(a) * radii.y));
    }
}
static void ImDrawListAddEllipse(ImDrawList* dl,const ImVec2& centre, const ImVec2& radii,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true)   {
    if (!dl) return;
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
static void ImDrawListAddCircle(ImDrawList* dl,const ImVec2& centre, float radius,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true)   {
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
} //CodeEditorDrawListHelper

static inline int CountUTF8Chars(const char* text_begin, const char* text_end=NULL)   {
#   ifndef IMGUICODEEDITOR_USE_UTF8HELPER_H
    if (!text_end) text_end = text_begin + strlen(text_begin); // FIXME-OPT: Need to avoid this.
    int cnt = 0;const char* s = text_begin;unsigned int c = 0;
    while (s < text_end)    {
        // Decode and advance source
        c = (unsigned int)*s;
        if (c < 0x80)   {s += 1;++cnt;}
        else    {
            s += ImTextCharFromUtf8(&c, s, text_end);           // probably slower than UTF8Helper::decode(...)
            if (c == 0) break;  // Mmmh, not sure about this
            ++cnt;
        }
    }
    return cnt;
#   else // IMGUICODEEDITOR_USE_UTF8HELPER_H
    return UTF8Helper::CountUTF8Chars(text_begin,text_end); // This should be much faster (because UTF8Helper::decode() should be faster than ImTextCharFromUtf8()), but UTF8Helper also checks if a string is malformed, so I don't know.
#   endif //IMGUICODEEDITOR_USE_UTF8HELPER_H
}

static inline float MyCalcTextWidthA(ImFont* font,float size, const char* text_begin, const char* text_end, const char** remaining,int *pNumUTF8CharsOut=NULL,bool cancelOutCharacterSpacingForTheLastCharacterOfALine=false)  {
    // Warning: *pNumUTF8CharsOut must be set to zero by the caller
    const float scale = size / font->FontSize;
    float text_width = 0.f;int numUTF8Chars = 0;
//#   define NO_IMGUICODEEDITOR_USE_OPT_FOR_MONOSPACE_FONTS
#   ifdef NO_IMGUICODEEDITOR_USE_OPT_FOR_MONOSPACE_FONTS
    if (!text_end) text_end = text_begin + strlen(text_begin); // FIXME-OPT: Need to avoid this.
    const char* s = text_begin;
    while (s < text_end)    {
        // Decode and advance source
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)   {s += 1;++numUTF8Chars;}
        else    {
            s += ImTextCharFromUtf8(&c, s, text_end);
            if (c == 0) break;
            ++numUTF8Chars;
        }

        text_width += ((int)c < font->IndexXAdvance.Size ? font->IndexXAdvance[(int)c] : font->FallbackXAdvance) * scale;
    }
    if (remaining)  *remaining = s;
#   else //NO_IMGUICODEEDITOR_USE_OPT_FOR_MONOSPACE_FONTS
#       ifndef IMGUICODEEDITOR_USE_UTF8HELPER_H
    // Actually this does not work corrently with TABS, because TABS have a different width even in MONOSPACE fonts (TO FIX)
    IM_ASSERT(remaining==NULL); // this arg is not currently supported by this opt
    numUTF8Chars = ImGui::CountUTF8Chars(text_begin,text_end);
    text_width = (font->FallbackXAdvance * scale) * numUTF8Chars;   // We use font->FallbackXAdvance for all (we could have used font->IndexXAdvance[0])
#       else //IMGUICODEEDITOR_USE_UTF8HELPER_H
    if (!text_end) text_end = text_begin + strlen(text_begin); // FIXME-OPT: Need to avoid this.
    const unsigned char* s = (const unsigned char*) text_begin;
    const unsigned char* text_endUC = (const unsigned char*) text_end;
    uint32_t codepoint=0, state = 0;const uint32_t tab = (uint32_t)'\t';
    int numTabs = 0;
    for (numUTF8Chars = 0; s!=text_endUC; ++s)    {
        if (UTF8Helper::decode(&state, &codepoint, *s)==UTF8Helper::UTF8_ACCEPT) {
            ++numUTF8Chars;
            if (codepoint == tab) ++numTabs;
        }
    }
    text_width = scale * (font->FallbackXAdvance * (numUTF8Chars-numTabs) + font->IndexXAdvance[tab] * numTabs);
    if (remaining)  *remaining = (const char*) s;
#       endif //IMGUICODEEDITOR_USE_UTF8HELPER_H
#   endif //NO_IMGUICODEEDITOR_USE_OPT_FOR_MONOSPACE_FONTS
    if (pNumUTF8CharsOut) *pNumUTF8CharsOut+=numUTF8Chars;
    if (cancelOutCharacterSpacingForTheLastCharacterOfALine && text_width > 0.0f) text_width -= scale;
    return text_width;
}
static inline float MyCalcTextWidth(const char *text, const char *text_end=NULL, int *pNumUTF8CharsOut=NULL)    {
    // Warning: *pNumUTF8CharsOut must be set to zero by the caller
    return MyCalcTextWidthA(GImGui->Font,GImGui->FontSize,text,text_end,NULL,pNumUTF8CharsOut,true);
}


// Basically these 3 methods are similiar to the ones in ImFont or ImDrawList classes, but:
// -> specialized for text lines (no '\n' and '\r' chars).
// -> furthermore, now "pos" is taken by reference, because before I kept calling: font->AddText(...,pos,text,...);pos+=ImGui::CalcTextSize(text);
//      Now that is done in a single call.
// TODO:    remove all this garbage and just use plain ImGui methods and call ImGui::GetCursorPosX() instead of ImGui::CalcTextSize(...) every time.
//          [I won't get far in this addon if I keep adding useless code...]
// TODO: Do the same for utf8helper.h. Just remove it. I DON'T MIND if it will be slower, the code must be clean and ordered, not fast.
static inline void ImDrawListRenderTextLine(ImDrawList* draw_list,const ImFont* font,float size, ImVec2& pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end, bool cpu_fine_clip)
{
    if (!text_end) text_end = text_begin + strlen(text_begin);

    if ((int)pos.y > clip_rect.w) {
        pos.x+= (int)pos.x + MyCalcTextWidth(text_begin,text_end);
        return;
    }

    // Align to be pixel perfect
    pos.x = (float)(int)pos.x + font->DisplayOffset.x;
    pos.y = (float)(int)pos.y + font->DisplayOffset.y;
    float& x = pos.x;
    float& y = pos.y;

    const float scale = size / font->FontSize;
    const float line_height = font->FontSize * scale;

    const char* s = text_begin;
    if (y + line_height < clip_rect.y) while (s < text_end && *s != '\n')  s++;// Fast-forward to next line

    // Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
    const int vtx_count_max = (int)(text_end - s) * 4;
    const int idx_count_max = (int)(text_end - s) * 6;
    const int idx_expected_size = draw_list->IdxBuffer.Size + idx_count_max;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);


    ImDrawVert* vtx_write = draw_list->_VtxWritePtr;
    ImDrawIdx* idx_write = draw_list->_IdxWritePtr;
    unsigned int vtx_current_idx = draw_list->_VtxCurrentIdx;


    while (s < text_end)
    {
        // Decode and advance source
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)   {s += 1;}
        else    {
            s += ImTextCharFromUtf8(&c, s, text_end);
            if (c == 0) break;
        }

        float char_width = 0.0f;
        if (const ImFont::Glyph* glyph = font->FindGlyph((unsigned short)c))
        {
            char_width = glyph->XAdvance * scale;

            // Clipping on Y is more likely
            if (c != ' ' && c != '\t')
            {
                // We don't do a second finer clipping test on the Y axis (TODO: do some measurement see if it is worth it, probably not)
                float y1 = (float)(y + glyph->Y0 * scale);
                float y2 = (float)(y + glyph->Y1 * scale);

                float x1 = (float)(x + glyph->X0 * scale);
                float x2 = (float)(x + glyph->X1 * scale);
                if (x1 <= clip_rect.z && x2 >= clip_rect.x)
                {
                    // Render a character
                    float u1 = glyph->U0;
                    float v1 = glyph->V0;
                    float u2 = glyph->U1;
                    float v2 = glyph->V1;

                    // CPU side clipping used to fit text in their frame when the frame is too small. Only does clipping for axis aligned quads.
                    if (cpu_fine_clip)
                    {
                        if (x1 < clip_rect.x)
                        {
                            u1 = u1 + (1.0f - (x2 - clip_rect.x) / (x2 - x1)) * (u2 - u1);
                            x1 = clip_rect.x;
                        }
                        if (y1 < clip_rect.y)
                        {
                            v1 = v1 + (1.0f - (y2 - clip_rect.y) / (y2 - y1)) * (v2 - v1);
                            y1 = clip_rect.y;
                        }
                        if (x2 > clip_rect.z)
                        {
                            u2 = u1 + ((clip_rect.z - x1) / (x2 - x1)) * (u2 - u1);
                            x2 = clip_rect.z;
                        }
                        if (y2 > clip_rect.w)
                        {
                            v2 = v1 + ((clip_rect.w - y1) / (y2 - y1)) * (v2 - v1);
                            y2 = clip_rect.w;
                        }
                        if (y1 >= y2)
                        {
                            x += char_width;
                            continue;
                        }
                    }

                    // NB: we are not calling PrimRectUV() here because non-inlined causes too much overhead in a debug build.
                    // inlined:
                    {
                        idx_write[0] = (ImDrawIdx)(vtx_current_idx); idx_write[1] = (ImDrawIdx)(vtx_current_idx+1); idx_write[2] = (ImDrawIdx)(vtx_current_idx+2);
                        idx_write[3] = (ImDrawIdx)(vtx_current_idx); idx_write[4] = (ImDrawIdx)(vtx_current_idx+2); idx_write[5] = (ImDrawIdx)(vtx_current_idx+3);
                        vtx_write[0].pos.x = x1; vtx_write[0].pos.y = y1; vtx_write[0].col = col; vtx_write[0].uv.x = u1; vtx_write[0].uv.y = v1;
                        vtx_write[1].pos.x = x2; vtx_write[1].pos.y = y1; vtx_write[1].col = col; vtx_write[1].uv.x = u2; vtx_write[1].uv.y = v1;
                        vtx_write[2].pos.x = x2; vtx_write[2].pos.y = y2; vtx_write[2].col = col; vtx_write[2].uv.x = u2; vtx_write[2].uv.y = v2;
                        vtx_write[3].pos.x = x1; vtx_write[3].pos.y = y2; vtx_write[3].col = col; vtx_write[3].uv.x = u1; vtx_write[3].uv.y = v2;
                        vtx_write += 4;
                        vtx_current_idx += 4;
                        idx_write += 6;
                    }
                }
            }
        }

        x += char_width;
    }

    /*draw_list->_VtxWritePtr = vtx_write;
    draw_list->_VtxCurrentIdx = vtx_current_idx;
    draw_list->_IdxWritePtr = idx_write;*/

    // Give back unused vertices
    draw_list->VtxBuffer.resize((int)(vtx_write - draw_list->VtxBuffer.Data));
    draw_list->IdxBuffer.resize((int)(idx_write - draw_list->IdxBuffer.Data));
    draw_list->CmdBuffer[draw_list->CmdBuffer.Size-1].ElemCount -= (idx_expected_size - draw_list->IdxBuffer.Size);
    draw_list->_VtxWritePtr = vtx_write;
    draw_list->_IdxWritePtr = idx_write;
    draw_list->_VtxCurrentIdx = (unsigned int)draw_list->VtxBuffer.Size;



    // restore pos
    pos.x -= font->DisplayOffset.x;
    pos.y -= font->DisplayOffset.y;

}
static inline void ImDrawListAddTextLine(ImDrawList* draw_list,const ImFont* font, float font_size, ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, const ImVec4* cpu_fine_clip_rect = NULL)
{
    if (text_end == NULL)   text_end = text_begin + strlen(text_begin);
    if ((col & IM_COL32_A_MASK) == 0)   {
        pos.x+= (int)pos.x + MyCalcTextWidth(text_begin,text_end);
        return;
    }
    if (text_begin == text_end) return;

    IM_ASSERT(font->ContainerAtlas->TexID == draw_list->_TextureIdStack.back());  // Use high-level ImGui::PushFont() or low-level ImDrawList::PushTextureId() to change font.

    /*// reserve vertices for worse case (over-reserving is useful and easily amortized)
    const int char_count = (int)(text_end - text_begin);
    const int vtx_count_max = char_count * 4;
    const int idx_count_max = char_count * 6;
    const int vtx_begin = draw_list->VtxBuffer.Size;
    const int idx_begin = draw_list->IdxBuffer.Size;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);*/

    ImVec4 clip_rect = draw_list->_ClipRectStack.back();
    if (cpu_fine_clip_rect) {
        clip_rect.x = ImMax(clip_rect.x, cpu_fine_clip_rect->x);
        clip_rect.y = ImMax(clip_rect.y, cpu_fine_clip_rect->y);
        clip_rect.z = ImMin(clip_rect.z, cpu_fine_clip_rect->z);
        clip_rect.w = ImMin(clip_rect.w, cpu_fine_clip_rect->w);
    }
    ImDrawListRenderTextLine(draw_list,font,font_size, pos, col, clip_rect, text_begin, text_end, cpu_fine_clip_rect != NULL);

    // give back unused vertices
    // FIXME-OPT: clean this up
    /*draw_list->VtxBuffer.resize((int)(draw_list->_VtxWritePtr - draw_list->VtxBuffer.Data));
    draw_list->IdxBuffer.resize((int)(draw_list->_IdxWritePtr - draw_list->IdxBuffer.Data));
    int vtx_unused = vtx_count_max - (draw_list->VtxBuffer.Size - vtx_begin);
    int idx_unused = idx_count_max - (draw_list->IdxBuffer.Size - idx_begin);
    draw_list->CmdBuffer.back().ElemCount -= idx_unused;
    draw_list->_VtxWritePtr -= vtx_unused;
    draw_list->_IdxWritePtr -= idx_unused;
    draw_list->_VtxCurrentIdx = (ImDrawIdx)draw_list->VtxBuffer.Size;*/
}
static inline void ImDrawListAddTextLine(ImDrawList* draw_list,ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end=NULL)   {
    ImDrawListAddTextLine(draw_list,GImGui->Font, GImGui->FontSize, pos, col, text_begin, text_end);
}


} // namespace ImGui

namespace ImGuiCe   {

// Static stuff
CodeEditor::Style CodeEditor::style;  // static variable initialization
inline static bool EditColorImU32(const char* label,ImU32& color) {
    static ImVec4 tmp;
    tmp = ImColor(color);
    const bool changed = ImGui::ColorEdit4(label,&tmp.x);
    if (changed) color = ImColor(tmp);
    return changed;
}
static const char* FontStyleStrings[FONT_STYLE_COUNT] = {"NORMAL","BOLD","ITALIC","BOLD_ITALIC"};
static const char* SyntaxHighlightingTypeStrings[SH_COUNT] = {"SH_KEYWORD_ACCESS","SH_KEYWORD_CONSTANT","SH_KEYWORD_CONTEXT","SH_KEYWORD_DECLARATION","SH_KEYWORD_EXCEPTION","SH_KEYWORD_ITERATION","SH_KEYWORD_JUMP","SH_KEYWORD_KEYWORD_USER1","SH_KEYWORD_KEYWORD_USER2","SH_KEYWORD_METHOD","SH_KEYWORD_MODIFIER","SH_KEYWORD_NAMESPACE","SH_KEYWORD_OPERATOR","SH_KEYWORD_OTHER","SH_KEYWORD_PARAMENTER","SH_KEYWORD_PREPROCESSOR","SH_KEYWORD_PROPERTY","SH_KEYWORD_SELECTION","SH_KEYWORD_TYPE","SH_LOGICAL_OPERATORS","SH_MATH_OPERATORS","SH_BRACKETS_CURLY","SH_BRACKETS_SQUARE","SH_BRACKETS_ROUND","SH_PUNCTUATION","SH_STRING","SH_NUMBER","SH_COMMENT","SH_FOLDED_PARENTHESIS","SH_FOLDED_COMMENT","SH_FOLDED_REGION"};
static const char* SyntaxHighlightingColorStrings[SH_COUNT] = {"COLOR_KEYWORD_ACCESS","COLOR_KEYWORD_CONSTANT","COLOR_KEYWORD_CONTEXT","COLOR_KEYWORD_DECLARATION","COLOR_KEYWORD_EXCEPTION","COLOR_KEYWORD_ITERATION","COLOR_KEYWORD_JUMP","COLOR_KEYWORD_KEYWORD_USER1","COLOR_KEYWORD_KEYWORD_USER2","COLOR_KEYWORD_METHOD","COLOR_KEYWORD_MODIFIER","COLOR_KEYWORD_NAMESPACE","COLOR_KEYWORD_OPERATOR","COLOR_KEYWORD_OTHER","COLOR_KEYWORD_PARAMENTER","COLOR_KEYWORD_PREPROCESSOR","COLOR_KEYWORD_PROPERTY","COLOR_KEYWORD_SELECTION","COLOR_KEYWORD_TYPE","COLOR_LOGICAL_OPERATORS","COLOR_MATH_OPERATORS","COLOR_BRACKETS_CURLY","COLOR_BRACKETS_SQUARE","COLOR_BRACKETS_ROUND","COLOR_PUNCTUATION","COLOR_STRING","COLOR_NUMBER","COLOR_COMMENT","COLOR_FOLDED_PARENTHESIS","COLOR_FOLDED_COMMENT","COLOR_FOLDED_REGION"};
static const char* SyntaxHighlightingFontStrings[SH_COUNT] = {"FONT_KEYWORD_ACCESS","FONT_KEYWORD_CONSTANT","FONT_KEYWORD_CONTEXT","FONT_KEYWORD_DECLARATION","FONT_KEYWORD_EXCEPTION","FONT_KEYWORD_ITERATION","FONT_KEYWORD_JUMP","FONT_KEYWORD_KEYWORD_USER1","FONT_KEYWORD_KEYWORD_USER2","FONT_KEYWORD_METHOD","FONT_KEYWORD_MODIFIER","FONT_KEYWORD_NAMESPACE","FONT_KEYWORD_OPERATOR","FONT_KEYWORD_OTHER","FONT_KEYWORD_PARAMENTER","FONT_KEYWORD_PREPROCESSOR","FONT_KEYWORD_PROPERTY","FONT_KEYWORD_SELECTION","FONT_KEYWORD_TYPE","FONT_LOGICAL_OPERATORS","FONT_MATH_OPERATORS","FONT_BRACKETS_CURLY","FONT_BRACKETS_SQUARE","FONT_BRACKETS_ROUND","FONT_PUNCTUATION","FONT_STRING","FONT_NUMBER","FONT_COMMENT","FONT_FOLDED_PARENTHESIS","FONT_FOLDED_COMMENT","FONT_FOLDED_REGION"};
CodeEditor::Style::Style() {
    color_background =          ImColor(40,40,50,255);
    color_text_selected_background = ImColor(50,50,120,255);
    color_text =                ImGui::GetStyle().Colors[ImGuiCol_Text];
    color_line_numbers =        ImVec4(color_text.x,color_text.y,color_text.z,0.25f);
    color_icon_margin_error =        ImColor(255,0,0);
    color_icon_margin_warning =      ImColor(255,255,0);
    color_icon_margin_breakpoint =   ImColor(255,190,0,225);
    color_icon_margin_bookmark =     ImColor(190,190,225);
    color_icon_margin_contour =      ImColor(50,50,150);
    icon_margin_contour_thickness =  1.f;
    font_text = font_line_numbers = FONT_STYLE_NORMAL;

    for (int i=0;i<SH_COUNT;i++)    {
        color_syntax_highlighting[i] = ImColor(color_text);
        font_syntax_highlighting[i] = FONT_STYLE_NORMAL;
    }

    color_folded_parenthesis_background =
            color_folded_comment_background =
            color_folded_region_background = ImColor(color_background.x,color_background.y,color_background.z,0.f);
    folded_region_contour_thickness = 1.f;

    // TODO: Make better colorSyntaxHighlighting and fontSyntaxHighlighting here
    font_syntax_highlighting[SH_FOLDED_PARENTHESIS] =
            font_syntax_highlighting[SH_FOLDED_COMMENT] =
            font_syntax_highlighting[SH_FOLDED_REGION] = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_COMMENT]               = ImColor(150,220,255,200);
    color_syntax_highlighting[SH_NUMBER]                = ImColor(255,200,220,255);
    color_syntax_highlighting[SH_STRING]                = ImColor(255,64,70,255);

    color_syntax_highlighting[SH_KEYWORD_PREPROCESSOR]  = ImColor(50,120,200,255);
    font_syntax_highlighting[ SH_KEYWORD_PREPROCESSOR]  = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_BRACKETS_CURLY]        = ImColor(240,240,255,255);
    font_syntax_highlighting[ SH_BRACKETS_CURLY]        = FONT_STYLE_BOLD;
    color_syntax_highlighting[SH_BRACKETS_SQUARE]       = ImColor(255,240,240,255);
    font_syntax_highlighting[ SH_BRACKETS_SQUARE]       = FONT_STYLE_BOLD;
    color_syntax_highlighting[SH_BRACKETS_ROUND]        = ImColor(240,255,240,255);
    //font_syntax_highlighting[ SH_BRACKETS_ROUND]        = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_TYPE]    = ImColor(220,220,40,255);
    font_syntax_highlighting[ SH_KEYWORD_TYPE]    = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_CONSTANT]    = ImColor(220,80,40,255);
    font_syntax_highlighting[ SH_KEYWORD_CONSTANT]    = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_CONTEXT]    = ImColor(120,200,40,255);
    font_syntax_highlighting[ SH_KEYWORD_CONTEXT]    = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_ACCESS]        = ImColor(220,150,40,255);
    font_syntax_highlighting[ SH_KEYWORD_ACCESS]        = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_MODIFIER]      = ImColor(220,150,40,255);
    font_syntax_highlighting[ SH_KEYWORD_MODIFIER]      = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_NAMESPACE]     = ImColor(40,250,40,255);
    font_syntax_highlighting[ SH_KEYWORD_NAMESPACE]     = FONT_STYLE_BOLD_ITALIC;

    color_syntax_highlighting[SH_KEYWORD_DECLARATION]   = ImColor(40,250,40,255);
    font_syntax_highlighting[ SH_KEYWORD_DECLARATION]   = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_ITERATION]      = ImColor(200,180,100,255);
    font_syntax_highlighting[ SH_KEYWORD_ITERATION]      = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_JUMP]          = ImColor(220,150,100,255);
    font_syntax_highlighting[ SH_KEYWORD_JUMP]          = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_SELECTION]      = ImColor(220,220,100,255);
    font_syntax_highlighting[ SH_KEYWORD_SELECTION]      = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_OPERATOR]      = ImColor(220,150,240,255);
    font_syntax_highlighting[ SH_KEYWORD_OPERATOR]      = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_OTHER]      = ImColor(220,150,150,255);
    font_syntax_highlighting[ SH_KEYWORD_OTHER]      = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_MATH_OPERATORS]        = ImColor(255,150,100,255);
    //font_syntax_highlighting[ SH_MATH_OPERATORS]        = FONT_STYLE_BOLD;

    color_syntax_highlighting[SH_KEYWORD_KEYWORD_USER1]        = ImColor(140,230,120,255);
    color_syntax_highlighting[SH_KEYWORD_KEYWORD_USER2]        = ImColor(220,180,100,255);


    color_icon_margin_background = color_line_numbers_background = color_folding_margin_background
    = ImColor(color_background.x,color_background.y,color_background.z,0.f);

    color_syntax_highlighting[SH_FOLDED_COMMENT] = color_syntax_highlighting[SH_COMMENT];
    color_syntax_highlighting[SH_FOLDED_PARENTHESIS] = color_syntax_highlighting[SH_BRACKETS_CURLY];
    color_syntax_highlighting[SH_FOLDED_REGION] = ImColor(225,250,200,255);
}

bool CodeEditor::Style::Edit(CodeEditor::Style& s) {
    bool changed = false;
    const float dragSpeed = 0.5f;
    const char prec[] = "%1.1f";
    ImGui::PushID(&s);

    ImGui::Separator();
    ImGui::Text("Main");
    ImGui::Separator();
    ImGui::Spacing();
    changed|=ImGui::ColorEdit4( "background##color_background",&s.color_background.x);
    changed|=ImGui::ColorEdit4( "text##color_text",&s.color_text.x);
    changed|=ImGui::ColorEdit4( "textSelectedBg##color_text_selected_background",&s.color_text_selected_background.x);
    changed|=ImGui::Combo("text##font_text",&s.font_text,&FontStyleStrings[0],FONT_STYLE_COUNT,-1);
    changed|=EditColorImU32( "line_numbers_bg##color_line_numbers_background",s.color_line_numbers_background);
    changed|=ImGui::ColorEdit4( "line_numbers##color_line_numbers",&s.color_line_numbers.x);
    changed|=ImGui::Combo("line_numbers##font_line_numbers",&s.font_line_numbers,&FontStyleStrings[0],FONT_STYLE_COUNT,-1);
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Text("Margin");
    ImGui::Separator();
    ImGui::Spacing();
    changed|=EditColorImU32(    "error##color_margin_error",s.color_icon_margin_error);
    changed|=EditColorImU32(    "warning##color_margin_warning",s.color_icon_margin_warning);
    changed|=EditColorImU32(    "breakpoint##color_margin_breakpoint",s.color_icon_margin_breakpoint);
    changed|=EditColorImU32(    "bookmark##color_margin_bookmark",s.color_icon_margin_bookmark);
    changed|=EditColorImU32(    "contour##color_margin_contour",s.color_icon_margin_contour);
    changed|=ImGui::DragFloat(  "contour_width##margin_contour_thickness",&s.icon_margin_contour_thickness,dragSpeed,0.5f,5.f,prec);
    ImGui::Spacing();
    changed|=EditColorImU32( "icon_margin_bg##color_icon_margin_background",s.color_icon_margin_background);
    changed|=EditColorImU32( "folding_margin_bg##color_folding_margin_background",s.color_folding_margin_background);
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Text("Syntax Highlighting");
    ImGui::Separator();
    ImGui::Spacing();
    static int item=0;
    ImGui::Combo("##Token##Syntax Highlighting Token",&item, &SyntaxHighlightingTypeStrings[0], SH_COUNT, -1);
    ImGui::Spacing();
    changed|=EditColorImU32("color##color_sh_token",s.color_syntax_highlighting[item]);
    changed|=ImGui::Combo("font##font_sh_token",&s.font_syntax_highlighting[item],&FontStyleStrings[0],FONT_STYLE_COUNT,-1);
    if (item == SH_FOLDED_PARENTHESIS) changed|=EditColorImU32("background##color_folded_parenthesis_background",s.color_folded_parenthesis_background);
    else if (item == SH_FOLDED_COMMENT)	changed|=EditColorImU32("background##color_folded_comment_background",s.color_folded_comment_background);
    else if (item == SH_FOLDED_REGION)	{
        changed|=EditColorImU32("background##color_folded_region_background",s.color_folded_region_background);
        changed|=ImGui::DragFloat(  "contour_width##folded_region_contour_thickness",&s.folded_region_contour_thickness,dragSpeed,0.5f,5.f,prec);
    }

    ImGui::Separator();

    ImGui::PopID();
    return changed;
}
#if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
#ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
#include "../imguihelper/imguihelper.h"
bool CodeEditor::Style::Save(const CodeEditor::Style &style,ImGuiHelper::Serializer& s)    {
    if (!s.isValid()) return false;

    ImVec4 tmpColor = ImColor(style.color_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_background",4);
    tmpColor = ImColor(style.color_text);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_text",4);
    tmpColor = ImColor(style.color_text_selected_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_text_selected_background",4);
    s.save(&style.font_text,"font_text");
    tmpColor = ImColor(style.color_line_numbers_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_line_numbers_background",4);
    tmpColor = ImColor(style.color_line_numbers);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_line_numbers",4);
    s.save(&style.font_line_numbers,"font_line_numbers");
    tmpColor = ImColor(style.color_icon_margin_error);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_error",4);
    tmpColor = ImColor(style.color_icon_margin_warning);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_warning",4);
    tmpColor = ImColor(style.color_icon_margin_breakpoint);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_breakpoint",4);
    tmpColor = ImColor(style.color_icon_margin_bookmark);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_bookmark",4);
    tmpColor = ImColor(style.color_icon_margin_contour);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_contour",4);

    tmpColor = ImColor(style.color_icon_margin_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_icon_margin_background",4);
    tmpColor = ImColor(style.color_folding_margin_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_folding_margin_background",4);


    for (int i=0;i<SH_COUNT;i++)    {
        tmpColor = ImColor(style.color_syntax_highlighting[i]);s.save(ImGui::FT_COLOR,&tmpColor.x,SyntaxHighlightingColorStrings[i],4);
        s.save(ImGui::FT_ENUM,&style.font_syntax_highlighting[i],SyntaxHighlightingFontStrings[i]);
    }

    s.save(ImGui::FT_FLOAT,&style.icon_margin_contour_thickness,"icon_margin_contour_thickness");

    tmpColor = ImColor(style.color_folded_parenthesis_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_folded_parenthesis_background",4);
    tmpColor = ImColor(style.color_folded_comment_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_folded_comment_background",4);
    tmpColor = ImColor(style.color_folded_region_background);s.save(ImGui::FT_COLOR,&tmpColor.x,"color_folded_region_background",4);

    s.save(ImGui::FT_FLOAT,&style.folded_region_contour_thickness,"folded_region_contour_thickness");

    return true;
}
#endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
#include "../imguihelper/imguihelper.h"
static bool StyleParser(ImGuiHelper::FieldType ft,int /*numArrayElements*/,void* pValue,const char* name,void* userPtr)    {
    CodeEditor::Style& s = *((CodeEditor::Style*) userPtr);
    ImVec4& tmp = *((ImVec4*) pValue);  // we cast it soon to float for now...    
    switch (ft) {
    case ImGui::FT_FLOAT:
        if (strcmp(name,"icon_margin_contour_thickness")==0)                s.icon_margin_contour_thickness = tmp.x;
    else if (strcmp(name,"folded_region_contour_thickness")==0)             s.folded_region_contour_thickness = tmp.x;
    //else if (strcmp(name,"grid_size")==0)                                 s.grid_size = tmp.x;
    break;
    case ImGui::FT_INT:
        //if (strcmp(name,"link_num_segments")==0)                          s.link_num_segments = *((int*)pValue);
    break;
    case ImGui::FT_ENUM:
        if (strcmp(name,"font_line_numbers")==0)   {
            int pi = *((int*) pValue);
            if (pi>=0 && pi<FONT_STYLE_COUNT) s.font_line_numbers = pi;
        }
        else if (strcmp(name,"font_text")==0)   {
            int pi = *((int*) pValue);
            if (pi>=0 && pi<FONT_STYLE_COUNT) s.font_text = pi;
        }
        else for (int i=0;i<SH_COUNT;i++) {
            if (strcmp(name,SyntaxHighlightingFontStrings[i])==0)           {
                int pi = *((int*) pValue);
                if (pi>=0 && pi<FONT_STYLE_COUNT) s.font_syntax_highlighting[i] = pi;
                break;
            }
        }
    break;
    case ImGui::FT_COLOR:
        if (strcmp(name,"color_background")==0)                             s.color_background = ImColor(tmp);
        else if (strcmp(name,"color_text")==0)                              s.color_text = ImColor(tmp);
        else if (strcmp(name,"color_text_selected_background")==0)          s.color_text_selected_background = ImColor(tmp);
        else if (strcmp(name,"color_line_numbers_background")==0)           s.color_line_numbers = ImColor(tmp);
        else if (strcmp(name,"color_line_numbers")==0)                      s.color_line_numbers = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_error")==0)                 s.color_icon_margin_error = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_warning")==0)               s.color_icon_margin_warning = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_breakpoint")==0)            s.color_icon_margin_breakpoint = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_bookmark")==0)              s.color_icon_margin_bookmark = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_contour")==0)               s.color_icon_margin_contour = ImColor(tmp);
        else if (strcmp(name,"color_folded_parenthesis_background")==0)     s.color_folded_parenthesis_background = ImColor(tmp);
        else if (strcmp(name,"color_folded_comment_background")==0)         s.color_folded_comment_background = ImColor(tmp);
        else if (strcmp(name,"color_folded_region_background")==0)          s.color_folded_region_background = ImColor(tmp);
        else if (strcmp(name,"color_icon_margin_background")==0)            s.color_icon_margin_background = ImColor(tmp);
        else if (strcmp(name,"color_folding_margin_background")==0)         s.color_folding_margin_background = ImColor(tmp);
        else {
            for (int i=0;i<SH_COUNT;i++) {
                if (strcmp(name,SyntaxHighlightingColorStrings[i])==0)      {s.color_syntax_highlighting[i] = ImColor(tmp);break;}
            }
        }
    break;
    default:
    // TODO: check
    break;
    }
    return false;
}
bool CodeEditor::Style::Load(CodeEditor::Style &style, ImGuiHelper::Deserializer& d, const char **pOptionalBufferStart)  {
    if (!d.isValid()) return false;
    const char* offset = d.parse(StyleParser,(void*)&style,pOptionalBufferStart?(*pOptionalBufferStart):NULL);
    if (pOptionalBufferStart) *pOptionalBufferStart=offset;
    return true;
}
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#endif //NO_IMGUIHELPER_SERIALIZATION


const ImFont* CodeEditor::ImFonts[FONT_STYLE_COUNT] = {NULL,NULL,NULL,NULL};

void CodeEditor::TextLineWithSHV(const char* fmt, va_list args) {
    if (ImGui::GetCurrentWindow()->SkipItems)  return;

//#define TEST_HERE
#ifndef TEST_HERE
    ImGuiContext& g = *GImGui;
    const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    TextLineUnformattedWithSH(g.TempBuffer, text_end);
#else //TEST_HERE
    ImGuiContext& g = *GImGui;
    const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    // Bg Color-------------------
    ImU32 bg_col = ImColor(255,255,255,50);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    // All the way to the right
    //ImGui::RenderFrame(pos, ImVec2(pos.x + ImGui::GetContentRegionMax().x+ImGui::GetScrollX(), pos.y + ImGui::GetTextLineHeight()), bg_col,false);
    // Until text end
    ImGui::RenderFrame(pos, ImVec2(pos.x + ImGui::MyCalcTextWidth(g.TempBuffer, text_end), pos.y + ImGui::GetTextLineHeight()), bg_col,false);
    //----------------------------
    TextLineUnformattedWithSH(g.TempBuffer, text_end);
#undef TEST_HERE
#endif //TEST_HERE
}
void CodeEditor::TextLineWithSH(const char* fmt, ...)   {
    va_list args;
    va_start(args, fmt);
    TextLineWithSHV(fmt, args);
    va_end(args);
}


struct Line {
    enum Attribute {
	AT_BOOKMARK             = 1,
	AT_BREAKPOINT           = 1<<1,
	AT_WARNING              = 1<<2,
	AT_ERROR                = 1<<3,
	AT_FOLDING_START        = 1<<4,
	AT_FOLDING_END          = 1<<5,
	AT_HIDDEN               = 1<<6,
	AT_SAVE_SPACE_LINE      = 1<<7,
	AT_FOLDING_START_FOLDED   = 1<<8
    };

    // Fixed data
    ImString text;
    int attributes;

    // Offst Data
    int lineNumber;		// zero based
    unsigned offset;            // in bytes, from the start of the document, without '\n' and '\r'
    unsigned offsetInUTF8chars; // in UTF8,  from the start of the document, without '\n' and '\r'
    int numUTF8chars;

    // Folding Data
    const FoldingTag* foldingStartTag;
    int foldingStartOffset;     // in bytes, used if AT_FOLDING_START,	from the start of the line to the first char of the "start folding" tag
    Line* foldingEndLine;	// reference used if AT_FOLDING_START
    int foldingEndOffset;       // in bytes, used if AT_FOLDING_END,	from the start of the line to the last  char of the "end   folding" tag
    Line* foldingStartLine;	// reference used if AT_FOLDING_END

    // Proposal: use "foldingStartLine" and "foldingEndLine" for all the lines inside a folding region too.
    // => DONE <=

    void reset() {text="";}
    int size() {return text.size();}
    Line(const ImString& txt="") : text(txt),attributes(0),
    lineNumber(-1),offset(0),offsetInUTF8chars(0),numUTF8chars(0),
    foldingStartTag(NULL),foldingStartOffset(0),foldingEndLine(NULL),
    foldingEndOffset(0),foldingStartLine(NULL) {}

    inline bool isFoldable() const {return (attributes&AT_FOLDING_START);}
    inline bool isFolded() const {return (attributes&AT_FOLDING_START) && (attributes&AT_FOLDING_START_FOLDED);}
    inline void setFoldedState(bool state) {if (!state) attributes&=~AT_FOLDING_START_FOLDED;else attributes|=AT_FOLDING_START_FOLDED;}
    inline bool isFoldingEnd() const {return (attributes&AT_FOLDING_END);}
    inline bool canFoldingBeMergedWithLineAbove() const {return (attributes&AT_FOLDING_START)&&(attributes&AT_SAVE_SPACE_LINE);}
    inline void resetFoldingAttributes() {
	attributes&=~(AT_FOLDING_START|AT_FOLDING_END|AT_SAVE_SPACE_LINE|AT_FOLDING_START_FOLDED|AT_HIDDEN);
	foldingStartOffset=foldingEndOffset=0;
	foldingStartTag = NULL;
	foldingStartLine = foldingEndLine = NULL;
    }
    inline void resetFoldingStartAttributes() {
	attributes&=~(AT_FOLDING_START|AT_SAVE_SPACE_LINE|AT_FOLDING_START_FOLDED|AT_HIDDEN);
	foldingStartOffset=0;
	foldingStartTag = NULL;
	foldingEndLine = NULL;
    }
    inline void resetFoldingEndAttributes() {
	attributes&=~(AT_FOLDING_END|AT_HIDDEN);
	foldingEndOffset=0;
	foldingStartLine = NULL;
    }
    inline static void AdjustHiddenFlagsForFoldingStart(Lines& lines,const Line& line) {
	if (!line.isFoldable() || !line.foldingEndLine) return;
	bool state = !line.isFolded();
	for (int i=line.lineNumber+1,isz=line.foldingEndLine->lineNumber;i<isz;i++) {
	    Line* ln = lines[i];
	    ln->setHidden(state);
	    if (ln->isFolded() && ln->foldingEndLine) i=ln->foldingEndLine->lineNumber-1;
	}
	if (line.foldingStartTag->kind==FOLDING_TYPE_REGION) line.foldingEndLine->setHidden(state);
    }
    inline bool isHidden() const {return attributes&AT_HIDDEN;}
    inline void setHidden(bool state)  {if (state) attributes&=~AT_HIDDEN;else attributes|=AT_HIDDEN;}
    friend class Lines;friend class FoldSegment;friend class FoldingString;
};


Line *Lines::add(int lineNum) {
    if (lineNum<0 || lineNum>size()) lineNum=size();
    resize(size()+1);
    for (int i=size()-1;i>lineNum;i--)    (*this)[i] = (*this)[i-1];
    Line* line = (*this)[lineNum] = IMGUI_NEW(Line);
    return line;
}

bool Lines::remove(int lineNum)    {
    if (lineNum<0 || lineNum>=size()) return false;
    Line* line = (*this)[lineNum];
    for (int i=lineNum,isz=size()-1;i<isz;i++)   (*this)[i] = (*this)[i+1];
    resize(size()-1);
    IMGUI_DELETE(Line,line);line=NULL;
    return true;
}

void Lines::destroy(bool keepFirstLine) {
    for (int i=size()-1;i>=0;i--)  {
        Line*& line = (*this)[i];
        IMGUI_DELETE(Line,line);line=NULL;
    }
    Base::clear();
    if (keepFirstLine) add();
}


void Lines::getText(ImString &rv, int startLineNum, int startLineOffsetBytes, int endLineNum, int endLineOffsetBytes) const {
    rv="";
    IM_ASSERT(size()>0);

    if (startLineNum<0) startLineNum=0;
    if (startLineOffsetBytes<0) startLineOffsetBytes=0;
    else if (startLineOffsetBytes>(*this)[startLineNum]->text.size()) {++startLineNum;startLineOffsetBytes=0;}
    if (startLineNum>=size()) return;
    if (endLineNum<0 || endLineNum>=size()) {endLineNum=size()-1;endLineOffsetBytes=(*this)[endLineNum]->text.size();}

    rv+=(*this)[startLineNum]->text.substr(startLineOffsetBytes);
    if (startLineNum!=endLineNum) rv+=cr;
    for (int ln=startLineNum+1;ln<=endLineNum-1;ln++) {
        rv+=(*this)[ln]->text;
        rv+=cr;
    }
    rv+=(*this)[endLineNum]->text.substr(0,endLineOffsetBytes);
}

void Lines::getText(ImString &rv, int startTotalOffsetInBytes, int endTotalOffsetBytes) const {
    rv="";
    IM_ASSERT(size()>0);
    const int CR_SIZE = (int) cr.size();
    const ImString& CR = cr;
    if (startTotalOffsetInBytes<0) startTotalOffsetInBytes=0;
    if (endTotalOffsetBytes==0 || (endTotalOffsetBytes>0 && endTotalOffsetBytes<startTotalOffsetInBytes)) return;

    int offset=0,tmpOffset=0;
    Line* line0 = (*this)[0];
    tmpOffset=offset+line0->size();
    if (startTotalOffsetInBytes<tmpOffset)  rv+=line0->text.substr(startTotalOffsetInBytes-offset);
    offset=tmpOffset;
    tmpOffset=offset+CR_SIZE;
    if (startTotalOffsetInBytes<tmpOffset && size()>1)  {
        rv+=CR;
        if (endTotalOffsetBytes>0 && endTotalOffsetBytes<tmpOffset) {
            rv=rv.substr(0,endTotalOffsetBytes-startTotalOffsetInBytes);
            return;
        }
    }
    offset=tmpOffset;

    for (int ln=1,lnsz=size();ln<lnsz;ln++) {
        Line* line = (*this)[ln];
        tmpOffset=offset+line->size();
        if (rv.size()==0)   {
            if (startTotalOffsetInBytes<tmpOffset)  {
                if (endTotalOffsetBytes>0 && endTotalOffsetBytes<tmpOffset) {
                    rv+=line->text.substr(startTotalOffsetInBytes-offset,endTotalOffsetBytes-startTotalOffsetInBytes);
                    return;
                }
                else rv+=line->text.substr(startTotalOffsetInBytes-offset);
            }
            offset = tmpOffset;
            if (lnsz>ln+1)  {
                tmpOffset = offset+CR_SIZE;
                if (endTotalOffsetBytes<0 || endTotalOffsetBytes>=tmpOffset)  rv+=CR;
                else {
                    rv+=CR.substr(0,endTotalOffsetBytes-tmpOffset);
                    return;
                }
                offset=tmpOffset;
            }
            continue;
        }

        if (rv.size()>0 && endTotalOffsetBytes>0 && endTotalOffsetBytes<tmpOffset)  {
            rv+=line->text.substr(0,endTotalOffsetBytes-offset);
            return;
        }
        offset = tmpOffset;
        if (rv.size()>0) rv+=line->text;
        if (lnsz>ln+1)  {
            tmpOffset = offset+CR_SIZE;
            if (rv.size()>0)    {
                if (endTotalOffsetBytes<0 || endTotalOffsetBytes>=tmpOffset)  rv+=CR;
                else {
                    rv+=CR.substr(0,endTotalOffsetBytes-tmpOffset);
                    return;
                }
            }
            offset=tmpOffset;
        }
    }

}


void Lines::SplitText(const char *text, ImVector<Line*> &lines,ImString* pOptionalCRout)    {
    IM_ASSERT(lines.size()==0); // replacement to the line below
    //lines.clear();  //MMMmhhh, should we call delete on all the line* ?
    if (pOptionalCRout) *pOptionalCRout="\n";
    if (!text) return;
    ImString ln("");char c;unsigned offsetInBytes=0,offsetInUTF8Chars=0;
    unsigned nl_n=0,nl_rn=0,nl_r=0,nl_nr=0;Line* line=NULL;
    for (int i=0,isz=strlen(text);i<isz;i++)    {
	line = NULL;
        c=text[i];
        if (c=='\n')    {
	    line = IMGUI_NEW(Line);
            if (i+1<isz && text[i+1]=='\r') {++i;++nl_nr;}
            else ++nl_n;
        }
        else if (c=='\r')   {
	    line = IMGUI_NEW(Line);
            if (i+1<isz && text[i+1]=='\n') {++i;++nl_rn;}
            else ++nl_r;
        }
	if (line)   {
	    //-----------------------------
	    line->text =		ln;
	    line->lineNumber =		lines.size();
	    line->offset =		offsetInBytes;
	    line->offsetInUTF8chars =	offsetInUTF8Chars;
        line->numUTF8chars =	ImGui::CountUTF8Chars(line->text.c_str());
	    lines.push_back(line);
	    //-----------------------------
	    offsetInBytes+=line->size();
	    offsetInUTF8Chars+=line->numUTF8chars;
	    ln="";continue;
	}
        ln+=c;
    }
    line = IMGUI_NEW(Line);
    line->text=ln;
    //-----------------------------
    line->text =		ln;
    line->lineNumber =		lines.size();
    line->offset =		offsetInBytes;
    line->offsetInUTF8chars =	offsetInUTF8Chars;
    line->numUTF8chars =	ImGui::CountUTF8Chars(line->text.c_str());
    lines.push_back(line);
    //-----------------------------
    offsetInBytes+=line->size();
    offsetInUTF8Chars+=line->numUTF8chars;
    ln="";

    if (pOptionalCRout) {
        if (nl_n<nl_rn || nl_n<nl_r)    *pOptionalCRout= nl_rn >= nl_r ? "\r\n" : "\r";
        //fprintf(stderr,"CR=\"%s\"\n",(*pOptionalCRout=="\n")?"\\n":(*pOptionalCRout=="\r\n")?"\\r\\n":(*pOptionalCRout=="\r")?"\\r":"\\n\\r");
    }
}

void Lines::setText(const char *text)   {
    destroy(text ? false : true);
    if (!text) return;
    ImVector<Line*> Lines;SplitText(text,Lines,&cr);
    for (int i=0,isz=Lines.size();i<isz;i++) {
        Base::push_back(Lines[i]);
    }
}

void CodeEditor::SetFonts(const ImFont *normal, const ImFont *bold, const ImFont *italic, const ImFont *boldItalic)  {

    const ImFont* fnt = ImFonts[FONT_STYLE_NORMAL]=(normal ? normal : (ImGui::GetIO().Fonts->Fonts.size()>0) ? (ImGui::GetIO().Fonts->Fonts[0]) : NULL);
    ImFonts[FONT_STYLE_BOLD]=bold?bold:fnt;
    ImFonts[FONT_STYLE_ITALIC]=italic?italic:fnt;
    ImFonts[FONT_STYLE_BOLD_ITALIC]=boldItalic?boldItalic:ImFonts[FONT_STYLE_BOLD]?ImFonts[FONT_STYLE_BOLD]:ImFonts[FONT_STYLE_ITALIC]?ImFonts[FONT_STYLE_ITALIC]:fnt;

    IM_ASSERT(ImFonts[FONT_STYLE_NORMAL]->FontSize == ImFonts[FONT_STYLE_BOLD]->FontSize);
    IM_ASSERT(ImFonts[FONT_STYLE_BOLD]->FontSize == ImFonts[FONT_STYLE_ITALIC]->FontSize);
    IM_ASSERT(ImFonts[FONT_STYLE_ITALIC]->FontSize == ImFonts[FONT_STYLE_BOLD_ITALIC]->FontSize);
}



// Folding Stuff
FoldingTag::FoldingTag(const ImString &s, const ImString &e, const ImString &_title, FoldingType t, bool _gainOneLineWhenPossible)  {
    start = s;
    IM_ASSERT(start.length()>0); // start string can't be empty
    end = e;
    IM_ASSERT(end.length()>0 && (!(end==start && t!=FOLDING_TYPE_COMMENT))); // end string can't be empty or equal to start string if they're not multiline comments
    title = _title;
    kind = t;
    gainOneLineWhenPossible = _gainOneLineWhenPossible;
}
class FoldingString : public FoldingTag	{
public:
    class FoldingPoint  {
    public:
        int lineNumber;
	Line* line;
        int lineOffset;         // (in bytes) from the start of the line
        bool isStartFolding;
        bool canGainOneLine;    // used only if isStartFolding == tru
        ImString customTitle;
        int openCnt;
	inline FoldingPoint(int _lineNumber=-1,Line* _line=NULL,int _lineOffset=-1,bool _isStartFolding=false,int _openCnt=0,bool _canGainOneLine=false)
        : lineNumber(_lineNumber),line(_line),lineOffset(_lineOffset),
        isStartFolding(_isStartFolding),canGainOneLine(_canGainOneLine),openCnt(_openCnt)
        {}
    };

public:
    const ImString& getStart() const {return start;}
    const ImString& getEnd() const {return end;}
    FoldingType getKind() const {return kind;}
    // If the folding title is "", the folding title will be set as the text between "Start" and the rest of the line
    const ImString& getTitle() const {return title;}
    bool getGainOneLineWhileFoldingWhenPossible() const {return gainOneLineWhenPossible;}
public:
    ImVectorEx<FoldingPoint> foldingPoints;
    FoldingPoint* getMatchingFoldingPoint (int foldingPointIndex)    {
        FoldingPoint* rv = NULL;
        if (foldingPointIndex >= (int)foldingPoints.size())  return NULL;
        const FoldingPoint& fp = foldingPoints [foldingPointIndex];
        int openCnt = fp.openCnt;

        if (fp.isStartFolding) {
            for (int i = foldingPointIndex+1,isz = (int)foldingPoints.size();i < isz; i++) {
                rv = &foldingPoints[i];
                if (rv->openCnt == openCnt)	{
                    if (!rv->isStartFolding) return rv;
                    return NULL;
                }
            }
        } else {
            for (int i = foldingPointIndex-1; i>=0; i--) {
                rv = &foldingPoints[i];
                if (rv->openCnt == openCnt)	{
                    if (rv->isStartFolding) return rv;
                    return NULL;
                }
            }
        }
        return NULL;
    }
    FoldingString(const ImString& s,const ImString& e,const ImString& _title,FoldingType t,bool _gainOneLineWhenPossible=false)
    : FoldingTag(s,e,_title,t,_gainOneLineWhenPossible)
    {
        openCnt=0;
    }
    FoldingString(const FoldingTag& tag) : FoldingTag(tag) {
        openCnt=0;
    }
    FoldingString() : FoldingTag() {
        openCnt=0;
    }

    int openCnt;//=0;

    int matchStartBeg;
    int curStartCharIndex;      // inside Start
    int matchEndBeg;
    int curEndCharIndex;		// inside End
    bool lookForCustomTitle;

};
class FoldingStringVector : public ImVectorEx<FoldingString> {
    protected:
    typedef ImVectorEx<FoldingString> Base;
    public:
    bool mergeAdditionalTrailingCharIfPossible;
    char additionalTrailingChar;
    FoldingStringVector() {
        mergeAdditionalTrailingCharIfPossible = false;
        additionalTrailingChar = ';';
        punctuationStringsMerged[0] = '\0';
        resetSHvariables();
    }
    FoldingStringVector(size_t size) : Base(size) {
        mergeAdditionalTrailingCharIfPossible = false;
        additionalTrailingChar = ';';
        punctuationStringsMerged[0] = '\0';
        resetSHvariables();
    }

    FoldingStringVector(const ImVectorEx<FoldingTag>& tags,bool _mergeAdditionalTrailingCharIfPossible=false,char _additionalTrailingChar=';')   {
        punctuationStringsMerged[0] = '\0';
        mergeAdditionalTrailingCharIfPossible = _mergeAdditionalTrailingCharIfPossible;
        additionalTrailingChar = _additionalTrailingChar;
        this->reserve(tags.size());
        for (size_t i=0,isz=tags.size();i<isz;i++)  {
            this->push_back(FoldingString(tags[i]));
        }
        resetSHvariables();
    }

    void resetAllTemporaryData()    {
        for (size_t i=0,isz=size();i<isz;i++) {
            FoldingString& f = (*this)[i];
            f.openCnt = 0;
            f.matchStartBeg=f.matchEndBeg=-1;
            f.curStartCharIndex=f.curEndCharIndex=0;
            f.lookForCustomTitle=false;
            f.foldingPoints.clear();
        }
    }
    void resetTemporaryLineData()
    {
        for (size_t i=0,isz=size();i<isz;i++) {
            FoldingString& f =(*this)[i];
            f.matchStartBeg=f.matchEndBeg=-1;
            f.curStartCharIndex=f.curEndCharIndex=0;
            f.lookForCustomTitle=false;
        }
    }

    // Stuff added for Syntax highlighting:
    public:
    void resetSHvariables() {
        for (int i=0;i<SH_LOGICAL_OPERATORS;i++) keywords[i].clear();
        for (int i=0;i<SH_STRING-SH_LOGICAL_OPERATORS+1;i++) punctuationStrings[i]=NULL;
        singleLineComment = multiLineCommentStart = multiLineCommentEnd = NULL;
        punctuationStringsMerged[0]='\0';
        punctuationStringsMergedSHMap.clear();        
        languageExtensions = NULL;
        stringEscapeChar = '\\';
        stringDelimiterChars = NULL;
    }

    // These const char* are all references to persistent strings except punctuationStringsMerged
    ImVectorEx<const char* > keywords[SH_LOGICAL_OPERATORS];
    const char* punctuationStrings[SH_STRING-SH_LOGICAL_OPERATORS+1];
    const char* singleLineComment;
    const char* multiLineCommentStart,*multiLineCommentEnd;
    char stringEscapeChar;
    const char* stringDelimiterChars;   // e.g. "\"'"

    char punctuationStringsMerged[1024];	// This must be sorted alphabetically (to be honest now that we use a map, sorting it's not needed anymore (and it was not used even before!))
    ImVectorEx<int> punctuationStringsMergedSHMap;
    const char* languageExtensions;

    public:
    // Maybe we could defer this call until a language is effectively required...
    void initSyntaxHighlighting() {
    static const char* PunctuationStringsDefault[SH_COMMENT-SH_LOGICAL_OPERATORS+1+2] = {"&!|~^","+-*/<>=","{}","[]","()",".:,;?%","\"'","//","/*","*/"};


    for (int i=0;i<SH_STRING-SH_LOGICAL_OPERATORS+1;i++) {
        if (!punctuationStrings[i]) punctuationStrings[i] = PunctuationStringsDefault[i];
	    //fprintf(stderr,"punctuationStrings[%d] = \"%s\"\n",i,punctuationStrings[i]);
	}

    if (!singleLineComment) singleLineComment = PunctuationStringsDefault[SH_COMMENT-SH_LOGICAL_OPERATORS];
    //fprintf(stderr,"singleLineComment = \"%s\"\n",singleLineComment);
    if (!multiLineCommentStart)	multiLineCommentStart = PunctuationStringsDefault[SH_COMMENT+1-SH_LOGICAL_OPERATORS];
    if (!multiLineCommentEnd)   multiLineCommentEnd = PunctuationStringsDefault[SH_COMMENT+2-SH_LOGICAL_OPERATORS];
	//fprintf(stderr,"multiLineCommentStart = \"%s\"\n",multiLineCommentStart);
	//fprintf(stderr,"multiLineCommentEnd = \"%s\"\n",multiLineCommentEnd);
    if (!stringDelimiterChars) {
        static const char* sdc = "\"'";
        stringDelimiterChars = sdc;
    }

	recalculateMergedString();

	//fprintf(stderr,"punctuationStringsMerged = \"%s\"\n",punctuationStringsMerged);

    }

    public:
    void recalculateMergedString() {
    punctuationStringsMerged[0]='\0';strcat(punctuationStringsMerged,"\t ");    // Add space and tab
	for (int i=0;i<SH_PUNCTUATION-SH_LOGICAL_OPERATORS+1;i++) {
        IM_ASSERT(strlen(punctuationStringsMerged)+strlen(punctuationStrings[i])<1022); // punctuationStringsMerged too short
	    strcat(punctuationStringsMerged,punctuationStrings[i]);
	}
	// Sort punctuationStringsMerged:
	const int len = strlen(punctuationStringsMerged);char tmp;
	for (int i=0;i<len;i++)	{
	    char& t = punctuationStringsMerged[i];
	    for (int j=i+1;j<len;j++)	{
		char& u = punctuationStringsMerged[j];
		if (u<t)    {tmp = t;t = u; u = tmp;}
	    }
	}
	punctuationStringsMergedSHMap.clear();
	punctuationStringsMergedSHMap.resize(len);
	for (int i=0;i<len;i++)	{
	    punctuationStringsMergedSHMap[i]=-1;
	    const char& t = punctuationStringsMerged[i];
	    //fprintf(stderr,"t[%d] = '%c'\n",i,punctuationStringsMerged[i]);

	    for (int j=0;j<SH_STRING-SH_LOGICAL_OPERATORS+1;j++) {
		const char* str = punctuationStrings[j];
		//fprintf(stderr,"punctuationStrings[%d] = \"%s\"\n",i,punctuationStrings[i]);
		if (!str || strlen(str)==0) continue;
		if (strchr(str,t)) {
		    punctuationStringsMergedSHMap[i] = j + SH_LOGICAL_OPERATORS;
		    break;
		}
	    }
        //IM_ASSERT (punctuationStringsMergedSHMap[i]>=0 && punctuationStringsMergedSHMap[i]<SH_COUNT);
	    //fprintf(stderr,"punctuationStringsMergedSHMap[%d] = %d\n",i,punctuationStringsMergedSHMap[i]);
	}
    }


};
class FoldSegment {
public:
    const FoldingTag* matchingTag;
    //ImString title;                         // e.g. "{...}", or "" for FOLDING_TYPE_REGION (if the folding title is "", the folding title will be set as the text between "startLineOffset" and the rest of the startLine)
    int startLineIndex,endLineIndex;        // zero based.
    Line *startLine, *endLine;
    int startLineOffset,endLineOffset;      // in bytes, relative to the start [pos of the first "start folding mark" char and pos after the last "end folding mark" char] of the respective lines.
    //FoldingType kind;
    bool isFolded;
    bool canGainOneLine;                    // when true, the folded code can be appended to lines[startLineIndex-1] (instead of startLine).
    inline FoldSegment(const FoldingTag* _matchingTag=NULL,
		       int _startLineIndex=-1,Line* _startLine=NULL,int _startLineOffset=-1,
		       int _endLineIndex=-1,Line* _endLine=NULL,int _endLineOffset=-1,
		       bool _canGainOneLine=false)
    : matchingTag(_matchingTag),
      startLineIndex(_startLineIndex),endLineIndex(_endLineIndex),
      startLine(_startLine),endLine(_endLine),
      startLineOffset(_startLineOffset),endLineOffset(_endLineOffset),
      isFolded(false),canGainOneLine(_canGainOneLine)
    {}
    void fprintf() const {
        ::fprintf(stderr,"startLine[%d:%d (k:%d %s %s) \"%s\"] endLine[%d:%d \"%s\"] title:\"%s\"",
		startLineIndex,startLineOffset,(int)matchingTag->kind,isFolded?"F":"O",canGainOneLine?"^":" ",startLine->text.c_str(),
                endLineIndex,endLineOffset,endLine->text.c_str(),
		matchingTag->title.c_str());
    }
};
class FoldSegmentVector : public ImVectorEx<FoldSegment> {
public:
};


static ImVectorEx<FoldingStringVector> gFoldingStringVectors;
static int gFoldingStringVectorIndices[LANG_COUNT] = {-1,-1,-1,-1,-1,-1}; // global variable (values are indices from LANG_ENUM into gFoldingStringVectors)
static ImString gTotalLanguageExtensionFilter = ""; // SOMETHING LIKE ".cpp;.h;.cs;.py"
static void InitFoldingStringVectors() {
    for (int i=0;i<(int)LANG_COUNT;i++) gFoldingStringVectorIndices[i] = -1;    // This is mandatory, because when I add an enum, the compiler somehow does not trigger any error in the declaration of gFoldingStringVectorIndices...
    if (gFoldingStringVectors.size()==0)    {
	//gFoldingStringVectors.reserve(LANG_COUNT);
        // CPP
        {
        FoldingStringVector foldingStrings; // To fill and push_back
        // Main Folding
	    foldingStrings.push_back(FoldingString ("{", "}", "{...}", FOLDING_TYPE_PARENTHESIS,true));
	    foldingStrings.push_back(FoldingString ("//region ", "//endregion", "", FOLDING_TYPE_REGION));
        foldingStrings.push_back (FoldingString ("/*", "*/", "/*...*/", FOLDING_TYPE_COMMENT));
        // Optional Folding
	    foldingStrings.push_back(FoldingString ("#pragma region ", "#pragma endregion", "", FOLDING_TYPE_REGION));
	    foldingStrings.push_back(FoldingString ("(", ")", "(...)", FOLDING_TYPE_PARENTHESIS, true));
	    foldingStrings.push_back(FoldingString ("[", "]", "[...]", FOLDING_TYPE_PARENTHESIS, true));

        // Syntax (for Syntax Highlighting)
        static const char* vars[]={"//","/*","*/","\"'"}; // static storage
        foldingStrings.singleLineComment = vars[0];
        foldingStrings.multiLineCommentStart = vars[1];
        foldingStrings.multiLineCommentEnd = vars[2];
        foldingStrings.stringDelimiterChars = vars[3];
        foldingStrings.stringEscapeChar = '\\';

        // Keywords
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_PREPROCESSOR;
            static const char* vars[] = {"#include","#if","#ifdef","#ifndef","#else","#elif","#endif","#define","#undef","#warning","#error","#pragma"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_ACCESS;
            static const char* vars[] = {"this","base","private","protected","public"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_DECLARATION;
            static const char* vars[] = {"class","struct","enum","union","template","typedef","typename"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_ITERATION;
            static const char* vars[] = {"for","while","do"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_JUMP;
            static const char* vars[] = {"break","continue","return","goto"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_SELECTION;
            static const char* vars[] = {"switch","case","default","if","else"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_OPERATOR;
            static const char* vars[] = {"new","delete","const_cast","dynamic_cast","reinterpret_cast","static_cast","static_assert","slots","signals","typeid","operator","decltype","and","and_eq","not","not_eq","or","or_eq","xor","xor_eq","bitand","bitor"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_EXCEPTION;
            static const char* vars[] = {"try","throw","catch","finally","bad_typeid","bad_cast","noexcept"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_CONSTANT;
            static const char* vars[] = {"true","false","NULL","nullptr","sizeof","alignof","alignas","type_info","type_index","this","asm"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_MODIFIER;
            static const char* vars[] = {"inline","virtual","override","final","explicit","export","friend","mutable","const","static","thread_local","volatile","extern","register"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_TYPE;
            static const char* vars[] = {"bool","void","int","signed","unsigned","long","float","double","short","char","wchar_t","char16_t","char32_t","size_t","ptrdiff_t","max_align_t","offsetof","string","vector","deque","map","unordered_map","set","auto"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_NAMESPACE;
            static const char* vars[] = {"namespace","using"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }

        // Extensions:
        static const char exts[] = ".c;.cpp;.cxx;.cc;.h;.hpp;.hxx"; // static storage
        foldingStrings.languageExtensions = exts;
        // Mandatory call:
        foldingStrings.initSyntaxHighlighting();     // Mandatory

        // Assignment:
        gFoldingStringVectors.push_back(foldingStrings);
	    gFoldingStringVectorIndices[LANG_CPP] = gFoldingStringVectors.size()-1;
        }
        // CS
        {
            FoldingStringVector foldingStrings;
            foldingStrings.push_back(FoldingString ("{", "}", "{...}", FOLDING_TYPE_PARENTHESIS,true));
            foldingStrings.push_back(FoldingString ("#region ", "#endregion", "", FOLDING_TYPE_REGION));
            foldingStrings.push_back (FoldingString ("/*", "*/", "/*...*/", FOLDING_TYPE_COMMENT));
            // Optionals:
            foldingStrings.push_back(FoldingString ("(", ")", "(...)", FOLDING_TYPE_PARENTHESIS, true));
            foldingStrings.push_back(FoldingString ("[", "]", "[...]", FOLDING_TYPE_PARENTHESIS, true));

	    // Syntax (for Syntax Highlighting)
	    static const char* vars[]={"//","/*","*/","\"'"}; // static storage
	    foldingStrings.singleLineComment = vars[0];
	    foldingStrings.multiLineCommentStart = vars[1];
	    foldingStrings.multiLineCommentEnd = vars[2];
	    foldingStrings.stringDelimiterChars = vars[3];
	    foldingStrings.stringEscapeChar = '\\';

            // Keywords
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_PREPROCESSOR;
                static const char* vars[] = {"#if","#else","#elif","#endif","#define","#undef","#warning","#error","#line","#region","#endregion","#pragma"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_ACCESS;
                static const char* vars[] = {"this","base"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_OPERATOR;
                static const char* vars[] = {"as","is","new","sizeof","typeof","stackalloc"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_SELECTION;
                static const char* vars[] = {"else","if","switch","case","default"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_ITERATION;
                static const char* vars[] = {"do","for","foreach","in","while"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_JUMP;
                static const char* vars[] = {"break","continue","goto","return"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_CONTEXT;
                static const char* vars[] = {"yield","partial","global","where","__arglist","__makeref","__reftype","__refvalue","by","descending",
                                             "from","group","into","orderby","select","let","ascending","join","on","equals"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_EXCEPTION;
                static const char* vars[] = {"try","throw","catch","finally"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_CONSTANT;
                static const char* vars[] = {"true","false","null"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_MODIFIER;
                static const char* vars[] = {"abstract","async","await","const","event","extern","override","readonly","sealed","static",
                                             "virtual","volatile","public","protected","private","internal"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_TYPE;
                static const char* vars[] = {"void","bool","byte","char","decimal","double","float","int","long","sbyte"
                                             "short","uint","ushort","ulong","object","string","var","dynamic"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_NAMESPACE;
                static const char* vars[] = {"namespace","using"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_PROPERTY;
                static const char* vars[] = {"get","set","add","remove","value"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_DECLARATION;
                static const char* vars[] = {"class","interface","delegate","enum","struct","explicit","implicit","operator"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_OTHER;
                static const char* vars[] = {"checked","unchecked","fixed","unsafe","lock"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }

            // Extensions:
            static const char exts[] = ".cs";
            foldingStrings.languageExtensions = exts;
            // Mandatory call:
            foldingStrings.initSyntaxHighlighting();     // Mandatory

            // Assignment:
            gFoldingStringVectors.push_back(foldingStrings);
            gFoldingStringVectorIndices[LANG_CS] = gFoldingStringVectors.size()-1;
        }
        // GLSL
        {
        FoldingStringVector foldingStrings; // To fill and push_back
        // Main Folding
        foldingStrings.push_back(FoldingString ("{", "}", "{...}", FOLDING_TYPE_PARENTHESIS,true));
        foldingStrings.push_back(FoldingString ("//region ", "//endregion", "", FOLDING_TYPE_REGION));
        foldingStrings.push_back (FoldingString ("/*", "*/", "/*...*/", FOLDING_TYPE_COMMENT));
        // Optional Folding
        foldingStrings.push_back(FoldingString ("#pragma region ", "#pragma endregion", "", FOLDING_TYPE_REGION));
        foldingStrings.push_back(FoldingString ("(", ")", "(...)", FOLDING_TYPE_PARENTHESIS, true));
        foldingStrings.push_back(FoldingString ("[", "]", "[...]", FOLDING_TYPE_PARENTHESIS, true));

        // Syntax (for Syntax Highlighting)
        static const char* vars[]={"//","/*","*/","\"'"}; // static storage
        foldingStrings.singleLineComment = vars[0];
        foldingStrings.multiLineCommentStart = vars[1];
        foldingStrings.multiLineCommentEnd = vars[2];
        foldingStrings.stringDelimiterChars = vars[3];
        foldingStrings.stringEscapeChar = '\\';

        // Keywords
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_KEYWORD_USER1;
            static const char* vars[] = {"gl_Position","gl_PointSize","gl_ClipVertex","gl_Vertex","gl_Normal","gl_Color","gl_SecondaryColor",
            "gl_MultiTexCoord0","gl_MultiTexCoord1","gl_MultiTexCoord2","gl_MultiTexCoord3","gl_MultiTexCoord4","gl_MultiTexCoord5",
            "gl_MultiTexCoord6","gl_MultiTexCoord7","gl_FogCoord","gl_FrontColor","gl_BackColor","gl_FrontSecondaryColor","gl_BackSecondaryColor",
            "gl_TexCoord","gl_FogFragCoord","gl_FragColor","gl_FragData","gl_FragDepth","gl_Color","gl_SecondaryColor","gl_FragCoord","gl_FrontFacing",
            "gl_VertexID","gl_InstanceID","gl_PointCoord",
            "gl_PatchVerticesIn","gl_PrimitiveID","gl_InvocationID","gl_TessLevelOuter","gl_TessLevelInner","gl_TessCoord","gl_PrimitiveIDIn",
            "gl_Layer","gl_SampleMask"
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_KEYWORD_USER2;
            static const char* vars[] = {"gl_ModelViewMatrix","gl_ModelViewProjectionMatrix","gl_ProjectionMatrix","gl_TextureMatrix",
            "gl_ModelViewMatrixInverse","gl_ModelViewProjectionMatrixInverse","gl_ProjectionMatrixInverse","gl_TextureMatrixInverse",
            "gl_ModelViewMatrixTranspose","gl_ModelViewProjectionMatrixTranspose","gl_ProjectionMatrixInverseTranspose","gl_TextureMatrixInverseTranspose",
            "gl_NormalMatrix","gl_NormalScale",
            "gl_DepthRange","gl_Fog","gl_LightSource","gl_LightModel","gl_FrontLightModelProduct","gl_BackLightModelProduct","gl_FrontLightProduct","gl_BackLightProduct","gl_FrontMaterial","gl_BackMaterial","gl_Point",
            "gl_TextureEnvColor","gl_ClipPlane","gl_EyePlaneS","gl_EyePlaneT","gl_EyePlaneR","gl_EyePlaneQ","gl_ObjectPlaneS","gl_ObjectPlaneT","gl_ObjectPlaneR","gl_ObjectPlaneQ"
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_CONSTANT;
            static const char* vars[] = {
            "gl_MaxVertexUniformComponents","gl_MaxFragmentUniformComponents","gl_MaxVertexAttribs","gl_MaxVaryingFloats","gl_MaxDrawBuffers",
            "gl_MaxTextureCoords","gl_MaxTextureUnits","gl_MaxTextureImageUnits","gl_MaxVertexTextureImageUnits","gl_MaxCombinedTextureImageUnits",
            "gl_MaxLights","gl_MaxClipPlanes",
            "gl_MaxVertexUniformVectors","gl_MaxVertexOutputVectors","gl_MaxFragmentInputVectors","gl_MaxFragmentUniformVectors","gl_MinProgramTexelOffset","gl_MaxProgramTexelOffset",
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_TYPE;
            static const char* vars[] = {"float","vec2","vec3","vec4","int","ivec2","ivec3","ivec4","uint","uvec2","uvec3","uvec4","bool","bvec2","bvec3","bvec4","mat2","mat3","mat4",
            "mat2x2","mat2x3","mat2x4","mat3x2","mat3x3","mat3x4","mat4x2", "mat4x3", "mat4x4","void",
            "double","dvec2", "dvec3","dvec4","dmat2", "dmat3", "dmat4", "dmat2x2", "dmat2x3","dmat2x4","dmat3x2","dmat3x3","dmat3x4","dmat4x2","dmat4x3","dmat4x4",
            "sampler1D","sampler2D","sampler3D","samplerCube","sampler1DShadow","sampler2DShadow","samplerCubeShadow","sampler2DArray","sampler2DArrayShadow",
            "isampler2D","isampler3D","isamplerCube","isampler2DArray","usampler2D","usampler3D","usamplerCube","usampler2DArray",
            "gl_DepthRangeParameters","gl_FogParameters","gl_LightSourceParameters","gl_LightModelParameters","gl_LightModelProducts","gl_LightProducts","gl_MaterialParameters","gl_PointParameters",
            "gl_PerVertex"
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_MODIFIER;
            static const char* vars[] = {"uniform","attribute","varying","const","in","out","inout","centroid","invariant","interpolation","storage","precision","parameter","flat","smooth","layout","location","patch","noperspective",
            "shared", "packed", "std140",
            "highp","mediump","lowp"
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_OPERATOR;
            static const char* vars[] = {"sin","cos","tan","asin","acos","atan","radians","degrees","pow","exp","log","exp2","log2","sqrt","inversesqrt",
            "sinh","cosh","tanh","asinh","acosh","atanh","trunc","round","roundEven","modf",
            "fma","frexp","ldexp","interpolateAtCentroid","interpolateAtSample","interpolateAtOffset",
            "EmitStreamVertex","EndStreamPrimitive","EmitVertex","EndPrimitive","barrier",
            "isnan","isinf","floatBitsToInt","floatBitsToUint","intBitsToFloat","uintBitsToFloat","outerProduct","transpose","determinant","inverse",
            "textureSize","textureOffset","texelFetch","texelFetchOffset","textureProjOffset","textureLodOffset","textureProjLodOffset","textureGrad",
            "textureGradOffset","textureProjGrad",
            "textureProjGradOffset",
            "abs","ceil","clamp","floor","fract","max","min","mix","mod","sign","smoothstep","step","ftrasform","cross","distance","dot","faceforward"
            "length","normalize","reflect","refract","dFdx","dFdy","fwidth","matrixCompMult","all","any","equal","greaterThan","greaterThanEqual","lessThan","lessThanEqual","not","notEqual"
            "texture","texture1D","texture2D","texture3D","textureCube",
            "textureProj","texture1DProj","texture2DProj","texture3DProj",
            "shadow","shadow1D","shadow2D","shadow1DProj","shadow2DProj",
            "textureLod","texture1DLod","texture2DLod","textureCubeLod",
            "textureProjLod","texture1DProjLod","texture2DProjLod","texture3DProjLod",
            "textureGather","textureGatherOffset","textureGatherOffsets",
            "shadowLod","shadow1DLod","shadow2DLod",
            "shadowProjLod","shadow1DProjLod","shadow2DProjLod",
            "noise1","noise2","noise3","noise4",
            "main"//,"","","","","","","","","","","",""
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_PREPROCESSOR;
            static const char* vars[] = {
            "#include",
            "#if","#ifdef","#ifndef","#else","#elif","#endif","#define","#undef","#extension","#line","#error","#pragma","#version","__LINE__","__FILE__","__VERSION__","GL_ES","GL_compatibility_profile"
            };
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_ITERATION;
            static const char* vars[] = {"for","while","do"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_JUMP;
            static const char* vars[] = {"break","continue","return","discard"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_SELECTION;
            static const char* vars[] = {"switch","case","default","if","else"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_DECLARATION;
            static const char* vars[] = {"struct"/*,"class","enum","union","template","typedef","typename"*/};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }
        {
            const SyntaxHighlightingType sht = SH_KEYWORD_ACCESS;
            static const char* vars[] = {"require","enable","warn","disable"};
            const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
        }

        // Extensions:
        static const char exts[] = ".glsl;.vert;.geom;.frag;.vsh;.gsh;.fsh"; // static storage
        foldingStrings.languageExtensions = exts;
        // Mandatory call:
        foldingStrings.initSyntaxHighlighting();     // Mandatory

        // Assignment:
        gFoldingStringVectors.push_back(foldingStrings);
        gFoldingStringVectorIndices[LANG_GLSL] = gFoldingStringVectors.size()-1;
        }
        // LUA
        {
            FoldingStringVector foldingStrings;
            foldingStrings.push_back(FoldingString ("{", "}", "{...}", FOLDING_TYPE_PARENTHESIS,false));
            foldingStrings.push_back(FoldingString ("--region ", "--endregion", "", FOLDING_TYPE_REGION));
            foldingStrings.push_back (FoldingString ("--[[", "--]]", "--...--", FOLDING_TYPE_COMMENT));
            // Optionals:
            foldingStrings.push_back(FoldingString ("(", ")", "(...)", FOLDING_TYPE_PARENTHESIS));
            foldingStrings.push_back(FoldingString ("[", "]", "[...]", FOLDING_TYPE_PARENTHESIS));
            // Syntax (for Syntax Highlighting)
            static const char* vars[]={"--","--[[","]--"}; // static storage
            foldingStrings.singleLineComment = vars[0];
            foldingStrings.multiLineCommentStart = vars[1];
            foldingStrings.multiLineCommentEnd = vars[2];

            // Keywords
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_OPERATOR;
                static const char* vars[] = {"and","or","not"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_SELECTION;
                static const char* vars[] = {"else","elseif","end","if","in","then"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_ITERATION;
                static const char* vars[] = {"do","for","repeat","until","while"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_CONSTANT;
                static const char* vars[] = {"false","nil","true"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_MODIFIER;
                static const char* vars[] = {"global","local"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_DECLARATION;
                static const char* vars[] = {"function"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }

            // Extensions:
            static const char exts[] = ".lua"; // static storage
            foldingStrings.languageExtensions = exts;
            // Mandatory call:
            foldingStrings.initSyntaxHighlighting();     // Mandatory

            // Assignment:
            gFoldingStringVectors.push_back(foldingStrings);
            gFoldingStringVectorIndices[LANG_LUA] = gFoldingStringVectors.size()-1;
        }
        // PYTHON
        {
            FoldingStringVector foldingStrings;
            foldingStrings.push_back(FoldingString ("{", "}", "{...}", FOLDING_TYPE_PARENTHESIS,true));
            foldingStrings.push_back(FoldingString ("#region ", "#endregion", "", FOLDING_TYPE_REGION));
            foldingStrings.push_back (FoldingString ("\"\"\"", "\"\"\"", "\"\"\"...\"\"\"", FOLDING_TYPE_COMMENT));
            // Optionals:
            foldingStrings.push_back(FoldingString ("(", ")", "(...)", FOLDING_TYPE_PARENTHESIS));
            foldingStrings.push_back(FoldingString ("[", "]", "[...]", FOLDING_TYPE_PARENTHESIS));

            // Syntax (for Syntax Highlighting)
            static const char* vars[]={"#","\"\"\"","\"\"\""}; // static storage
            foldingStrings.singleLineComment = vars[0];
            foldingStrings.multiLineCommentStart = vars[1];
            foldingStrings.multiLineCommentEnd = vars[2];

            // Keywords
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_SELECTION;
                static const char* vars[] = {"and","assert","break","class","continue","def","del","elif","else","except",
                                             "exec","finally","for","global","if","in","is","lambda","not","or",
                                             "pass","print","raise","return","try","while","yield","with"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_JUMP;
                static const char* vars[] = {"abs","all","any","apply","basestring","bool","buffer","callable","chr","classmethod",
                                             "cmp","coerce","compile","complex","delattr","dict","dir","divmod","enumerate","eval",
                                             "execfile","file","filter","float","frozenset","getattr","globals","hasattr","hash","hex",
                                             "id","input","int","intern","isinstance","issubclass","iter","len","list","locals",
                                             "long","map","max","min","object","oct","open","ord","pow","property",
                                             "range","raw_input","reduce","reload","repr","reversed","round","setattr","set","slice",
                                             "sorted","staticmethod","str","sum","super","tuple","type","unichr","unicode","vars",
                                             "xrange","zip"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_CONTEXT;
                static const char* vars[] = {"False","None","True"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_EXCEPTION;
                static const char* vars[] = {"ArithmeticError","AssertionError","AttributeError","EnvironmentError","EOFError","Exception",
                                             "FloatingPointError","ImportError","IndentationError","IndexError",
                                             "IOError","KeyboardInterrupt","KeyError","LookupError","MemoryError",
                                             "NameError","NotImplementedError","OSError","OverflowError","ReferenceError",
                                             "RuntimeError","StandardError","StopIteration","SyntaxError","SystemError",
                                             "SystemExit","TabError","TypeError","UnboundLocalError","UnicodeDecodeError",
                                             "UnicodeEncodeError","UnicodeError","UnicodeTranslateError","ValueError","WindowsError",
                                             "ZeroDivisionError","Warning","UserWarning","DeprecationWarning","PendingDeprecationWarning",
                                             "SyntaxWarning","OverflowWarning","RuntimeWarning","FutureWarning"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_TYPE;
                static const char* vars[] = {"BufferType","BuiltinFunctionType","BuiltinMethodType","ClassType","CodeType",
                                             "ComplexType","DictProxyType","DictType","DictionaryType","EllipsisType",
                                             "FileType","FloatType","FrameType","FunctionType","GeneratorType",
                                             "InstanceType","IntType","LambdaType","ListType","LongType",
                                             "MethodType","ModuleType","NoneType","ObjectType","SliceType",
                                             "StringType","StringTypes","TracebackType","TupleType","TypeType",
                                             "UnboundMethodType","UnicodeType","XRangeType"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_NAMESPACE;
                static const char* vars[] = {"import","from","as"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }
            {
                const SyntaxHighlightingType sht = SH_KEYWORD_OTHER;
                static const char* vars[] = {"__abs__","__add__","__all__","__author__","__bases__",
                                             "__builtins__","__call__","__class__","__cmp__","__coerce__",
                                             "__contains__","__debug__","__del__","__delattr__","__delitem__",
                                             "__delslice__","__dict__","__div__","__divmod__","__doc__",
                                             "__docformat__","__eq__","__file__","__float__","__floordiv__",
                                             "__future__","__ge__","__getattr__","__getattribute__","__getitem__",
                                             "__getslice__","__gt__","__hash__","__hex__","__iadd__",
                                             "__import__","__imul__","__init__","__int__","__invert__",
                                             "__iter__","__le__","__len__","__long__","__lshift__",
                                             "__lt__","__members__","__metaclass__","__mod__","__mro__",
                                             "__mul__","__name__","__ne__","__neg__","__new__",
                                             "__nonzero__","__oct__","__or__","__path__","__pos__",
                                             "__pow__","__radd__","__rdiv__","__rdivmod__","__reduce__",
                                             "__repr__","__rfloordiv__","__rlshift__","__rmod__","__rmul__",
                                             "__ror__","__rpow__","__rrshift__","__rsub__","__rtruediv__",
                                             "__rxor__","__setattr__","__setitem__","__setslice__","__self__",
                                             "__slots__","__str__","__sub__","__truediv__","__version__",
                                             "__xor__"};
                const int varsSize = (int)sizeof(vars)/sizeof(vars[0]);foldingStrings.keywords[sht].reserve(foldingStrings.keywords[sht].size()+varsSize);for (int i=0;i<varsSize;i++) foldingStrings.keywords[sht].push_back(vars[i]);
            }

            // Extensions:
            static const char exts[] = ".py";
            foldingStrings.languageExtensions = exts;
            // Mandatory call:
            foldingStrings.initSyntaxHighlighting();     // Mandatory


            // Assignment:
            gFoldingStringVectors.push_back(foldingStrings);
            gFoldingStringVectorIndices[LANG_PYTHON] = gFoldingStringVectors.size()-1;
        }
    }
}
void CodeEditor::StaticInit()   {
    if (StaticInited) return;
    if (!ImFonts[FONT_STYLE_NORMAL]) SetFonts(NULL);
    if (gFoldingStringVectors.size()==0) InitFoldingStringVectors();

    gTotalLanguageExtensionFilter = "";
    for (int lng=0;lng<LANG_COUNT;lng++)    {
    const int id = gFoldingStringVectorIndices[lng];
    if (id<0) continue;
    const FoldingStringVector& fsv = gFoldingStringVectors[id];
    if (fsv.languageExtensions && strlen(fsv.languageExtensions)>0)	{
        if (gTotalLanguageExtensionFilter.size()>0) gTotalLanguageExtensionFilter+=";";
        gTotalLanguageExtensionFilter+=fsv.languageExtensions;
    }
    }

    StaticInited = true;
}
void CodeEditor::init() {
    inited = true;
    StaticInit();
}


bool CodeEditor::StaticInited = false;
inline static FoldingStringVector* GetGlobalFoldingStringVectorForLanguage(Language language)    {
    const int index = gFoldingStringVectorIndices[language];
    return (index>=0 && index<gFoldingStringVectors.size()) ? &gFoldingStringVectors[index] : NULL;
}
bool CodeEditor::HasFoldingSupportForLanguage(Language language)    {
    return (GetGlobalFoldingStringVectorForLanguage(language)!=NULL);
}
bool CodeEditor::SetFoldingSupportForLanguage(Language language, const ImVectorEx<FoldingTag> &foldingTags, bool mergeAdditionalTrailingCharIfPossible, char additionalTrailingChar)  {
    IM_ASSERT(!StaticInited);   // you must use the static methods before rendering or initing any instance of the Code Editor
    FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(language);
    if (!fsv) {
        gFoldingStringVectors.push_back(FoldingStringVector());
        gFoldingStringVectorIndices[language]= gFoldingStringVectors.size()-1;
        fsv = &gFoldingStringVectors[gFoldingStringVectors.size()-1];
    }
    if (!fsv) return false;
    fsv->clear();
    *fsv = FoldingStringVector(foldingTags,mergeAdditionalTrailingCharIfPossible,additionalTrailingChar);
    return true;
}
bool CodeEditor::AddSyntaxHighlightingTokens(Language language, SyntaxHighlightingType type, const char **tokens, int numTokens)
{
    IM_ASSERT(!StaticInited);   // you must use the static methods before rendering or initing any instance of the Code Editor
    IM_ASSERT(tokens && numTokens>0);
    IM_ASSERT(language<LANG_COUNT);
    IM_ASSERT(type<SH_LOGICAL_OPERATORS);   // Only keywords here please
    FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(language);
    IM_ASSERT(fsv); // No time to check this
    if (!fsv) return false;
    for (int i=0;i<numTokens;i++)  fsv->keywords[type].push_back(tokens[i]);
    return true;
}

bool CodeEditor::ClearSyntaxHighlightingTokens(Language language, SyntaxHighlightingType type)
{
    IM_ASSERT(!StaticInited);   // you must use the static methods before rendering or initing any instance of the Code Editor
    IM_ASSERT(language<LANG_COUNT);
    IM_ASSERT(type<SH_LOGICAL_OPERATORS);   // Only keywords here please
    FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(language);
    IM_ASSERT(fsv); // No time to check this
    if (!fsv) return false;
    fsv->keywords[type].clear();
    return true;
}

bool CodeEditor::SetSyntaxHighlightingExtraStuff(Language language, const char *singleLineComment, const char *stringDelimiters, const char *logicalOperators, const char *mathOperators, const char *punctuation)
{
    IM_ASSERT(!StaticInited);   // you must use the static methods before rendering or initing any instance of the Code Editor
    IM_ASSERT(language<LANG_COUNT);
    FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(language);
    IM_ASSERT(fsv); // No time to check this
    if (!fsv) return false;
    if (singleLineComment) fsv->singleLineComment = singleLineComment;
    if (stringDelimiters)  fsv->punctuationStrings[SH_STRING-SH_LOGICAL_OPERATORS]              = stringDelimiters;
    if (logicalOperators)  fsv->punctuationStrings[SH_LOGICAL_OPERATORS-SH_LOGICAL_OPERATORS]   = logicalOperators;
    if (mathOperators)  fsv->punctuationStrings[SH_MATH_OPERATORS-SH_LOGICAL_OPERATORS]         = mathOperators;
    if (punctuation)  fsv->punctuationStrings[SH_PUNCTUATION-SH_LOGICAL_OPERATORS]              = punctuation;
    return true;
}

Language CodeEditor::GetLanguageFromFilename(const char *filename)
{
    if (!filename || strlen(filename)==0) return LANG_NONE;
    const char* ext = strrchr(filename,'.');
    if (!ext || strlen(ext)==0) return LANG_NONE;
    const int ext_len = strlen(ext);
    for (int i=0,isz=LANG_COUNT;i<isz;i++)  {
        const int index = gFoldingStringVectorIndices[i];
        if (index<0 || index>=gFoldingStringVectors.size()) continue;
        const FoldingStringVector* fsv = &gFoldingStringVectors[index];
        if (!fsv) continue;
        const char* exts = fsv->languageExtensions; // e.g. ".cpp;.h;.hpp"
        if (!exts || strlen(exts)==0) continue;
        //fprintf(stderr,"exts[%d]=\"%s\" (ext=\"%s\") i=%d\n",i,exts,ext,i);
        const char* ex = ext;
        int cnt=0;
        for (int j=0,jsz=strlen(exts);j<jsz;j++)    {
            const char* ch = &exts[j];
            while (*ex++==*ch++) {
                ++cnt;
                if (cnt==ext_len) return (Language) i;
            }
            ex = ext;cnt=0;
        }

    }
    return LANG_NONE;
}

#ifndef NO_IMGUICODEEDITOR_SAVE
bool CodeEditor::save(const char* filename) {
    if (!filename || strlen(filename)==0) return false;
    ImString text = "";
    lines.getText(text);
    FILE* f = ImFileOpen(filename,"w");
    if (!f) return false;
    // TODO: UTF8 BOM here ?
    fwrite((const void*) text.c_str(),text.size(),1,f);
    fclose(f);
    return true;
}
#endif //NO_IMGUICODEEDITOR_SAVE
#ifndef NO_IMGUICODEEDITOR_LOAD
bool CodeEditor::load(const char* filename, Language optionalLanguage) {
    if (!filename || strlen(filename)==0) return false;
    ImVector<char> text;
    FILE* f = ImFileOpen(filename,"r");
    if (!f) return false;
    fseek(f,0,SEEK_END);
    const size_t length = ftell(f);
    fseek(f,0,SEEK_SET);
    // TODO: UTF8 BOM here ?
    text.resize(length+1);
    fread((void*) &text[0],length,1,f);
    text[length]='\0';
    fclose(f);
    if (optionalLanguage==LANG_COUNT) {
        optionalLanguage = GetLanguageFromFilename(filename);
    }
    setText(length>0 ? &text[0] : "",optionalLanguage);
    return true;
}
#endif //NO_IMGUICODEEDITOR_LOAD
void CodeEditor::setText(const char *text, Language _lang) {
    if (lang!=_lang)    {
        shTypeKeywordMap.clear();
        shTypePunctuationMap.clear();

        FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(_lang);
        if (fsv)    {
            // replace our keywords hashmap
            for (int i=0,isz = SH_LOGICAL_OPERATORS;i<isz;i++)  {
                const ImVectorEx<const char*>& v = fsv->keywords[i];
                for (int j=0,jsz=v.size();j<jsz;j++) {
                    shTypeKeywordMap.put((MyKeywordMapType::KeyType)v[j],i);
                    //fprintf(stderr,"Putting in shTypeMap: \"%s\",%d\n",v[j],i);
                }
            }
            // replace our punctuation hashmap
            for (int i=0,isz = strlen(fsv->punctuationStringsMerged);i<isz;i++)  {
                const char c = fsv->punctuationStringsMerged[i];
                shTypePunctuationMap.put(c,fsv->punctuationStringsMergedSHMap[i]);
                //fprintf(stderr,"Putting in shTypePunctuationMap: '%c',%d\n",c,fsv->punctuationStringsMergedSHMap[i]);
            }
        }
    }
    lang = _lang;
    lines.setText(text);
    if (enableTextFolding) {
        ParseTextForFolding(false,true);
    }
}

inline static ImString TrimSpacesAndTabs(const ImString& sIn)    {
    ImString tOut = sIn;
    if (sIn.size()<1) return tOut;
    int sz=0;
    while ((sz=tOut.size())>0)   {
	if (tOut[0]==' ' || tOut[0]=='\t')  {tOut=tOut.substr(1);continue;}
	--sz;
	if (tOut[sz]==' ' || tOut[sz]=='\t')  {tOut=tOut.substr(0,sz);continue;}
	break;
    }
    return tOut;
}

void CodeEditor::ParseTextForFolding(bool forceAllSegmentsFoldedOrNot, bool foldingStateToForce) {
    // Naive algorithm: TODO: rewrite to handle "//" and strings correctly

    if (!enableTextFolding || lines.size()==0) return;
    FoldingStringVector* pFoldingStrings = GetGlobalFoldingStringVectorForLanguage(lang);
    if (!pFoldingStrings || pFoldingStrings->size()==0) return;
    FoldingStringVector& foldingStrings = *pFoldingStrings;
    //--------------------- START --------------------------------------------

    foldingStrings.resetAllTemporaryData();

    ImVectorEx<FoldSegment> foldSegments;

    bool acceptStartMultilineCommentFolding = true;	//internal, do not touch
    Line* line=NULL;
    char ch;const int numLines = lines.size();
    const int singleLineCommentSize = foldingStrings.singleLineComment ? strlen(foldingStrings.singleLineComment) : 0;
    int firstValidCharPos = -1;
    for (int i=0;i<numLines; i++) {
        line = lines[i];
        if (!line) continue;
        const ImString& text = line->text;
        if (text.size() == 0)   continue;
        foldingStrings.resetTemporaryLineData();

        firstValidCharPos = -1; // calculated only if singleLineCommentSize>0 ATM
    for (int ti=0,tisz=text.length(); ti<tisz; ti++) {
	    const char c = text [ti];
        if (firstValidCharPos==-1 && singleLineCommentSize && (!(c==' ' || c=='\t'))) {
            firstValidCharPos = ti;
            // check if it's a line comment so we can exit early and prevent tokens after "//" to be incorrectly detected.
            // TODO: shouldn't we exit even when we're not "firstValidCharPos" ? YES, but doing proper code folding is DIFFICULT... so we don't do it.
            const char* ptext = &text[ti];
            if (strncmp(ptext,foldingStrings.singleLineComment,singleLineCommentSize)==0) {
                //fprintf(stderr,"%d) %c (%s %d)\n",i+1,c,foldingStrings.singleLineComment,singleLineCommentSize);
                // Ok, but we can't skip regions: (e.g. "//region Blah blah blah" or "//endregion")
                bool mustSkip = true;
                for (int fsi=0,fsisz=foldingStrings.size();fsi<fsisz;fsi++) {
                    const FoldingString& fs = foldingStrings[fsi];
                    if (fs.kind!=FOLDING_TYPE_REGION) continue;
                    if ( (strncmp(ptext,fs.start.c_str(),fs.start.size())==0) || (strncmp(ptext,fs.end.c_str(),fs.end.size())==0) ) {
                        mustSkip = false;
                        break;
                    }
                }
                if (mustSkip) break;   // Skip line
            }
        }
	    for (int fsi=0,fsisz=foldingStrings.size();fsi<fsisz;fsi++) {
		FoldingString& fs = foldingStrings[fsi];
		if (acceptStartMultilineCommentFolding || fs.kind != FOLDING_TYPE_COMMENT) {
		    ch = fs.start[fs.curStartCharIndex];
            if (fs.lookForCustomTitle && fs.foldingPoints.size() > 0)   fs.foldingPoints[fs.foldingPoints.size() - 1].customTitle += c;// = fs.foldingPoints[fs.foldingPoints.Count-1].CustomTitle + c.ToString ();
            else if (c == ch) {
                if (fs.matchStartBeg < 0)   fs.matchStartBeg = ti;
                ++fs.curStartCharIndex;
                if (fs.curStartCharIndex == fs.start.length()) {
                    //Tmatch-------------
                    //Console.WriteLine ("Match: \""+fs.Start+"\" at line: "+(i)+" and column: "+fs.matchStartBeg);

                    bool gainOneLine = false;
                    if (fs.gainOneLineWhenPossible && i > 1 && TrimSpacesAndTabs(text.substr(0, fs.matchStartBeg)).length() == 0) {
                        Line* prevline = lines[i - 1];
                        if (prevline) {
                            ImString prevText = prevline->text;
                            if (TrimSpacesAndTabs(prevText).length() != 0) {
                                gainOneLine = true;
                                //fs.foldingPoints.push_back(FoldingString::FoldingPoint (i - 1, prevText.length(), prevline->offset + prevText.length(), true, fs.openCnt));
                                fs.foldingPoints.push_back(FoldingString::FoldingPoint (i,line,fs.matchStartBeg,true,fs.openCnt,gainOneLine));
                            }
                        }
                    }
                    if (!gainOneLine) fs.foldingPoints.push_back(FoldingString::FoldingPoint (i,line,fs.matchStartBeg,true,fs.openCnt,gainOneLine));

                            if (fs.title.length() == 0)  fs.lookForCustomTitle = true;
                    ++fs.openCnt;
                    //------------------------
                    fs.curStartCharIndex = 0;
                    fs.matchStartBeg = -1;	//reset matcher
                    if (fs.kind == FOLDING_TYPE_COMMENT) {
                        acceptStartMultilineCommentFolding = false;
                        fs.curEndCharIndex = 0;
                        fs.matchEndBeg = -1;	//reset end matcher too
                    }
                }
            } else {
                fs.curStartCharIndex = 0;
                fs.matchStartBeg = -1;	//reset
            }
		}

		ch = fs.end [fs.curEndCharIndex];
		if (c == ch) {
		    if (fs.matchEndBeg < 0) fs.matchEndBeg = ti;
		    ++fs.curEndCharIndex;
		    if (fs.curEndCharIndex == fs.end.length()) {
			--fs.openCnt;
			//match-------------
			//Console.WriteLine ("Match: \""+fs.End+"\" at line: "+(i)+" and column: "+fs.matchEndBeg);
            fs.foldingPoints.push_back (FoldingString::FoldingPoint (i,line, fs.matchEndBeg,false,fs.openCnt));
			// region Add folding region (copying some data from regions already present)
			if (fs.foldingPoints.size() >= 2) {
                FoldingString::FoldingPoint& endPoint = fs.foldingPoints [fs.foldingPoints.size() - 1];
                FoldingString::FoldingPoint* startPoint = fs.getMatchingFoldingPoint(fs.foldingPoints.size() - 1);
                if (startPoint && endPoint.lineNumber == startPoint->lineNumber) {
                    // HP)
                    // Inside here 'startPoint' and 'endPoint' refer to the same line of text AND
                    // this same line is the one pointed by 'line' and 'text'
                    const bool foldSingleLineRegions = false;
                    if (!/*fs.*/foldSingleLineRegions)  startPoint = NULL;	// = do not fold because the user doesn't want to
                    else    {
                        //There are still cases in which we want not to fold the single line, even if user wants to
                        int netStartColumn = startPoint->lineOffset + fs.start.length();
                        int netEndColumn = endPoint.lineOffset - fs.end.length();
                        int netLength = netEndColumn - netStartColumn;
                        if (netLength < 1 || TrimSpacesAndTabs(text.substr(netStartColumn, netLength)).length() < 2)  startPoint = NULL;	// = do not fold
                    }
                }
                if (startPoint) {
				//bool useCustomTitle = (fs.title.length() == 0);
		//ImString title = useCustomTitle ? TrimSpacesAndTabs(startPoint->customTitle) : fs.title;

		FoldSegment newSegment(&fs,
                                       startPoint->lineNumber,startPoint->line,startPoint->lineOffset,
                                       endPoint.lineNumber,endPoint.line,endPoint.lineOffset,
				       startPoint->canGainOneLine);

				//Check to see if the same folding was already present (we will replace all present foldings),
				//and retrive if it was folded or not
                if (!forceAllSegmentsFoldedOrNot) newSegment.isFolded = newSegment.startLine->isFolded();
                else  newSegment.isFolded = foldingStateToForce;

				foldSegments.push_back(newSegment);
				//Console.WriteLine("Added Folding: "+title+ " Fold Region Length = "+(endPoint.Offset - startPoint.Offset));
			    }
			}
			// endregion
			//------------------------
			fs.curEndCharIndex = 0;
			fs.matchEndBeg = -1;	//reset
			if (fs.kind == FOLDING_TYPE_COMMENT/* && fs.openCnt<=0*/)   acceptStartMultilineCommentFolding = true;
		    }
		} else {
		    fs.curEndCharIndex = 0;
		    fs.matchEndBeg = -1;	//reset
		}
	    }

	}

    }

    // Reset Line foldings
    for (int lni=0,lnisz=lines.size();lni<lnisz;lni++)	{
	Line* line = lines[lni];
	line->resetFoldingAttributes();
    }

    // Now use "foldSegments" to update the folding state of all lines.
    for (int i=0,isz=foldSegments.size();i<isz;i++) {
	const FoldSegment& fs = foldSegments[i];

    //fprintf(stderr,"foldSegments[%d]: ",i);fs.fprintf();fprintf(stderr,"\n");

    // start folding line
    fs.startLine->attributes|=(Line::AT_FOLDING_START | (fs.canGainOneLine?Line::AT_SAVE_SPACE_LINE:0));
    fs.startLine->foldingStartTag = fs.matchingTag;
    fs.startLine->foldingEndLine = fs.endLine;
    fs.startLine->foldingStartOffset = fs.startLineOffset;
    fs.startLine->setFoldedState(fs.isFolded);
    // lines in between
    int nestedFoldings = 0;
    for (int j=fs.startLineIndex+1;j<fs.endLineIndex;j++)   {
        Line* line = lines[j];
        if (fs.isFolded) line->setHidden(true);
        // Testing Stuff ------------
        if (line->isFoldable()) {
            if (nestedFoldings==0) line->foldingStartLine = fs.startLine;
            ++nestedFoldings;
            continue;
        }
        if (line->isFoldingEnd()) {
            --nestedFoldings;
            if (nestedFoldings==0) line->foldingEndLine = fs.endLine;
            continue;
        }
        if (nestedFoldings==0)  {
            line->foldingStartLine = fs.startLine;
            line->foldingEndLine = fs.endLine;
            //fprintf(stderr,"Setting: line[%d] foldingStartLine=%d foldingEndLine=%d\n",j+1,line->foldingStartLine->lineNumber+1,line->foldingEndLine->lineNumber+1);
        }
        // End Testing Stuff ------------
    }

    // end folding line
    fs.endLine->attributes|=Line::AT_FOLDING_END;
    fs.endLine->foldingEndOffset = fs.endLineOffset;
    fs.endLine->foldingStartLine = fs.startLine;

    // Fix: remove "canGainOneLine" to lines whose previous line was foldable itself
    if (fs.startLine->canFoldingBeMergedWithLineAbove())    {
        if (fs.startLine->lineNumber-1>=0 && lines[fs.startLine->lineNumber-1]->isFoldable())  fs.startLine->attributes&=~Line::AT_SAVE_SPACE_LINE;
    }
    else if (fs.startLine->isFoldable()) {
        if (fs.startLine->lineNumber+1<lines.size() && lines[fs.startLine->lineNumber+1]->canFoldingBeMergedWithLineAbove()) lines[fs.startLine->lineNumber+1]->attributes&=~Line::AT_SAVE_SPACE_LINE;
    }

    }



}


static const SyntaxHighlightingType FoldingTypeToSyntaxHighlightingType[FOLDING_TYPE_REGION+1] = {SH_FOLDED_PARENTHESIS,SH_FOLDED_COMMENT,SH_FOLDED_REGION};

// Global temporary variables:
static bool gCurlineStartedWithDiesis;
static Line* gCurline;
static bool gIsCurlineHovered;
static bool gIsCursorChanged;

// Main method
void CodeEditor::render()   {
    if (lines.size()==0) return;
    if (!inited) init();
    static const ImVec4 transparent(1,1,1,0);

    ImGuiIO& io = ImGui::GetIO();

    // Init
    if (!ImFonts[0]) {
        if (io.Fonts->Fonts.size()==0) return;
        for (int i=0;i<FONT_STYLE_COUNT;i++) ImFonts[i]=io.Fonts->Fonts[0];
    }



    bool leftPaneHasMouseCursor = false;
    if (show_left_pane) {
        // Helper stuff for setting up the left splitter
        static ImVec2 lastWindowSize=ImGui::GetWindowSize();      // initial window size
        ImVec2 windowSize = ImGui::GetWindowSize();
        const bool windowSizeChanged = lastWindowSize.x!=windowSize.x || lastWindowSize.y!=windowSize.y;
        if (windowSizeChanged) lastWindowSize = windowSize;
        static float w = lastWindowSize.x*0.2f;                    // initial width of the left window

        //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

        ImGui::BeginChild("code_editor_left_pane", ImVec2(w,0));

        ImGui::Spacing();
        if (show_style_editor)   {
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Style Editor##styleEditor",NULL,false))   {
                ImGui::Separator();
                // Color Mode
                static const char* btnlbls[2]={"HSV##myColorBtnType","RGB##myColorBtnType"};
                if (colorEditMode!=ImGuiColorEditMode_RGB)  {
                    if (ImGui::SmallButton(btnlbls[0])) {
                        colorEditMode = ImGuiColorEditMode_RGB;
                        ImGui::ColorEditMode(colorEditMode);
                    }
                }
                else if (colorEditMode!=ImGuiColorEditMode_HSV)  {
                    if (ImGui::SmallButton(btnlbls[1])) {
                        colorEditMode = ImGuiColorEditMode_HSV;
                        ImGui::ColorEditMode(colorEditMode);
                    }
                }
                ImGui::SameLine(0);ImGui::Text("Color Mode");
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::ColorEditMode(colorEditMode);
                Style::Edit(this->style);
                ImGui::Separator();
#if             (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
                const char* saveName = "codeEditor.ce.style";
                const char* saveNamePersistent = "/persistent_folder/codeEditor.ce.style";
                const char* pSaveName = saveName;
#               ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
                if (ImGui::SmallButton("Save##saveGNEStyle")) {
#                   ifndef NO_IMGUIEMSCRIPTEN
                    pSaveName = saveNamePersistent;
#                   endif //NO_IMGUIEMSCRIPTEN
                    if (Style::Save(this->style,pSaveName)) {
#                   ifndef NO_IMGUIEMSCRIPTEN
                    ImGui::EmscriptenFileSystemHelper::Sync();
#                   endif //NO_IMGUIEMSCRIPTEN
                    }
                }
                ImGui::SameLine();
#               endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#               ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
                if (ImGui::SmallButton("Load##loadGNEStyle")) {
#                   ifndef NO_IMGUIEMSCRIPTEN
                    if (ImGuiHelper::FileExists(saveNamePersistent)) pSaveName = saveNamePersistent;
#                   endif //NO_IMGUIEMSCRIPTEN
                    Style::Load(this->style,pSaveName);
                }
                ImGui::SameLine();
#               endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#               endif //NO_IMGUIHELPER_SERIALIZATION

                if (ImGui::SmallButton("Reset##resetGNEStyle")) {
                    Style::Reset(this->style);
                }
            }
            ImGui::Separator();
        }
#if ((!(defined(NO_IMGUICODEEDITOR_SAVE)) || (!defined(NO_IMGUICODEEDITOR_LOAD))))
    if (show_load_save_buttons) {
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Load/Save File##serialization",NULL,false))   {
        ImGui::Separator();
#       ifdef NO_IMGUIFILESYSTEM
        const char* saveName = "myCodeEditorFile.cpp";
#       ifndef NO_IMGUICODEEDITOR_SAVE
        if (ImGui::SmallButton("Save##saveGNE")) {
            save(saveName);
        }
        ImGui::SameLine();
#       endif //NO_IMGUICODEEDITOR_SAVE
#       ifndef NO_IMGUICODEEDITOR_LOAD
        if (ImGui::SmallButton("Load##loadGNE")) {
            load(saveName);
        }
#		endif //NO_IMGUICODEEDITOR_LOAD
#       else //NO_IMGUIFILESYSTEM
        const char* chosenPath = NULL;                
#       ifndef NO_IMGUICODEEDITOR_LOAD
        static ImGuiFs::Dialog fsInstanceLoad;
        const bool loadButtonPressed = ImGui::SmallButton("Load##loadCE");
	chosenPath = fsInstanceLoad.chooseFileDialog(loadButtonPressed,fsInstanceLoad.getChosenPath(),gTotalLanguageExtensionFilter.c_str()/*fsv ? fsv->languageExtensions: ""*/);
        if (strlen(chosenPath)>0) load(chosenPath);
        ImGui::SameLine();
#		endif //NO_IMGUICODEEDITOR_LOAD
#       ifndef NO_IMGUICODEEDITOR_SAVE
        static ImGuiFs::Dialog fsInstanceSave;
        const bool saveButtonPressed = ImGui::SmallButton("Save##saveCE");
	const FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(lang);
        chosenPath = fsInstanceSave.saveFileDialog(saveButtonPressed,fsInstanceSave.getChosenPath(),fsv ? fsv->languageExtensions: "");
        if (strlen(chosenPath)>0) save(chosenPath);
#       endif //NO_IMGUICODEEDITOR_SAVE
#       endif //NO_IMGUIFILESYSTEM
        }
#       endif // (!(defined(NO_IMGUICODEEDITOR_SAVE) || !defined(NO_IMGUICODEEDITOR_LOAD)))
        ImGui::Separator();
    }
        ImGui::EndChild();


        // horizontal splitter
        ImGui::SameLine(0);
        static const float splitterWidth = 6.f;

        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(1,1,1,0.2f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(1,1,1,0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(1,1,1,0.5f));
        ImGui::Button("##hsplitter1", ImVec2(splitterWidth,-1));
        ImGui::PopStyleColor(3);
        const bool splitterActive = ImGui::IsItemActive();
        if (ImGui::IsItemHovered() || splitterActive) {
            leftPaneHasMouseCursor = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (splitterActive)  w += ImGui::GetIO().MouseDelta.x;
        if (splitterActive || windowSizeChanged)  {
            const float minw = ImGui::GetStyle().WindowPadding.x + ImGui::GetStyle().FramePadding.x;
            const float maxw = minw + windowSize.x - splitterWidth - ImGui::GetStyle().WindowMinSize.x - ImGui::GetStyle().WindowPadding.x - ImGui::GetStyle().ItemSpacing.x;
            if (w>maxw)         w = maxw;
            else if (w<minw)    w = minw;
        }
        ImGui::SameLine(0);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, style.color_background);  // The line below is just to set the bg color....
    ImGui::BeginChild("CodeEditorChild", ImVec2(0,0), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

    if (!io.FontAllowUserScaling && io.MouseWheel && ImGui::GetCurrentWindow()==GImGui->HoveredWindow)   {
        // Zoom / Scale window
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();//g.HoveredWindow;
        float new_font_scale = ImClamp(window->FontWindowScale + g.IO.MouseWheel * 0.10f, 0.50f, 2.50f);
        float scale = new_font_scale / window->FontWindowScale;
        window->FontWindowScale = new_font_scale;

        const ImVec2 offset = (g.IO.MousePos - window->Pos) * (1.0f - scale);
        window->Pos += offset;
        window->PosFloat += offset;
        // these two don't affect child windows AFAIK
        //window->Size *= scale;
        //window->SizeFull *= scale;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

    const ImVec2 windowPos = ImGui::GetWindowPos();
    //const ImVec2 windowSize = ImGui::GetWindowSize();
    const float windowScale = ImGui::GetCurrentWindow()->FontWindowScale;

    const float lineHeight = ImGui::GetTextLineHeight();    // This is the font size too
    int lineStart,lineEnd;
    ImGui::CalcListClipping(lines.size(),lineHeight, &lineStart, &lineEnd);
    // Ensure that lineStart is not hidden
    while (lineStart<lines.size()-1 && lineStart>0 && (lines[lineStart]->isHidden() || (lines[lineStart]->canFoldingBeMergedWithLineAbove() && lines[lineStart]->isFolded()))) {
        if (io.MouseWheel>=0) {--lineStart;/*--lineEnd;*/}
        else {++lineStart;++lineEnd;}
    }
    if (scrollToLine>=0) {
        if (lineStart>scrollToLine)  lineStart = scrollToLine;
        else if (lineEnd<=scrollToLine)   lineEnd = scrollToLine+1;
        else scrollToLine = -1;   // we reset it now
        if (scrollToLine>=0)  {
            //TODO: ensure that scrollLine is visible by unfolding lines
        }
    }
    if (lineEnd>=lines.size()) lineEnd = lines.size()-1;

    const ImVec2 btnSize(lineHeight,lineHeight);    // used in showIconMagin
    float lineNumberSize = 0;                       // used in showLineNumbers
    int precision =  0;
    if (showLineNumbers)    { // No time for a better approach
        ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_line_numbers]));
        if (lineEnd<9)          {precision=1;lineNumberSize=ImGui::MyCalcTextWidth("9");}
        else if (lineEnd<99)    {precision=2;lineNumberSize=ImGui::MyCalcTextWidth("99");}
        else if (lineEnd<999)   {precision=3;lineNumberSize=ImGui::MyCalcTextWidth("999");}
        else if (lineEnd<9999)  {precision=4;lineNumberSize=ImGui::MyCalcTextWidth("9999");}
        else if (lineEnd<99999) {precision=5;lineNumberSize=ImGui::MyCalcTextWidth("99999");}
        else if (lineEnd<999999){precision=6;lineNumberSize=ImGui::MyCalcTextWidth("999999");}
        else                    {precision=7;lineNumberSize=ImGui::MyCalcTextWidth("9999999");}
        ImGui::PopFont();
    }
    ImGui::PushFont(const_cast<ImFont*>(ImFonts[FONT_STYLE_NORMAL]));
    const float sizeFoldingMarks = ImGui::MyCalcTextWidth("   ");    // Wrong, but no time to fix it
    const ImVec2 startCursorPosIconMargin(ImGui::GetCursorPosX(),ImGui::GetCursorPosY() + (lineStart * lineHeight));
    const ImVec2 startCursorPosLineNumbers(startCursorPosIconMargin.x + (showIconMargin ? btnSize.x : 0.f),startCursorPosIconMargin.y);
    const ImVec2 startCursorPosFoldingMarks(startCursorPosLineNumbers.x + (enableTextFolding ? lineNumberSize : 0.f),startCursorPosLineNumbers.y);
    const ImVec2 startCursorPosTextEditor(startCursorPosFoldingMarks.x + sizeFoldingMarks,startCursorPosFoldingMarks.y);

    visibleLines.clear();

    // Draw text and fill visibleLines
    {
        ImGui::SetCursorPos(startCursorPosTextEditor);
        ImGui::BeginGroup();
        gIsCursorChanged = false;
        const float folded_region_contour_thickness = style.folded_region_contour_thickness * windowScale;
        ImGui::PushStyleColor(ImGuiCol_Text,style.color_text);
        ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_text]));
        bool mustSkipNextVisibleLine = false;
        for (int i = lineStart;i<=lineEnd;i++) {
            if (i>=lines.size()) break;
            if (i==scrollToLine) ImGui::SetScrollPosHere();
            Line* line = lines[i];
            if (line->isHidden()) {
                //fprintf(stderr,"line %d is hidden\n",line->lineNumber+1); // This seems to happen on "//endregion" lines only
                ++lineEnd;
                continue;
            }

            if (mustSkipNextVisibleLine) mustSkipNextVisibleLine = false;
            else visibleLines.push_back(line);

            gCurlineStartedWithDiesis = gIsCurlineHovered = false;
            gCurline = line;
            // ImGui::PushID(line);// ImGui::PopID();
            //if (line->isFoldable()) {fprintf(stderr,"Line[%d] is foldable\n",line->lineNumber);}

            if (line->isFoldable()) {
                const int startOffset = (line->isFoldingEnd() && line->foldingStartLine->isFolded()) ? (line->foldingEndOffset+line->foldingStartLine->foldingStartTag->end.size()) : 0;
                if (line->isFolded()) {
                    // start line of a folded region

                    // draw sh text before the folding point
                    if (line->foldingStartOffset>0) {
                        this->TextLineWithSH("%.*s",line->foldingStartOffset-startOffset,&line->text[startOffset]);
                        ImGui::SameLine(0,0);
                    }

                    // draw the folded tag
                    SyntaxHighlightingType sht = FoldingTypeToSyntaxHighlightingType[line->foldingStartTag->kind];
                    if ((style.color_syntax_highlighting[sht]&IM_COL32_A_MASK)==0)    {
                        if (line->foldingStartTag->kind==FOLDING_TYPE_COMMENT) sht = SH_COMMENT;
                        else if (line->foldingStartTag->kind==FOLDING_TYPE_PARENTHESIS) {
                            if (line->foldingStartTag->start[0]=='{') sht = SH_BRACKETS_CURLY;
                            else if (line->foldingStartTag->start[0]=='[') sht = SH_BRACKETS_SQUARE;
                            else if (line->foldingStartTag->start[0]=='(') sht = SH_BRACKETS_ROUND;
                        }
                    }
                    ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]));
                    ImGui::PushStyleColor(ImGuiCol_Text,ImColor(style.color_syntax_highlighting[sht]));
                    if (line->foldingStartTag->title.size()>0) {
                        // See if we must draw the bg:
                        const ImU32 bgColor = line->foldingStartTag->kind == FOLDING_TYPE_COMMENT ? style.color_folded_comment_background : line->foldingStartTag->kind == FOLDING_TYPE_PARENTHESIS ? style.color_folded_parenthesis_background : 0;
                        if ((bgColor & IM_COL32_A_MASK) !=0)	{
                            const ImVec2 regionNameSize = ImGui::CalcTextSize(line->foldingStartTag->title.c_str());    // Well, it'a a monospace font... we can optimize it
                            ImVec2 startPos = ImGui::GetCursorPos();
                            startPos.x+= windowPos.x - ImGui::GetScrollX() - lineHeight*0.09f;
                            startPos.y+= windowPos.y - ImGui::GetScrollY();
                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,startPos,ImVec2(startPos.x+regionNameSize.x+lineHeight*0.25f,startPos.y+regionNameSize.y),bgColor,0,0.f,0x0F,0);
                        }
                        // Normal folding
                        ImGui::Text("%s",line->foldingStartTag->title.c_str());
                        ImGui::SameLine(0,0);
                        mustSkipNextVisibleLine = true;++lineEnd;
                    }
                    else {
                        // Custom region
                        const char* regionName = &line->text[line->foldingStartOffset+line->foldingStartTag->start.size()];
                        if (regionName && regionName[0]!='\0')  {
                            // Draw frame around text
                            const ImVec2 regionNameSize = ImGui::CalcTextSize(regionName);    // Well, it'a a monospace font... we can optimize it
                            ImVec2 startPos = ImGui::GetCursorPos();
                            startPos.x+= windowPos.x - ImGui::GetScrollX() - lineHeight*0.18f;
                            startPos.y+= windowPos.y - ImGui::GetScrollY();
                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,startPos,ImVec2(startPos.x+regionNameSize.x+lineHeight*0.5f,startPos.y+regionNameSize.y),ImColor(style.color_folded_region_background),ImColor(style.color_syntax_highlighting[sht]),0.f,0x0F,folded_region_contour_thickness);

                            // Draw text
                            ImGui::Text("%s",regionName);
                        }
                    }
                    i = line->foldingEndLine->lineNumber-1;lineEnd+=line->foldingEndLine->lineNumber-line->lineNumber-1;
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                }
                else if (line->foldingStartTag->kind==FOLDING_TYPE_COMMENT) {
                    // Open multiline comment starting line

                    // draw sh text before the folding point (that is not folded now)
                    if (line->foldingStartOffset>0) {
                        this->TextLineWithSH("%.*s",line->foldingStartOffset-startOffset,&line->text[startOffset]);
                        ImGui::SameLine(0,0);
                    }

                    // draw the rest of the line directly as SH_COMMENT (using ImGui::Text(...), without parsing it with this->TextLineWithSH(...))
                    const SyntaxHighlightingType sht = SH_COMMENT;
                    ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]));
                    ImGui::PushStyleColor(ImGuiCol_Text,ImColor(style.color_syntax_highlighting[sht]));
                    ImGui::Text("%s",&line->text[line->foldingStartOffset]);
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                }
                else this->TextLineWithSH("%s",&line->text[startOffset]);
            }
            else if (line->isFoldingEnd())  {
                if (line->foldingStartLine->isFolded()) {
                    // End line of a folded region. Here we must just display what's left after the foldngEndOffset.
                    this->TextLineWithSH("%s",&line->text[line->foldingEndOffset+line->foldingStartLine->foldingStartTag->end.size()]);
                    //fprintf(stderr,"Line[%d] is a folded folding end\n",line->lineNumber);
                }
                else if (line->foldingStartLine->foldingStartTag->kind==FOLDING_TYPE_COMMENT) {
                    // Open multiline comment ending line

                    // draw the start of the line directly as SH_COMMENT (using ImGui::Text(...), without parsing it with this->TextLineWithSH(...))
                    const SyntaxHighlightingType sht = SH_COMMENT;
                    ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]));
                    ImGui::PushStyleColor(ImGuiCol_Text,ImColor(style.color_syntax_highlighting[sht]));
                    ImGui::Text("%.*s",line->foldingEndOffset+line->foldingStartLine->foldingStartTag->end.size(),line->text.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopFont();

                    // draw the end of the line parsed
                    ImGui::SameLine(0,0);
                    this->TextLineWithSH("%s",&line->text[line->foldingEndOffset+line->foldingStartLine->foldingStartTag->end.size()]);
                }
                else this->TextLineWithSH("%s",line->text.c_str());
            }
            else if (line->foldingStartLine && line->foldingStartLine->foldingStartTag->kind==FOLDING_TYPE_COMMENT) {
                // Internal lines of an unfolded comment region
                const SyntaxHighlightingType sht = SH_COMMENT;
                ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]));
                ImGui::PushStyleColor(ImGuiCol_Text,ImColor(style.color_syntax_highlighting[sht]));
                ImGui::Text("%s",line->text.c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            else this->TextLineWithSH("%s",line->text.c_str());

            const bool nextLineMergeble = (i+1<lines.size() && lines[i+1]->canFoldingBeMergedWithLineAbove() && lines[i+1]->isFolded());
            if (nextLineMergeble) {
                ImGui::SameLine(0,0);
                mustSkipNextVisibleLine=true;++lineEnd;
                //fprintf(stderr,"Line[%d] can be merged to the next\n",lines[i]->lineNumber);
            }
        }
        gCurlineStartedWithDiesis = gIsCurlineHovered = false;gCurline = NULL;    // Reset temp variables
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndGroup();
        if (!leftPaneHasMouseCursor)    {
            if (!gIsCursorChanged) {
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
                else ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            }
        }        
    }

    //if (visibleLines.size()>0) fprintf(stderr,"visibleLines.size()=%d firstVisibleLine=%d lastVisibleLine=%d\n",visibleLines.size(),visibleLines[0]->lineNumber+1,visibleLines[visibleLines.size()-1]->lineNumber+1);

    // Draw icon margin
    if (showIconMargin && ImGui::GetScrollX()<startCursorPosLineNumbers.x) {
        ImGui::SetCursorPos(startCursorPosIconMargin);
        // Draw background --------------------------------------
        if ((style.color_icon_margin_background&IM_COL32_A_MASK)!=0)    {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            //const float deltaX = windowScale * ImGui::GetStyle().WindowPadding.x;   // I'm not sure it's ImGui::GetStyle().WindowPadding.x *2.f...
            const ImVec2 ssp(startCursorPosIconMargin.x+windowPos.x-ImGui::GetScrollX()-lineHeight*0.25f,startCursorPosIconMargin.y+windowPos.y-ImGui::GetScrollY());
            const ImVec2 sep(ssp.x+lineHeight*1.25f,ssp.y+ lineHeight*visibleLines.size());
            drawList->AddRectFilled(ssp,sep,style.color_icon_margin_background);
        }
        //-------------------------------------------------------
        ImVec2 startPos,endPos,screenStartPos,screenEndPos;
        const float icon_margin_contour_thickness = style.icon_margin_contour_thickness * windowScale;
        ImGui::BeginGroup();
        for (int i = 0,isz=visibleLines.size();i<isz;i++) {
            Line* line = visibleLines[i];

            ImGui::PushID(line);
            startPos = ImGui::GetCursorPos();
            screenStartPos.x = startPos.x + windowPos.x - ImGui::GetScrollX() + lineHeight*0.75f;//windowScale * ImGui::GetStyle().WindowPadding.x;
            screenStartPos.y = startPos.y + windowPos.y - ImGui::GetScrollY();
            if (ImGui::InvisibleButton("##DummyButton",btnSize))    {
                if (io.KeyCtrl) {
                    line->attributes^=Line::AT_BOOKMARK;
                    fprintf(stderr,"AT_BOOKMARK(%d) = %s\n",i,(line->attributes&Line::AT_BOOKMARK)?"true":"false");
                }
                else {
                    line->attributes^=Line::AT_BREAKPOINT;
                    fprintf(stderr,"AT_BREAKPOINT(%d) = %s\n",i,(line->attributes&Line::AT_BREAKPOINT)?"true":"false");
                }
            }
            endPos = ImGui::GetCursorPos();
            screenEndPos.x = endPos.x + windowPos.x - ImGui::GetScrollX();
            screenEndPos.y = endPos.y + windowPos.y - ImGui::GetScrollY();
            if (line->attributes&Line::AT_ERROR) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,screenStartPos,screenEndPos,style.color_icon_margin_error,style.color_icon_margin_contour,0.f,0x0F,icon_margin_contour_thickness);
            }
            else if (line->attributes&Line::AT_WARNING) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,screenStartPos,screenEndPos,style.color_icon_margin_warning,style.color_icon_margin_contour,0.f,0x0F,icon_margin_contour_thickness);
            }
            if (line->attributes&Line::AT_BREAKPOINT) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 center((screenStartPos.x+screenEndPos.x)*0.5f,(screenStartPos.y+screenEndPos.y)*0.5f);
                const float radius = (screenEndPos.y-screenStartPos.y)*0.4f;
                //drawList->AddCircleFilled(center,radius,style.color_margin_breakpoint);
                //drawList->AddCircle(center,radius,style.color_margin_contour);
                ImGui::CodeEditorDrawListHelper::ImDrawListAddCircle(drawList,center,radius,style.color_icon_margin_breakpoint,style.color_icon_margin_contour,12,icon_margin_contour_thickness);

            }
            if (line->attributes&Line::AT_BOOKMARK) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 center((screenStartPos.x+screenEndPos.x)*0.5f,(screenStartPos.y+screenEndPos.y)*0.5f);
                const float radius = (screenEndPos.y-screenStartPos.y)*0.25f;
                const ImVec2 a(center.x-radius,center.y-radius);
                const ImVec2 b(center.x+radius,center.y+radius);
                //ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,a,b,style.color_margin_bookmark,style.color_margin_contour);
                ImGui::CodeEditorDrawListHelper::ImDrawListAddRect(drawList,a,b,style.color_icon_margin_bookmark,style.color_icon_margin_contour,0.f,0x0F,icon_margin_contour_thickness);
            }
            ImGui::SetCursorPos(endPos);

            if (ImGui::IsItemHovered())	{
                // to remove (dbg)
		ImGui::SetTooltip("Line:%d offset=%u offsetInUTF8chars=%u\nsize()=%d numUTF8chars=%d isFoldingStart=%s\nisFoldingEnd=%s isFolded=%s isHidden=%s\nfoldingStartLine=%d foldingEndLine=%d",line->lineNumber+1,line->offset,line->offsetInUTF8chars,
                                  line->text.size(),line->numUTF8chars,
                                  line->isFoldable()?"true":"false",
                                  (line->attributes&Line::AT_FOLDING_END)?"true":"false",
                                  line->isFolded()?"true":"false",
                                  line->isHidden()?"true":"false",
                                  line->foldingStartLine ? line->foldingStartLine->lineNumber+1 : -1,
                                  line->foldingEndLine ? line->foldingEndLine->lineNumber+1 : -1
                                                   );
            }

            ImGui::PopID();
        }
        ImGui::EndGroup();
        ImGui::SameLine(0,0);
    }
    //else fprintf(stderr,"Hidden: %1.2f\n",ImGui::GetScrollX());

     // Draw line numbers
    if (showLineNumbers && ImGui::GetScrollX()<startCursorPosFoldingMarks.x) {
        ImGui::SetCursorPos(startCursorPosLineNumbers);
        ImGui::BeginGroup();
        // Draw background --------------------------------------
        if ((style.color_line_numbers_background&IM_COL32_A_MASK)!=0)    {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 ssp(startCursorPosLineNumbers.x+windowPos.x-ImGui::GetScrollX(),startCursorPosLineNumbers.y+windowPos.y-ImGui::GetScrollY());
            const ImVec2 sep(ssp.x+lineNumberSize,ssp.y+ lineHeight*visibleLines.size());
            drawList->AddRectFilled(ssp,sep,style.color_line_numbers_background);
        }
        //-------------------------------------------------------
        ImGui::PushStyleColor(ImGuiCol_Text,style.color_line_numbers);
        ImGui::PushFont(const_cast<ImFont*>(ImFonts[style.font_line_numbers]));
        // I've struggled so much to display the line numbers half of their size, with no avail...
        for (int i = 0,isz=visibleLines.size();i<isz;i++) {
            Line* line = visibleLines[i];
            ImGui::Text("%*d",precision,(line->lineNumber+1));
        }
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ImGui::SameLine(0,0);
    }
    //else fprintf(stderr,"Hidden: %1.2f\n",ImGui::GetScrollX());

    // Draw folding spaces:
    if (enableTextFolding &&  ImGui::GetScrollX()<startCursorPosTextEditor.x) {
        ImGui::SetCursorPos(startCursorPosFoldingMarks);
        // Draw background --------------------------------------
        if ((style.color_folding_margin_background&IM_COL32_A_MASK)!=0)    {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 ssp(startCursorPosFoldingMarks.x+windowPos.x-ImGui::GetScrollX(),startCursorPosFoldingMarks.y+windowPos.y-ImGui::GetScrollY());
            const ImVec2 sep(ssp.x+sizeFoldingMarks*0.8f,ssp.y+ lineHeight*visibleLines.size());
            drawList->AddRectFilled(ssp,sep,style.color_folding_margin_background);
        }
        //-------------------------------------------------------
        ImGui::BeginGroup();
        ImGui::PushFont(const_cast<ImFont*>(ImFonts[FONT_STYLE_NORMAL]));
        ImGui::PushStyleColor(ImGuiCol_Header,transparent);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,transparent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,transparent);
        Line *line=NULL,*nextLine=NULL,*foldableLine=NULL;
        for (int i = 0,isz=visibleLines.size();i<isz;i++) {
            line = visibleLines[i];
            //fprintf(stderr,"Line[%d] - %d \n",line->lineNumber,i);
            if (line == nextLine) {nextLine=NULL;ImGui::Text("%s"," ");continue;}
            foldableLine = line->isFoldable() ? line : NULL;
            if (!foldableLine)  {
                nextLine = line->lineNumber+1<lines.size()?lines[line->lineNumber+1]:NULL;
                nextLine = (nextLine && nextLine->canFoldingBeMergedWithLineAbove()) ? nextLine : NULL;
                foldableLine = nextLine;
                //if (foldableLine) fprintf(stderr,"line: %d\n",foldableLine->lineNumber);
            }
            else nextLine = NULL;

            if (foldableLine)
            {
                //fprintf(stderr,"enableTextFolding: Line[%d] is foldable\n",line->lineNumber);
                SyntaxHighlightingType sht = FoldingTypeToSyntaxHighlightingType[foldableLine->foldingStartTag->kind];
                if ((style.color_syntax_highlighting[sht]&IM_COL32_A_MASK)==0)    {
                    if (line->foldingStartTag->kind==FOLDING_TYPE_COMMENT) sht = SH_COMMENT;
                    else if (line->foldingStartTag->kind==FOLDING_TYPE_PARENTHESIS) {
                        if (line->foldingStartTag->start[0]=='{') sht = SH_BRACKETS_CURLY;
                        else if (line->foldingStartTag->start[0]=='[') sht = SH_BRACKETS_SQUARE;
                        else if (line->foldingStartTag->start[0]=='(') sht = SH_BRACKETS_ROUND;
                    }
                }
                const bool wasFolded = foldableLine->isFolded();
                ImGui::PushStyleColor(ImGuiCol_Text,ImColor(style.color_syntax_highlighting[sht]));
                ImGui::SetNextTreeNodeOpen(!wasFolded,ImGuiSetCond_Always);
                if (!ImGui::TreeNode(line,"%s",""))  {
                    if (!wasFolded)   {
                        // process next lines to make them visible someway
                        foldableLine->setFoldedState(true);
                        Line::AdjustHiddenFlagsForFoldingStart(lines,*foldableLine);
                        /*if (nextLineIsFoldableAndMergeble && foldableLine!=nextLine && !nextLine->isFolded()) {
                            nextLine->setFoldedState(true);
                            Line::AdjustHiddenFlagsForFoldingStart(lines,*nextLine);
                        }*/
                    }
                }
                else {
                    ImGui::TreePop();
                    if (wasFolded)   {
                        // process next lines to make them invisible someway
                        foldableLine->setFoldedState(false);
                        Line::AdjustHiddenFlagsForFoldingStart(lines,*foldableLine);
                        /*if (nextLineIsFoldableAndMergeble && foldableLine!=nextLine && nextLine->isFolded()) {
                            nextLine->setFoldedState(false);
                            Line::AdjustHiddenFlagsForFoldingStart(lines,*nextLine);
                        }*/
                    }
                }
                ImGui::PopStyleColor();
            }
            else {
                ImGui::Text("%s"," ");
                //fprintf(stderr,"enableTextFolding: Line[%d] is not foldable\n",line->lineNumber);
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
        ImGui::EndGroup();
        ImGui::SameLine(0,0);
    }
    //else fprintf(stderr,"Hidden: %1.2f\n",ImGui::GetScrollX());

    ImGui::PopFont();
    ImGui::PopStyleVar();
    scrollToLine=-1;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((lines.size() - lineEnd) * lineHeight));

    ImGui::EndChild();      // "CodeEditorChild"
    ImGui::PopStyleColor(); // style.color_background

}


void CodeEditor::TextLineUnformattedWithSH(const char* text, const char* text_end)  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    IM_ASSERT(text != NULL);
    const char* text_begin = text;
    if (text_end == NULL) text_end = text + strlen(text); // FIXME-OPT

    {

        // Account of baseline offset
        ImVec2 text_pos = window->DC.CursorPos;
        text_pos.y += window->DC.CurrentLineTextBaseOffset;

        // I would like to remove the call to CalcText(...) here (in "text_size"), and I could simply retrieve
        // bb.Min after the call to RenderTextLineWrappedWithSH(...) to calculate it for free...
        // But the truth is that RenderTextLineWrappedWithSH(...) is using "gIsCurlineHovered"
        // to detect if it can make more expensive processing to detect when mouse is hovering over tokens.

        // Maybe here we can set "gIsCurlineHovered" only if mouse is in the y range and > x0.
        // Is it possible to do it ? How ?

        // Idea: Do it only if "ImGui::GetIO().KeyCtrl" is down.

        if (ImGui::GetIO().KeyCtrl) {
            // TO FIX: Folded code when KeyCtrl is down appears shifted one tab to the right. Why ?

            const ImVec2 text_size(ImGui::MyCalcTextWidth(text_begin, text_end),ImGui::GetTextLineHeight());
            ImRect bb(text_pos, text_pos + text_size);
            ImGui::ItemSize(text_size);
            if (!ImGui::ItemAdd(bb, NULL))  return;
            gIsCurlineHovered = ImGui::IsItemHovered();

            // Render (we don't hide text after ## in this end-user function)
            RenderTextLineWrappedWithSH(bb.Min, text_begin, text_end);
        }
        else {
            const ImVec2 old_text_pos = text_pos;

            gIsCurlineHovered = false;
            RenderTextLineWrappedWithSH(text_pos, text_begin, text_end);

            const ImVec2 text_size(text_pos.x-old_text_pos.x,ImGui::GetTextLineHeight());
            ImRect bb(old_text_pos, old_text_pos + text_size);
            ImGui::ItemSize(text_size);
            if (!ImGui::ItemAdd(bb, NULL))  return;
            gIsCurlineHovered = ImGui::IsItemHovered();
        }
    }
}
template <int NUM_TOKENS> inline static const char* FindNextToken(const char* text,const char* text_end,const char* token_start[NUM_TOKENS],const char* token_end[NUM_TOKENS],int* pTokenIndexOut=NULL,const char* optionalStringDelimiters=NULL,const char stringEscapeChar='\\',bool skipEscapeChar=false,bool findNewLineCharToo=false) {
    if (pTokenIndexOut) *pTokenIndexOut=-1;
    const char *pt,*t,*tks,*tke;
    int optionalStringDelimitersSize = optionalStringDelimiters ? strlen(optionalStringDelimiters) : -1;
    for (const char* p = text;p!=text_end;p++)  {
	if (token_start)    {
	    for (int j=0;j<NUM_TOKENS;j++)  {
		tks = token_start[j];
		tke = token_end[j];
		t = tks;
		pt = p;
		while (*t++==*pt++) {
		    if (t==tke) {
			if (pTokenIndexOut) *pTokenIndexOut=j;
			return p;
		    }
		}
		//fprintf(stderr,"%d) \"%s\"\n",j,tks);
	    }
	}
	if (optionalStringDelimiters)	{
	    for (int j=0;j<optionalStringDelimitersSize;j++)	{
		if (*p == optionalStringDelimiters[j])	{
		    if (skipEscapeChar || p == text || *(p-1)!=stringEscapeChar) {
			if (pTokenIndexOut) *pTokenIndexOut=NUM_TOKENS + j;
			return p;
		    }
		}
	    }
	}
    if (findNewLineCharToo) {
        if (*p=='\n') {
            if (skipEscapeChar || p == text || *(p-1)!=stringEscapeChar) {
                if (pTokenIndexOut) *pTokenIndexOut=NUM_TOKENS + optionalStringDelimitersSize;
            return p;
            }
        }
    }
    }
    return NULL;
}
void CodeEditor::RenderTextLineWrappedWithSH(ImVec2& pos, const char* text, const char* text_end, bool skipLineCommentAndStringProcessing)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    int text_len = (int)(text_end - text);
    if (!text_end)  text_end = text + text_len; // FIXME-OPT
    if (text==text_end) return;

    const FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(this->lang);
    if (!fsv || lang==LANG_NONE)	{
        const int text_len = (int)(text_end - text);
        if (text_len > 0)   {
            ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_end);
            if (g.LogEnabled) LogRenderedText(pos, text, text_end);
        }
        return;
    }

    if (!skipLineCommentAndStringProcessing) {
        const char* startComments[3] = {fsv->singleLineComment,fsv->multiLineCommentStart,fsv->multiLineCommentEnd};
        const char* endComments[3] = {fsv->singleLineComment+strlen(fsv->singleLineComment),fsv->multiLineCommentStart+strlen(fsv->multiLineCommentStart),fsv->multiLineCommentEnd+strlen(fsv->multiLineCommentEnd)};
        //for (int j=0;j<3;j++) fprintf(stderr,"%d) \"%s\"\n",j,startComments[j]);
        int tki=-1;
        const char* tk = FindNextToken<2>(text,text_end,startComments,endComments,&tki,fsv->stringDelimiterChars,fsv->stringEscapeChar,false);
        if (tk) {
            if (tki==0) {   // Found "//"
                if (tk!=text) RenderTextLineWrappedWithSH(pos,text,tk,true);    // Draw until "//"
                text = tk;
                const int text_len = (int)(text_end - text);
                if (text_len > 0)   {
                    ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_COMMENT]]), g.FontSize, pos, style.color_syntax_highlighting[SH_COMMENT], text, text_end);
                    if (g.LogEnabled) LogRenderedText(pos, text, text_end);
                }
                return;
            }
            else if (tki==1) {	// Found "/*"
                if (tk!=text) RenderTextLineWrappedWithSH(pos,text,tk,true);    // Draw until "/*"
                const int lenOpenCmn = strlen(startComments[1]);
                const char* endCmt = text_end;
                const char* tk2 = FindNextToken<1>(tk+lenOpenCmn,text_end,&startComments[2],&endComments[2]);	// Look for "*/"
                if (tk2) endCmt = tk2+strlen(startComments[2]);
                text = tk;
                const int text_len = (int)(endCmt - text);
                if (text_len > 0)   {
                    ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_COMMENT]]), g.FontSize, pos, style.color_syntax_highlighting[SH_COMMENT], text, endCmt);
                    if (g.LogEnabled) LogRenderedText(pos, text, endCmt);
                }
                if (tk2 && endCmt<text_end) RenderTextLineWrappedWithSH(pos,endCmt,text_end);    // Draw after "*/"
                return;
            }
            else if (tki>1) {	// Found " (or ')
                tki = tki-2;	// Found fsv->stringDelimiterChars[tki]
                if (tk!=text) RenderTextLineWrappedWithSH(pos,text,tk,true);    // Draw until "
                //const bool mustSkipNextEscapeChar = (lang==LANG_CS && tk>text && *(tk-1)=='@');   // Nope, I don't think this matters...
                if (tk+1<text_end)  {
                    // Here I have to match exactly fsv->stringDelimiterChars[tki], not another! (we don't use FindNextToken<>)
                    const char *tk2 = NULL,*tk2Start = tk+1;
                    while ((tk2 = strchr(tk2Start,fsv->stringDelimiterChars[tki])))   {
                        if (tk2>=text_end) {tk2 = NULL;break;}
                        tk2Start = tk2+1;
                        if (tk2>text && *(tk2-1)==fsv->stringEscapeChar) continue;
                        break;
                    }
                    const char* endStringSH = tk2==NULL ? text_end : (tk2+1);
                    // Draw String:
                    const ImVec2 oldPos = pos;
                    ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_STRING]]), g.FontSize, pos, style.color_syntax_highlighting[SH_STRING], tk, endStringSH);
                    if (g.LogEnabled) LogRenderedText(pos, tk, endStringSH);
                    const float token_width = pos.x - oldPos.x;  //MyCalcTextWidth(tk,endStringSH);
                    // TEST: Mouse interaction on token (gIsCurlineHovered == true when CTRL is pressed)-------------------
                    if (gIsCurlineHovered) {
                        // See if we can strip 2 chars
                        const ImVec2 token_size(token_width,g.FontSize);
                        const ImVec2& token_pos = oldPos;
                        ImRect bb(token_pos, token_pos + token_size);
                        if (ImGui::ItemAdd(bb, NULL) && ImGui::IsItemHovered()) {
                            window->DrawList->AddLine(ImVec2(bb.Min.x,bb.Max.y), bb.Max, style.color_syntax_highlighting[SH_STRING], 2.f);
                            //if (ImGui::GetIO().MouseClicked[0])  {fprintf(stderr,"Mouse clicked on token: \"%s\"(%d->\"%s\") curlineStartedWithDiesis=%s line=\"%s\"\n",s,len_tok,tok,curlineStartedWithDiesis?"true":"false",curline->text.c_str());}
                            ImGui::SetTooltip("Token (quotes are included): %.*s\nSH = %s\nLine (%d):\"%s\"\nLine starts with '#': %s",(int)(endStringSH-tk),tk,SyntaxHighlightingTypeStrings[SH_STRING],gCurline->lineNumber+1,gCurline->text.c_str(),gCurlineStartedWithDiesis?"true":"false");
                            gIsCursorChanged = true;ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                        }
                    }
                    // ----------------------------------------------------------------------------------------------------
                    //pos.x+=token_width;
                    if (tk2==NULL)	return;	// No other match found
                    text = endStringSH;
                    if (text!=text_end) {
                        //fprintf(stderr,"\"%.*s\"\n",(int)(text_end-text),text);
                        RenderTextLineWrappedWithSH(pos,text,text_end);
                    }
                    return;
                }
            }
            return;
        }
    }



    const char sp = ' ';const char tab='\t';
    const char *s = text;
    bool firstTokenHasPreprocessorStyle = false;	// hack to handle lines such as "#  include <...>" in cpp
    bool lineStartsWithDiesis = false;
    // Process the start of the line

    // skip tabs and spaces
    while (*s==sp || *s==tab)	{
    if (s+1==text_end)  {
        ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_end);
        if (g.LogEnabled) LogRenderedText(pos, text, text_end);
        return;
    }
    ++s;
    }
    if (s>text)	{
        // Draw Tabs and spaces
        ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, s);
        if (g.LogEnabled) LogRenderedText(pos, text, s);
        text=s;
    }

    // process special chars at the start of the line (e.g. "//" or '#' in cpp)
    if (s<text_end) {
        // Handle '#' in Cpp
        if (*s=='#')	{
            gCurlineStartedWithDiesis = lineStartsWithDiesis = true;
            if ((lang==LANG_CPP || lang==LANG_GLSL) && s+1<text_end && (*(s+1)==sp || *(s+1)==tab)) firstTokenHasPreprocessorStyle = true;
        }
    }



    // Tokenise
    IM_ASSERT(fsv->punctuationStringsMerged!=NULL && strlen(fsv->punctuationStringsMerged)>0);
    static char Text[2048];
    IM_ASSERT(text_end-text<2048);
    strncpy(Text,text,text_end-text);	// strtok is destructive
    Text[text_end-text]='\0';
    s=text;
    char* tok = strtok(Text,fsv->punctuationStringsMerged),*oldTok=Text;
    int offset = 0,len_tok=0,num_tokens=0;
    short int tokenIsNumber = 0;
    while (tok) {
        offset = tok-oldTok;
        if (offset>0) {
            // Print Punctuation
            /*window->DrawList->AddText(g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Button), s, s+offset, wrap_width);
    if (g.LogEnabled) LogRenderedText(pos, s, s+offset);
    //pos.x+=charWidth*(offset);
    pos.x+=CalcTextWidth(s, s+offset).x;*/
            for (int j=0;j<offset;j++)  {
                const char* ch = s+j;
                int sht = -1;
                if (tokenIsNumber && *ch=='.') {sht = SH_NUMBER;++tokenIsNumber;}
                if (sht==-1 && !shTypePunctuationMap.get(*ch,sht)) sht = -1;
                if (sht>=0 && sht<SH_COUNT) ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], s+j, s+j+1);
                else ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), s+j, s+j+1);
                if (g.LogEnabled) LogRenderedText(pos, s+j, s+j+1);
            }
        }
        s+=offset;
        // Print Token (Syntax highlighting through HashMap here)
        len_tok = strlen(tok);if (--tokenIsNumber<0) tokenIsNumber=0;
        if (len_tok>0)  {
            int sht = -1;	// Mandatory assignment
            // Handle special starting tokens
            if ((firstTokenHasPreprocessorStyle && num_tokens<2) || ((lang==LANG_CPP || lang==LANG_GLSL) && lineStartsWithDiesis && strcmp(tok,"defined")==0))	sht = SH_KEYWORD_PREPROCESSOR;
            if (sht==-1)	{
                // Handle numbers
                if (tokenIsNumber==0)   {
                    const char *tmp=tok,*tmpEnd=(tok+len_tok);
                    for (tmp=tok;tmp!=tmpEnd;++tmp)	{
                        if ((*tmp)<'0' || (*tmp)>'9')	{
                            if ((tmp+2 == tmpEnd) && (*tmp)=='.')  tokenIsNumber = 1;	// TODO: What if '.' is a token splitter char ?
                            break;
                        }
                    }
                    if (tmp==tmpEnd) tokenIsNumber = 1;
                }
                if (tokenIsNumber) sht = SH_NUMBER;
            }
            const ImVec2 oldPos = pos;
            if (sht>=0 || shTypeKeywordMap.get(tok,sht)) {
                //fprintf(stderr,"Getting shTypeMap: \"%s\",%d\n",tok,sht);
                ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], tok, tok+len_tok);
            }
            else {
                //fprintf(stderr,"Not Getting shTypeMap: \"%s\",%d\n",tok,sht);
                ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), tok, tok+len_tok);
            }
            if (g.LogEnabled) LogRenderedText(pos, tok, tok+len_tok);
            const float token_width = pos.x - oldPos.x;//MyCalcTextWidth(tok,tok+len_tok);   // We'll use this later
            // TEST: Mouse interaction on token (gIsCurlineHovered == true when CTRL is pressed)-------------------
            if (gIsCurlineHovered) {
                const ImVec2 token_size(token_width,g.FontSize);// = ImGui::CalcTextSize(tok, tok+len_tok, false, 0.f);
                const ImVec2& token_pos = oldPos;
                ImRect bb(token_pos, token_pos + token_size);
                if (ImGui::ItemAdd(bb, NULL) && ImGui::IsItemHovered()) {
                    window->DrawList->AddLine(ImVec2(bb.Min.x,bb.Max.y), bb.Max, sht>=0 ? style.color_syntax_highlighting[sht] : ImGui::GetColorU32(ImGuiCol_Text), 2.f);
                    if (ImGui::GetIO().MouseClicked[0])  {fprintf(stderr,"Mouse clicked on token: \"%s\"(%d->\"%s\") curlineStartedWithDiesis=%s line=\"%s\"\n",s,len_tok,tok,gCurlineStartedWithDiesis?"true":"false",gCurline->text.c_str());}
                    ImGui::SetTooltip("Token: \"%s\" len=%d\nToken unclamped: \"%s\"\nSH = %s\nLine (%d):\"%s\"\nLine starts with '#': %s",tok,len_tok,s,sht<0 ? "None" : SyntaxHighlightingTypeStrings[sht],gCurline->lineNumber+1,gCurline->text.c_str(),gCurlineStartedWithDiesis?"true":"false");
                    gIsCursorChanged = true;ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                }
            }
            // ----------------------------------------------------------------------------------------------------
        }
        //printf("Token: %s\n", tok);
        oldTok = tok+len_tok;s+=len_tok;
        tok = strtok(NULL,fsv->punctuationStringsMerged);

        ++num_tokens;
    }

    offset = text_end-s;
    if (offset>0) {
        // Print Punctuation
        //window->DrawList->AddText(g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Button), s, s+offset, wrap_width);
        //if (g.LogEnabled) LogRenderedText(pos, s, s+offset);
        for (int j=0;j<offset;j++)  {
            const char* ch = s+j;
            int sht = -1;
            if (tokenIsNumber && *ch=='.') {sht = SH_NUMBER;++tokenIsNumber;}
            if (sht==-1 && !shTypePunctuationMap.get(*ch,sht)) sht = -1;
            if (sht>=0 && sht<SH_COUNT) ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], s+j, s+j+1);
            else ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), s+j, s+j+1);
            if (g.LogEnabled) LogRenderedText(pos, s+j, s+j+1);
        }
    }

}

template <int NUM_TOKENS> inline static const char* FindPrevToken(const char* text,const char* text_end,const char* token_start[NUM_TOKENS],const char* token_end[NUM_TOKENS],int* pTokenIndexOut=NULL,const char* optionalStringDelimiters=NULL,const char stringEscapeChar='\\',bool skipEscapeChar=false,bool findNewLineCharToo=false) {
    if (pTokenIndexOut) *pTokenIndexOut=-1;
    const char *pt,*t,*tks,*tke;
    int optionalStringDelimitersSize = optionalStringDelimiters ? strlen(optionalStringDelimiters) : -1;
    for (const char* p = text_end;p!=text;--p)  {
        if (token_start)    {
            for (int j=0;j<NUM_TOKENS;j++)  {
                tks = token_start[j];
                tke = token_end[j];
                t = tke;
                pt = p;
                while (*(--t)==*(--pt)) {
                    if (t==tks) {
                        if (pTokenIndexOut) *pTokenIndexOut=j;
                        return p;
                    }
                }
                //fprintf(stderr,"%d) \"%s\"\n",j,tks);
            }
        }
        if (optionalStringDelimiters)	{
            for (int j=0;j<optionalStringDelimitersSize;j++)	{
                if (*(p-1) == optionalStringDelimiters[j])	{
                    if (skipEscapeChar || (p-1) == text || *(p-2)!=stringEscapeChar) {
                        if (pTokenIndexOut) *pTokenIndexOut=NUM_TOKENS + j;
                        return p;
                    }
                }
            }
        }
        if (findNewLineCharToo) {
            if (*(p-1)=='\n') {
                if (skipEscapeChar || (p-1) == text || *(p-2)!=stringEscapeChar) {
                    if (pTokenIndexOut) *pTokenIndexOut=NUM_TOKENS + optionalStringDelimitersSize;
                    return p;
                }
            }
        }
    }
    return NULL;
}



} // namespace ImGuiCe


namespace ImGuiCe {
using namespace ImGui;

struct BadCodeEditorData {
    typedef ImGuiCe::CodeEditor::MyKeywordMapType MyKeywordMapType;
    MyKeywordMapType shTypeKeywordMap;
    ImHashMapChar    shTypePunctuationMap;
    ImGuiCe::Language lang;
    const char* startComments[3];
    const char* endComments[3];
    char stringEscapeChar;
    const char* stringDelimiterChars;   // e.g. "\"'"
    char punctuationStringsMerged[1024];	// This must be sorted alphabetically (to be honest now that we use a map, sorting it's not needed anymore (and it was not used even before!))
    mutable bool inited;
    BadCodeEditorData() : lang(ImGuiCe::LANG_NONE),inited(false) {
        for (int i=0;i<3;i++) {startComments[i]=endComments[i]=NULL;}
        stringEscapeChar='\\';
        static const char* _stringDelimiterChars = "\"'";
        stringDelimiterChars = _stringDelimiterChars;
        punctuationStringsMerged[0]='\0';
    }
    void init(ImGuiCe::Language _lang) {
        if (inited) return;
        inited = true;
        // TODO: Fill shTypePunctuationMap and shTypeKeywordMap based on
        shTypeKeywordMap.clear();
        shTypePunctuationMap.clear();

        FoldingStringVector* fsv = GetGlobalFoldingStringVectorForLanguage(_lang);
        if (fsv)    {
            // replace our keywords hashmap
            for (int i=0,isz = SH_LOGICAL_OPERATORS;i<isz;i++)  {
                const ImVectorEx<const char*>& v = fsv->keywords[i];
                for (int j=0,jsz=v.size();j<jsz;j++) {
                    shTypeKeywordMap.put((MyKeywordMapType::KeyType)v[j],i);
                    //fprintf(stderr,"Putting in shTypeMap: \"%s\",%d\n",v[j],i);
                }
            }
            // replace our punctuation hashmap
            for (int i=0,isz = strlen(fsv->punctuationStringsMerged);i<isz;i++)  {
                const char c = fsv->punctuationStringsMerged[i];
                shTypePunctuationMap.put(c,fsv->punctuationStringsMergedSHMap[i]);
                //fprintf(stderr,"Putting in shTypePunctuationMap: '%c',%d\n",c,fsv->punctuationStringsMergedSHMap[i]);
            }

            startComments[0] = fsv->singleLineComment;
            startComments[1] = fsv->multiLineCommentStart;
            startComments[2] = fsv->multiLineCommentEnd;
            endComments[0] = fsv->singleLineComment+strlen(fsv->singleLineComment);
            endComments[1] = fsv->multiLineCommentStart+strlen(fsv->multiLineCommentStart);
            endComments[2] = fsv->multiLineCommentEnd+strlen(fsv->multiLineCommentEnd);

            stringEscapeChar = fsv->stringEscapeChar;
            stringDelimiterChars = fsv->stringDelimiterChars;
            strcpy(punctuationStringsMerged,fsv->punctuationStringsMerged);

        }
        lang = _lang;
    }

    // These are temporary variables, but I reuse this struct for now...
    mutable float textSizeX;
    mutable bool pendingOpenMultilineComment;   // TODO: put into a "state" struct
    void resetStateVariables() {
        textSizeX = 0;
        pendingOpenMultilineComment = false;
    }
};

static BadCodeEditorData BadCodeEditorLanguageData[ImGuiCe::LANG_COUNT];

static void MyRenderTextLineWrappedWithSH(const BadCodeEditorData& ceData,ImVec2& pos, const char* text, const char* text_end, bool skipLineCommentAndStringProcessing=false)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    ImFont** ImFonts = const_cast<ImFont**>(ImGuiCe::CodeEditor::ImFonts);
    if (!ImFonts[0]) {
        if (ImGui::GetIO().Fonts->Fonts.size()==0) return;
        for (int i=0;i<FONT_STYLE_COUNT;i++) ImFonts[i]=ImGui::GetIO().Fonts->Fonts[0];
    }
    ImGuiCe::CodeEditor::Style& style = ImGuiCe::CodeEditor::style;

    int text_len = (int)(text_end - text);
    if (!text_end)  text_end = text + text_len; // FIXME-OPT
    if (text==text_end) return;

    if (ceData.lang==LANG_NONE)	{
        const int text_len = (int)(text_end - text);
        if (text_len > 0)   {
            ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_end);
            if (g.LogEnabled) LogRenderedText(pos, text, text_end);
        }
        return;
    }

    if (!skipLineCommentAndStringProcessing) {
        const char* startComments[3] = {ceData.startComments[0],ceData.startComments[1],ceData.startComments[2]};
        const char* endComments[3] = {ceData.endComments[0],ceData.endComments[1],ceData.endComments[2]};
        //for (int j=0;j<3;j++) fprintf(stderr,"%d) \"%s\"\n",j,startComments[j]);
        int tki=-1;
        if (!ceData.pendingOpenMultilineComment) {
            const char* tk = FindNextToken<2>(text,text_end,startComments,endComments,&tki,ceData.stringDelimiterChars,ceData.stringEscapeChar,false);
            if (tk) {
                if (tki==0) {   // Found "//"
                    if (tk!=text) MyRenderTextLineWrappedWithSH(ceData,pos,text,tk,true);    // Draw until "//"
                    text = tk;
                    const int text_len = (int)(text_end - text);
                    if (text_len > 0)   {
                        ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_COMMENT]]), g.FontSize, pos, style.color_syntax_highlighting[SH_COMMENT], text, text_end);
                        if (g.LogEnabled) LogRenderedText(pos, text, text_end);
                    }
                    return;
                }
                else if (tki==1) {	// Found "/*"
                    ceData.pendingOpenMultilineComment = true;  // correct ?
                    if (tk!=text) MyRenderTextLineWrappedWithSH(ceData,pos,text,tk,true);    // Draw until "/*"
                    const int lenOpenCmn = strlen(startComments[1]);
                    const char* endCmt = text_end;
                    const char* tk2 = FindNextToken<1>(tk+lenOpenCmn,text_end,&startComments[2],&endComments[2]);	// Look for "*/"
                    if (tk2) {
                        endCmt = tk2+strlen(startComments[2]);
                        ceData.pendingOpenMultilineComment = false;
                    }
                    text = tk;
                    const int text_len = (int)(endCmt - text);
                    if (text_len > 0)   {
                        ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_COMMENT]]), g.FontSize, pos, style.color_syntax_highlighting[SH_COMMENT], text, endCmt);
                        if (g.LogEnabled) LogRenderedText(pos, text, endCmt);
                    }
                    if (tk2 && endCmt<text_end) MyRenderTextLineWrappedWithSH(ceData,pos,endCmt,text_end);    // Draw after "*/"
                    return;
                }
                else if (tki>1) {	// Found " (or ')
                    tki = tki-2;	// Found ceData.stringDelimiterChars[tki]
                    if (tk!=text) MyRenderTextLineWrappedWithSH(ceData,pos,text,tk,true);    // Draw until "
                    //const bool mustSkipNextEscapeChar = (lang==LANG_CS && tk>text && *(tk-1)=='@');   // Nope, I don't think this matters...
                    if (tk+1<text_end)  {
                        // Here I have to match exactly ceData.stringDelimiterChars[tki], not another! (we don't use FindNextToken<>)
                        const char *tk2 = NULL,*tk2Start = tk+1;
                        while ((tk2 = strchr(tk2Start,ceData.stringDelimiterChars[tki])))   {
                            if (tk2>=text_end) {tk2 = NULL;break;}
                            tk2Start = tk2+1;
                            if (tk2>text && *(tk2-1)==ceData.stringEscapeChar) continue;
                            break;
                        }
                        const char* endStringSH = tk2==NULL ? text_end : (tk2+1);
                        // Draw String:
                        const ImVec2 oldPos = pos;
                        ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_STRING]]), g.FontSize, pos, style.color_syntax_highlighting[SH_STRING], tk, endStringSH);
                        if (g.LogEnabled) LogRenderedText(pos, tk, endStringSH);
                        const float token_width = pos.x - oldPos.x;  //MyCalcTextWidth(tk,endStringSH);
                        // TEST: Mouse interaction on token (gIsCurlineHovered == true when CTRL is pressed)-------------------
                        if (gIsCurlineHovered) {
                            // See if we can strip 2 chars
                            const ImVec2 token_size(token_width,g.FontSize);
                            const ImVec2& token_pos = oldPos;
                            ImRect bb(token_pos, token_pos + token_size);
                            if (ImGui::ItemAdd(bb, NULL) && ImGui::IsItemHovered()) {
                                window->DrawList->AddLine(ImVec2(bb.Min.x,bb.Max.y), bb.Max, style.color_syntax_highlighting[SH_STRING], 2.f);
                                //if (ImGui::GetIO().MouseClicked[0])  {fprintf(stderr,"Mouse clicked on token: \"%s\"(%d->\"%s\") curlineStartedWithDiesis=%s line=\"%s\"\n",s,len_tok,tok,curlineStartedWithDiesis?"true":"false",curline->text.c_str());}
                                ImGui::SetTooltip("Token (quotes are included): %.*s\nSH = %s\nLine (%d):\"%s\"\nLine starts with '#': %s",(int)(endStringSH-tk),tk,SyntaxHighlightingTypeStrings[SH_STRING],gCurline->lineNumber+1,gCurline->text.c_str(),gCurlineStartedWithDiesis?"true":"false");
                                gIsCursorChanged = true;ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                            }
                        }
                        // ----------------------------------------------------------------------------------------------------
                        //pos.x+=token_width;
                        if (tk2==NULL)	return;	// No other match found
                        text = endStringSH;
                        if (text!=text_end) {
                            //fprintf(stderr,"\"%.*s\"\n",(int)(text_end-text),text);
                            MyRenderTextLineWrappedWithSH(ceData,pos,text,text_end);
                        }
                        return;
                    }
                }
                return;
            }
        }
        else {
            const char* endCmt = text_end;
            const char* tk2 = FindNextToken<1>(text,text_end,&startComments[2],&endComments[2]);	// Look for "*/"
            if (tk2) {
                endCmt = tk2+strlen(startComments[2]);
                ceData.pendingOpenMultilineComment = false;
            }
            const int text_len = (int)(endCmt - text);
            if (text_len > 0)   {
                ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[SH_COMMENT]]), g.FontSize, pos, style.color_syntax_highlighting[SH_COMMENT], text, endCmt);
                if (g.LogEnabled) LogRenderedText(pos, text, endCmt);
            }
            if (tk2 && endCmt<text_end) MyRenderTextLineWrappedWithSH(ceData,pos,endCmt,text_end);    // Draw after "*/"
            return;
        }
    }



    const char sp = ' ';const char tab='\t';
    const char *s = text;
    bool firstTokenHasPreprocessorStyle = false;	// hack to handle lines such as "#  include <...>" in cpp
    bool lineStartsWithDiesis = false;
    // Process the start of the line

    // skip tabs and spaces
    while (*s==sp || *s==tab)	{
    if (s+1==text_end)  {
        ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_end);
        if (g.LogEnabled) LogRenderedText(pos, text, text_end);
        return;
    }
    ++s;
    }
    if (s>text)	{
        // Draw Tabs and spaces
        ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, s);
        if (g.LogEnabled) LogRenderedText(pos, text, s);
        text=s;
    }

    // process special chars at the start of the line (e.g. "//" or '#' in cpp)
    if (s<text_end) {
        // Handle '#' in Cpp
        if (*s=='#')	{
            gCurlineStartedWithDiesis = lineStartsWithDiesis = true;
            if ((ceData.lang==LANG_CPP || ceData.lang==LANG_GLSL) && s+1<text_end && (*(s+1)==sp || *(s+1)==tab)) firstTokenHasPreprocessorStyle = true;
        }
    }



    // Tokenise
    const ImGuiCe::CodeEditor::MyKeywordMapType& shTypeKeywordMap = ceData.shTypeKeywordMap;
    const ImHashMapChar& shTypePunctuationMap = ceData.shTypePunctuationMap;
    IM_ASSERT(ceData.punctuationStringsMerged!=NULL && strlen(ceData.punctuationStringsMerged)>0);
    static char Text[2048];
    IM_ASSERT(text_end-text<2048);
    strncpy(Text,text,text_end-text);	// strtok is destructive
    Text[text_end-text]='\0';
    s=text;
    char* tok = strtok(Text,ceData.punctuationStringsMerged),*oldTok=Text;
    int offset = 0,len_tok=0,num_tokens=0;
    short int tokenIsNumber = 0;
    while (tok) {
        offset = tok-oldTok;
        if (offset>0) {
            // Print Punctuation
            /*window->DrawList->AddText(g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Button), s, s+offset, wrap_width);
    if (g.LogEnabled) LogRenderedText(pos, s, s+offset);
    //pos.x+=charWidth*(offset);
    pos.x+=CalcTextWidth(s, s+offset).x;*/
            for (int j=0;j<offset;j++)  {
                const char* ch = s+j;
                int sht = -1;
                if (tokenIsNumber && *ch=='.') {sht = SH_NUMBER;++tokenIsNumber;}
                if (sht==-1 && !shTypePunctuationMap.get(*ch,sht)) sht = -1;
                if (sht>=0 && sht<SH_COUNT) ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], s+j, s+j+1);
                else ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), s+j, s+j+1);
                if (g.LogEnabled) LogRenderedText(pos, s+j, s+j+1);
            }
        }
        s+=offset;
        // Print Token (Syntax highlighting through HashMap here)
        len_tok = strlen(tok);if (--tokenIsNumber<0) tokenIsNumber=0;
        if (len_tok>0)  {
            int sht = -1;	// Mandatory assignment
            // Handle special starting tokens
            if ((firstTokenHasPreprocessorStyle && num_tokens<2) || ((ceData.lang==LANG_CPP || ceData.lang==LANG_GLSL) && lineStartsWithDiesis && strcmp(tok,"defined")==0))	sht = SH_KEYWORD_PREPROCESSOR;
            if (sht==-1)	{
                // Handle numbers
                if (tokenIsNumber==0)   {
                    const char *tmp=tok,*tmpEnd=(tok+len_tok);
                    for (tmp=tok;tmp!=tmpEnd;++tmp)	{
                        if ((*tmp)<'0' || (*tmp)>'9')	{
                            if ((tmp+2 == tmpEnd) && (*tmp)=='.')  tokenIsNumber = 1;	// TODO: What if '.' is a token splitter char ?
                            break;
                        }
                    }
                    if (tmp==tmpEnd) tokenIsNumber = 1;
                }
                if (tokenIsNumber) sht = SH_NUMBER;
            }
            const ImVec2 oldPos = pos;
            if (sht>=0 || shTypeKeywordMap.get(tok,sht)) {
                //fprintf(stderr,"Getting shTypeMap: \"%s\",%d\n",tok,sht);
                ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], tok, tok+len_tok);
            }
            else {
                //fprintf(stderr,"Not Getting shTypeMap: \"%s\",%d\n",tok,sht);
                ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), tok, tok+len_tok);
            }
            if (g.LogEnabled) LogRenderedText(pos, tok, tok+len_tok);
            const float token_width = pos.x - oldPos.x;//MyCalcTextWidth(tok,tok+len_tok);   // We'll use this later
            // TEST: Mouse interaction on token (gIsCurlineHovered == true when CTRL is pressed)-------------------
            if (gIsCurlineHovered) {
                const ImVec2 token_size(token_width,g.FontSize);// = ImGui::CalcTextSize(tok, tok+len_tok, false, 0.f);
                const ImVec2& token_pos = oldPos;
                ImRect bb(token_pos, token_pos + token_size);
                if (ImGui::ItemAdd(bb, NULL) && ImGui::IsItemHovered()) {
                    window->DrawList->AddLine(ImVec2(bb.Min.x,bb.Max.y), bb.Max, sht>=0 ? style.color_syntax_highlighting[sht] : ImGui::GetColorU32(ImGuiCol_Text), 2.f);
                    if (ImGui::GetIO().MouseClicked[0])  {fprintf(stderr,"Mouse clicked on token: \"%s\"(%d->\"%s\") curlineStartedWithDiesis=%s line=\"%s\"\n",s,len_tok,tok,gCurlineStartedWithDiesis?"true":"false",gCurline->text.c_str());}
                    ImGui::SetTooltip("Token: \"%s\" len=%d\nToken unclamped: \"%s\"\nSH = %s\nLine (%d):\"%s\"\nLine starts with '#': %s",tok,len_tok,s,sht<0 ? "None" : SyntaxHighlightingTypeStrings[sht],gCurline->lineNumber+1,gCurline->text.c_str(),gCurlineStartedWithDiesis?"true":"false");
                    gIsCursorChanged = true;ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                }
            }
            // ----------------------------------------------------------------------------------------------------
        }
        //printf("Token: %s\n", tok);
        oldTok = tok+len_tok;s+=len_tok;
        tok = strtok(NULL,ceData.punctuationStringsMerged);

        ++num_tokens;
    }

    offset = text_end-s;
    if (offset>0) {
        // Print Punctuation
        //window->DrawList->AddText(g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Button), s, s+offset, wrap_width);
        //if (g.LogEnabled) LogRenderedText(pos, s, s+offset);
        for (int j=0;j<offset;j++)  {
            const char* ch = s+j;
            int sht = -1;
            if (tokenIsNumber && *ch=='.') {sht = SH_NUMBER;++tokenIsNumber;}
            if (sht==-1 && !shTypePunctuationMap.get(*ch,sht)) sht = -1;
            if (sht>=0 && sht<SH_COUNT) ImGui::ImDrawListAddTextLine(window->DrawList,const_cast<ImFont*>(ImFonts[style.font_syntax_highlighting[sht]]), g.FontSize, pos, style.color_syntax_highlighting[sht], s+j, s+j+1);
            else ImGui::ImDrawListAddTextLine(window->DrawList,g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), s+j, s+j+1);
            if (g.LogEnabled) LogRenderedText(pos, s+j, s+j+1);
        }
    }

}
static void MyTextLineUnformattedWithSH(const BadCodeEditorData& ceData,const char* text, const char* text_end)  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    IM_ASSERT(text != NULL);
    const char* text_begin = text;
    if (text_end == NULL) text_end = text + strlen(text); // FIXME-OPT

    {

        // Account of baseline offset
        ImVec2 text_pos = window->DC.CursorPos;
        text_pos.y += window->DC.CurrentLineTextBaseOffset;


        const ImVec2 old_text_pos = text_pos;

        gIsCurlineHovered = false;
        MyRenderTextLineWrappedWithSH(ceData,text_pos, text_begin, text_end);
        //IM_ASSERT(old_text_pos.y==text_pos.y);
        if (ceData.textSizeX < text_pos.x - old_text_pos.x) ceData.textSizeX = text_pos.x - old_text_pos.x;

        const float textLineHeight = //GImGui->FontSize;
        ImGui::GetTextLineHeight();

        /*const ImVec2 text_size(text_pos.x-old_text_pos.x,textLineHeight);
        ImRect bb(old_text_pos, old_text_pos + text_size);
        ImGui::ItemSize(text_size);
        if (!ImGui::ItemAdd(bb, NULL))  return;
        gIsCurlineHovered = ImGui::IsItemHovered();*/
        window->DC.CursorPos.y = old_text_pos.y+ textLineHeight;
    }
}
static void MyTextLineWithSHV(const BadCodeEditorData& ceData,const char* fmt, va_list args) {
    if (ImGui::GetCurrentWindow()->SkipItems)  return;

    ImGuiContext& g = *GImGui;
    const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    MyTextLineUnformattedWithSH(ceData,g.TempBuffer, text_end);
}
static void MyTextLineWithSH(const BadCodeEditorData& ceData,const char* fmt, ...)   {
    va_list args;
    va_start(args, fmt);
    MyTextLineWithSHV(ceData,fmt, args);
    va_end(args);
}


bool BadCodeEditor(const char* label, char* buf, size_t buf_size,ImGuiCe::Language lang, const ImVec2& size_arg, ImGuiInputTextFlags flags, ImGuiTextEditCallback callback, void* user_data)
{
//#define BAD_CODE_EDITOR_HAS_HORIZONTAL_SCROLLBAR  // doesn't work... maybe we can fix it someday... but default ImGui::InputTextMultiline() should be made with it...
#   ifndef BAD_CODE_EDITOR_HAS_HORIZONTAL_SCROLLBAR
    const ImGuiWindowFlags myChildWindowFlags = 0;
#   else //BAD_CODE_EDITOR_HAS_HORIZONTAL_SCROLLBAR
    const ImGuiWindowFlags myChildWindowFlags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
#   endif //BAD_CODE_EDITOR_HAS_HORIZONTAL_SCROLLBAR

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    flags|= ImGuiInputTextFlags_Multiline|ImGuiInputTextFlags_AllowTabInput;
    IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackHistory) && (flags & ImGuiInputTextFlags_Multiline))); // Can't use both together (they both use up/down keys)
    IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackCompletion) && (flags & ImGuiInputTextFlags_AllowTabInput))); // Can't use both together (they both use tab key)

    if (lang>=ImGuiCe::LANG_COUNT) lang = ImGuiCe::LANG_NONE;
    if (!ImGuiCe::CodeEditor::StaticInited) ImGuiCe::CodeEditor::StaticInit();
    BadCodeEditorData& langData = BadCodeEditorLanguageData[lang];
    if (!langData.inited) langData.init(lang);
    langData.resetStateVariables();

    ImGuiContext& g = *GImGui;
    const ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiCe::CodeEditor::Style& ceStyle = ImGuiCe::CodeEditor::style;
    ImFont** ImFonts = const_cast<ImFont**>(ImGuiCe::CodeEditor::ImFonts);
    if (!ImFonts[0]) {
        if (ImGui::GetIO().Fonts->Fonts.size()==0) return false;
        for (int i=0;i<FONT_STYLE_COUNT;i++) ImFonts[i]=ImGui::GetIO().Fonts->Fonts[0];
    }
    ImGui::PushID(label);
    const ImGuiID id = window->GetID("");
    const bool is_editable = (flags & ImGuiInputTextFlags_ReadOnly) == 0;

    bool showLineNumbers = true;
    float lineNumberSize = 0;
    int lineNumberPrecision =  0;

    const ImVec2 label_size(0,0);// = CalcTextSize(label, NULL, true);
    ImGui::PushFont(ImFonts[FONT_STYLE_NORMAL]);
    float textLineHeight = ImGui::GetTextLineHeight();
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), (textLineHeight * 15.f) + style.FramePadding.y*2.0f);
    if (size_arg.x<=0) {
        const float remainingSpace = ImGui::GetContentRegionAvail().x + size_arg.x;
        if (remainingSpace>0) size.x = remainingSpace;
    }
    if (showLineNumbers) {
        // FIXME: This chunk of code is executed avery frame (even if the editor is out of view/hidden)

        int firstVisibleLineNumber = 0; // It's not available right now and we don't store any 'state' here.

        // So we must enter and exit the child window space that owns the vertical scrollbar


        // This doesn't work:
        /*
        const ImVec2 cursorPos = ImGui::GetCursorPos();
        if (ImGui::BeginChildFrame(id*2, size,myChildWindowFlags))
        {
            firstVisibleLineNumber = (int) (ImGui::GetScrollY()/textLineHeight);
            ImGui::EndChildFrame();
        }
        ImGui::SetCursorPos(cursorPos);
        */
        // Workaround
        char title[256];
        ImFormatString(title, IM_ARRAYSIZE(title), "%s.%08X", window->Name, 2*id);
        ImGuiWindow* childWindow = FindWindowByName(title);
        const float scaledTextHeight = textLineHeight*(childWindow?childWindow->FontWindowScale:1.f);
        if (childWindow) firstVisibleLineNumber = (int) (childWindow->Scroll.y/scaledTextHeight);

        const int numVisibleLines =  (int) (size.y/scaledTextHeight);
        const int lastVisibleLineNumber = firstVisibleLineNumber + numVisibleLines;
        //fprintf(stderr,"%d->%d (%d)\n",firstVisibleLineNumber,lastVisibleLineNumber,numVisibleLines);
        // No time for a better approach
        const float avgSingleLetterWidth = //ImGui::CalcTextSize("9").x;
        textLineHeight*0.75f;
        if (lastVisibleLineNumber<9)          lineNumberPrecision=1;
        else if (lastVisibleLineNumber<99)    lineNumberPrecision=2;
        else if (lastVisibleLineNumber<999)   lineNumberPrecision=3;
        else if (lastVisibleLineNumber<9999)  lineNumberPrecision=4;
        else if (lastVisibleLineNumber<99999) lineNumberPrecision=5;
        else if (lastVisibleLineNumber<999999)lineNumberPrecision=6;
        else                                  lineNumberPrecision=7;
        lineNumberSize=(float)lineNumberPrecision*avgSingleLetterWidth;
        if (childWindow) lineNumberSize*=childWindow->FontWindowScale;

        if (size.x - lineNumberSize <=0) {showLineNumbers=false;lineNumberSize=0.f;}
        else size.x-=lineNumberSize;
    }
    ImGui::PopFont();
    const ImVec2 startCursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(startCursorPos.x+lineNumberSize,startCursorPos.y));
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    //const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? (style.ItemInnerSpacing.x + label_size.x) : 0.0f, 0.0f));

    ImGuiWindow* draw_window = window;    

    //static float codeEditorContentWidth = size.x;   // TODO: move to "state"

    ImGui::PushStyleColor(ImGuiCol_FrameBg,ceStyle.color_background);
    ImGui::BeginGroup();
    //ImGui::SetNextWindowContentWidth(codeEditorContentWidth);
    if (!ImGui::BeginChildFrame(id*2, frame_bb.GetSize(),myChildWindowFlags))
    {
        ImGui::EndChildFrame();
        ImGui::EndGroup();
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(startCursorPos);
        ImGui::PopID();
        ImGui::SetCursorPos(ImVec2(startCursorPos.x+size.x,startCursorPos.y+size.y));   // Mandatory to consume space
        return false;
    }

    // Zooming CTRL + MW
    if (!io.FontAllowUserScaling && io.KeyCtrl && ImGui::GetCurrentWindow()==GImGui->HoveredWindow && (io.MouseWheel || io.MouseClicked[2]))   {
        // Zoom / Scale window
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();//g.HoveredWindow;
        float new_font_scale = ImClamp(window->FontWindowScale + g.IO.MouseWheel * 0.10f, 0.75f, 2.0f);
        if (io.MouseClicked[2]) new_font_scale = 1.f;   // MMB = RESET ZOOM
        float scale = new_font_scale / window->FontWindowScale;
        window->FontWindowScale = new_font_scale;

        const ImVec2 offset = (g.IO.MousePos - window->Pos) * (1.0f - scale);
        window->Pos += offset;
        window->PosFloat += offset;
        // these two don't affect child windows AFAIK
        //window->Size *= scale;
        //window->SizeFull *= scale;
    }
    //const float inputTextFontSize = window->FontWindowScale;

    draw_window = GetCurrentWindow();
    draw_window->DC.CursorPos += style.FramePadding;
    size.x -= draw_window->ScrollbarSizes.x;
    size.y -= (draw_window->ScrollbarSizes.y+style.WindowPadding.y);

    ImGui::PushFont(ImFonts[FONT_STYLE_NORMAL]);
    textLineHeight = ImGui::GetTextLineHeight();

    // NB: we are only allowed to access 'edit_state' if we are the active widget.
    ImGuiTextEditState& edit_state = g.InputTextState;

    const bool is_ctrl_down = io.KeyCtrl;
    const bool is_shift_down = io.KeyShift;
    const bool is_alt_down = io.KeyAlt;
    const bool focus_requested = FocusableItemRegister(window, g.ActiveId == id, (flags & (ImGuiInputTextFlags_CallbackCompletion|ImGuiInputTextFlags_AllowTabInput)) == 0);    // Using completion callback disable keyboard tabbing
    //const bool focus_requested_by_code = focus_requested && (window->FocusIdxAllCounter == window->FocusIdxAllRequestCurrent);
    //const bool focus_requested_by_tab = focus_requested && !focus_requested_by_code;

    const bool hovered = IsHovered(frame_bb, id);
    if (hovered)
    {
        SetHoveredID(id);
        g.MouseCursor = ImGuiMouseCursor_TextInput;
    }
    const bool user_clicked = hovered && io.MouseClicked[0];
    const bool user_scrolled = g.ActiveId == 0 && edit_state.Id == id && g.ActiveIdPreviousFrame == draw_window->GetID("#SCROLLY");

    bool select_all = (g.ActiveId != id) && (flags & ImGuiInputTextFlags_AutoSelectAll) != 0;
    if (focus_requested || user_clicked || user_scrolled)
    {
        if (g.ActiveId != id)
        {
            // Start edition
            // Take a copy of the initial buffer value (both in original UTF-8 format and converted to wchar)
            // From the moment we focused we are ignoring the content of 'buf' (unless we are in read-only mode)
            const int prev_len_w = edit_state.CurLenW;
            edit_state.Text.resize(buf_size+1);        // wchar count <= utf-8 count. we use +1 to make sure that .Data isn't NULL so it doesn't crash.
            edit_state.InitialText.resize(buf_size+1); // utf-8. we use +1 to make sure that .Data isn't NULL so it doesn't crash.
            ImFormatString(edit_state.InitialText.Data, edit_state.InitialText.Size, "%s", buf);
            const char* buf_end = NULL;
            edit_state.CurLenW = ImTextStrFromUtf8(edit_state.Text.Data, edit_state.Text.Size, buf, NULL, &buf_end);
            edit_state.CurLenA = (int)(buf_end - buf); // We can't get the result from ImFormatString() above because it is not UTF-8 aware. Here we'll cut off malformed UTF-8.
            edit_state.CursorAnimReset();

            // Preserve cursor position and undo/redo stack if we come back to same widget
            // FIXME: We should probably compare the whole buffer to be on the safety side. Comparing buf (utf8) and edit_state.Text (wchar).
            const bool recycle_state = (edit_state.Id == id) && (prev_len_w == edit_state.CurLenW);
            if (recycle_state)
            {
                // Recycle existing cursor/selection/undo stack but clamp position
                // Note a single mouse click will override the cursor/position immediately by calling stb_textedit_click handler.
                edit_state.CursorClamp();
            }
            else
            {
                edit_state.Id = id;
                edit_state.ScrollX = 0.0f;
                stb_textedit_initialize_state(&edit_state.StbState, false);
            }
            if (flags & ImGuiInputTextFlags_AlwaysInsertMode)
                edit_state.StbState.insert_mode = true;
        }
        SetActiveID(id, window);
        FocusWindow(window);
    }
    else if (io.MouseClicked[0])
    {
        // Release focus when we click outside
        if (g.ActiveId == id)
            SetActiveID(0);
    }

    bool value_changed = false;
    bool enter_pressed = false;

    if (g.ActiveId == id)
    {
        if (!is_editable && !g.ActiveIdIsJustActivated)
        {
            // When read-only we always use the live data passed to the function
            edit_state.Text.resize(buf_size+1);
            const char* buf_end = NULL;
            edit_state.CurLenW = ImTextStrFromUtf8(edit_state.Text.Data, edit_state.Text.Size, buf, NULL, &buf_end);
            edit_state.CurLenA = (int)(buf_end - buf);
            edit_state.CursorClamp();
        }

        edit_state.BufSizeA = buf_size;

        // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
        // Down the line we should have a cleaner library-wide concept of Selected vs Active.
        g.ActiveIdAllowOverlap = !io.MouseDown[0];

        // Edit in progress
        const float mouse_x = (g.IO.MousePos.x - frame_bb.Min.x - style.FramePadding.x) + edit_state.ScrollX;
        const float mouse_y = g.IO.MousePos.y - draw_window->DC.CursorPos.y - style.FramePadding.y;

        if (select_all || (hovered && io.MouseDoubleClicked[0]))
        {
            edit_state.SelectAll();
            edit_state.SelectedAllMouseLock = true;
        }
        else if (io.MouseClicked[0] && !edit_state.SelectedAllMouseLock)
        {
            stb_textedit_click(&edit_state, &edit_state.StbState, mouse_x, mouse_y);
            edit_state.CursorAnimReset();
        }
        else if (io.MouseDown[0] && !edit_state.SelectedAllMouseLock && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
        {
            stb_textedit_drag(&edit_state, &edit_state.StbState, mouse_x, mouse_y);
            edit_state.CursorAnimReset();
            edit_state.CursorFollow = true;
        }
        if (edit_state.SelectedAllMouseLock && !io.MouseDown[0])
            edit_state.SelectedAllMouseLock = false;

        if (g.IO.InputCharacters[0])
        {
            // Process text input (before we check for Return because using some IME will effectively send a Return?)
            // We ignore CTRL inputs, but need to allow CTRL+ALT as some keyboards (e.g. German) use AltGR - which is Alt+Ctrl - to input certain characters.
            if (!(is_ctrl_down && !is_alt_down) && is_editable)
            {
                for (int n = 0; n < IM_ARRAYSIZE(g.IO.InputCharacters) && g.IO.InputCharacters[n]; n++)
                    if (unsigned int c = (unsigned int)g.IO.InputCharacters[n])
                    {
                        // Insert character if they pass filtering
                        if (!InputTextFilterCharacter(&c, flags, callback, user_data))
                            continue;
                        edit_state.OnKeyPressed((int)c);
                    }
            }

            // Consume characters
            memset(g.IO.InputCharacters, 0, sizeof(g.IO.InputCharacters));
        }

        // Handle various key-presses
        bool cancel_edit = false;
        const int k_mask = (is_shift_down ? STB_TEXTEDIT_K_SHIFT : 0);
        const bool is_ctrl_only = is_ctrl_down && !is_alt_down && !is_shift_down;
        if (IsKeyPressedMap(ImGuiKey_LeftArrow))                        { edit_state.OnKeyPressed(is_ctrl_down ? STB_TEXTEDIT_K_WORDLEFT | k_mask : STB_TEXTEDIT_K_LEFT | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_RightArrow))                  { edit_state.OnKeyPressed(is_ctrl_down ? STB_TEXTEDIT_K_WORDRIGHT | k_mask  : STB_TEXTEDIT_K_RIGHT | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_UpArrow))                     { if (is_ctrl_down) SetWindowScrollY(draw_window, draw_window->Scroll.y - textLineHeight); else edit_state.OnKeyPressed(STB_TEXTEDIT_K_UP | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_DownArrow))                   { if (is_ctrl_down) SetWindowScrollY(draw_window, draw_window->Scroll.y + textLineHeight); else edit_state.OnKeyPressed(STB_TEXTEDIT_K_DOWN| k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Home))                        { edit_state.OnKeyPressed(is_ctrl_down ? STB_TEXTEDIT_K_TEXTSTART | k_mask : STB_TEXTEDIT_K_LINESTART | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_End))                         { edit_state.OnKeyPressed(is_ctrl_down ? STB_TEXTEDIT_K_TEXTEND | k_mask : STB_TEXTEDIT_K_LINEEND | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Delete) && is_editable)       { edit_state.OnKeyPressed(STB_TEXTEDIT_K_DELETE | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Backspace) && is_editable)    { edit_state.OnKeyPressed(STB_TEXTEDIT_K_BACKSPACE | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Enter))
        {
            bool ctrl_enter_for_new_line = (flags & ImGuiInputTextFlags_CtrlEnterForNewLine) != 0;
            if ((ctrl_enter_for_new_line && !is_ctrl_down) || (!ctrl_enter_for_new_line && is_ctrl_down))
            {
                SetActiveID(0);
                enter_pressed = true;
            }
            else if (is_editable)
            {
                const bool matchTabsInPreviousLine = true;  // if we want to match tabs from the previous line after this block...
		int numTabs = 0;
                if (matchTabsInPreviousLine) {
                    int prevLineStart = edit_state.StbState.cursor;
                    while (--prevLineStart>0) {
                        if (buf[prevLineStart]=='\n') {prevLineStart++;break;}
                    }
                    //fprintf(stderr,"edit_state.StbState.cursor=%d prevLineStart=%d\n",edit_state.StbState.cursor,prevLineStart);
                    while (prevLineStart<=edit_state.StbState.cursor && buf[prevLineStart++]=='\t') ++numTabs;

                }

                unsigned int c = '\n'; // Insert new line
                if (InputTextFilterCharacter(&c, flags, callback, user_data))
                    edit_state.OnKeyPressed((int)c);

                if (matchTabsInPreviousLine)    {
                    unsigned int tab = '\t';// Insert the same amount of tabs from the previous line
                    for (int i=0;i<numTabs;i++) {
                        if (InputTextFilterCharacter(&tab, flags, callback, user_data))
                            edit_state.OnKeyPressed((int)tab);
                    }
                }
            }
        }
        else if ((flags & ImGuiInputTextFlags_AllowTabInput) && IsKeyPressedMap(ImGuiKey_Tab) && !is_ctrl_down && !is_shift_down && !is_alt_down && is_editable)
        {
            unsigned int c = '\t'; // Insert TAB
            if (InputTextFilterCharacter(&c, flags, callback, user_data))
                edit_state.OnKeyPressed((int)c);
        }
        else if (IsKeyPressedMap(ImGuiKey_Escape))                              { SetActiveID(0); cancel_edit = true; }
        else if (is_ctrl_only && IsKeyPressedMap(ImGuiKey_Z) && is_editable)    { edit_state.OnKeyPressed(STB_TEXTEDIT_K_UNDO); edit_state.ClearSelection(); }
        else if (is_ctrl_only && IsKeyPressedMap(ImGuiKey_Y) && is_editable)    { edit_state.OnKeyPressed(STB_TEXTEDIT_K_REDO); edit_state.ClearSelection(); }
        else if (is_ctrl_only && IsKeyPressedMap(ImGuiKey_A))                   { edit_state.SelectAll(); edit_state.CursorFollow = true; }
        else if (is_ctrl_only && ((IsKeyPressedMap(ImGuiKey_X) && is_editable) || IsKeyPressedMap(ImGuiKey_C)) && (edit_state.HasSelection()))
        {
            // Cut, Copy
            const bool cut = IsKeyPressedMap(ImGuiKey_X);
            if (cut && !edit_state.HasSelection())
                edit_state.SelectAll();

            if (g.IO.SetClipboardTextFn)
            {
                const int ib = edit_state.HasSelection() ? ImMin(edit_state.StbState.select_start, edit_state.StbState.select_end) : 0;
                const int ie = edit_state.HasSelection() ? ImMax(edit_state.StbState.select_start, edit_state.StbState.select_end) : edit_state.CurLenW;
                edit_state.TempTextBuffer.resize((ie-ib) * 4 + 1);
                ImTextStrToUtf8(edit_state.TempTextBuffer.Data, edit_state.TempTextBuffer.Size, edit_state.Text.Data+ib, edit_state.Text.Data+ie);
                g.IO.SetClipboardTextFn(NULL,edit_state.TempTextBuffer.Data);
            }

            if (cut)
            {
                edit_state.CursorFollow = true;
                stb_textedit_cut(&edit_state, &edit_state.StbState);
            }
        }
        else if (is_ctrl_only && IsKeyPressedMap(ImGuiKey_V) && is_editable)
        {
            // Paste
            if (g.IO.GetClipboardTextFn)
            {
                if (const char* clipboard = g.IO.GetClipboardTextFn(NULL))
                {
                    // Remove new-line from pasted buffer
                    const int clipboard_len = (int)strlen(clipboard);
                    ImWchar* clipboard_filtered = (ImWchar*)ImGui::MemAlloc((clipboard_len+1) * sizeof(ImWchar));
                    int clipboard_filtered_len = 0;
                    for (const char* s = clipboard; *s; )
                    {
                        unsigned int c;
                        s += ImTextCharFromUtf8(&c, s, NULL);
                        if (c == 0)
                            break;
                        if (c >= 0x10000 || !InputTextFilterCharacter(&c, flags, callback, user_data))
                            continue;
                        clipboard_filtered[clipboard_filtered_len++] = (ImWchar)c;
                    }
                    clipboard_filtered[clipboard_filtered_len] = 0;
                    if (clipboard_filtered_len > 0) // If everything was filtered, ignore the pasting operation
                    {
                        stb_textedit_paste(&edit_state, &edit_state.StbState, clipboard_filtered, clipboard_filtered_len);
                        edit_state.CursorFollow = true;
                    }
                    ImGui::MemFree(clipboard_filtered);
                }
            }
        }

        if (cancel_edit)
        {
            // Restore initial value
            if (is_editable)
            {
                ImFormatString(buf, buf_size, "%s", edit_state.InitialText.Data);
                value_changed = true;
            }
        }
        else
        {
            // Apply new value immediately - copy modified buffer back
            // Note that as soon as the input box is active, the in-widget value gets priority over any underlying modification of the input buffer
            // FIXME: We actually always render 'buf' when calling DrawList->AddText, making the comment above incorrect.
            // FIXME-OPT: CPU waste to do this every time the widget is active, should mark dirty state from the stb_textedit callbacks.
            if (is_editable)
            {
                edit_state.TempTextBuffer.resize(edit_state.Text.Size * 4);
                ImTextStrToUtf8(edit_state.TempTextBuffer.Data, edit_state.TempTextBuffer.Size, edit_state.Text.Data, NULL);
            }

            // User callback
            if ((flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackAlways)) != 0)
            {
                IM_ASSERT(callback != NULL);

                // The reason we specify the usage semantic (Completion/History) is that Completion needs to disable keyboard TABBING at the moment.
                ImGuiInputTextFlags event_flag = 0;
                ImGuiKey event_key = ImGuiKey_COUNT;
                if ((flags & ImGuiInputTextFlags_CallbackCompletion) != 0 && IsKeyPressedMap(ImGuiKey_Tab))
                {
                    event_flag = ImGuiInputTextFlags_CallbackCompletion;
                    event_key = ImGuiKey_Tab;
                }
                else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressedMap(ImGuiKey_UpArrow))
                {
                    event_flag = ImGuiInputTextFlags_CallbackHistory;
                    event_key = ImGuiKey_UpArrow;
                }
                else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressedMap(ImGuiKey_DownArrow))
                {
                    event_flag = ImGuiInputTextFlags_CallbackHistory;
                    event_key = ImGuiKey_DownArrow;
                }

                if (event_flag) //event_key != ImGuiKey_COUNT || (flags & ImGuiInputTextFlags_CallbackAlways) != 0)
                {
                    ImGuiTextEditCallbackData callback_data;
                    memset(&callback_data, 0, sizeof(ImGuiTextEditCallbackData));
                    callback_data.EventFlag = event_flag;
                    callback_data.Flags = flags;
                    callback_data.UserData = user_data;
                    callback_data.ReadOnly = !is_editable;

                    callback_data.EventKey = event_key;
                    callback_data.Buf = edit_state.TempTextBuffer.Data;
                    callback_data.BufTextLen = edit_state.CurLenA;
                    callback_data.BufSize = edit_state.BufSizeA;
                    callback_data.BufDirty = false;

                    // We have to convert from wchar-positions to UTF-8-positions, which can be pretty slow (an incentive to ditch the ImWchar buffer, see https://github.com/nothings/stb/issues/188)
                    ImWchar* text = edit_state.Text.Data;
                    const int utf8_cursor_pos = callback_data.CursorPos = ImTextCountUtf8BytesFromStr(text, text + edit_state.StbState.cursor);
                    const int utf8_selection_start = callback_data.SelectionStart = ImTextCountUtf8BytesFromStr(text, text + edit_state.StbState.select_start);
                    const int utf8_selection_end = callback_data.SelectionEnd = ImTextCountUtf8BytesFromStr(text, text + edit_state.StbState.select_end);

                    // Call user code
                    callback(&callback_data);

                    // Read back what user may have modified
                    IM_ASSERT(callback_data.Buf == edit_state.TempTextBuffer.Data);  // Invalid to modify those fields
                    IM_ASSERT(callback_data.BufSize == edit_state.BufSizeA);
                    IM_ASSERT(callback_data.Flags == flags);
                    if (callback_data.CursorPos != utf8_cursor_pos)            edit_state.StbState.cursor = ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.CursorPos);
                    if (callback_data.SelectionStart != utf8_selection_start)  edit_state.StbState.select_start = ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionStart);
                    if (callback_data.SelectionEnd != utf8_selection_end)      edit_state.StbState.select_end = ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionEnd);
                    if (callback_data.BufDirty)
                    {
                        //edit_state.CurLenW = ImTextStrFromUtf8(text, edit_state.Text.Size, edit_state.TempTextBuffer.Data, NULL);
                        //edit_state.CurLenA = (int)strlen(edit_state.TempTextBuffer.Data);
                        IM_ASSERT(callback_data.BufTextLen == (int)strlen(callback_data.Buf)); // You need to maintain BufTextLen if you change the text!
                        edit_state.CurLenW = ImTextStrFromUtf8(edit_state.Text.Data, edit_state.Text.Size, callback_data.Buf, NULL);
                        edit_state.CurLenA = callback_data.BufTextLen;  // Assume correct length and valid UTF-8 from user, saves us an extra strlen()

                        edit_state.CursorAnimReset();
                    }
                }
            }

            // Copy back to user buffer
            if (is_editable && strcmp(edit_state.TempTextBuffer.Data, buf) != 0)
            {
                ImFormatString(buf, buf_size, "%s", edit_state.TempTextBuffer.Data);
                value_changed = true;
            }
        }
    }

    // Render
    const ImVec4 clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + size.x, frame_bb.Min.y + size.y); // Not using frame_bb.Max because we have adjusted size
    ImVec2 render_pos = draw_window->DC.CursorPos;

    const bool canModifyText = g.ActiveId == id || (edit_state.Id == id && g.ActiveId == draw_window->GetID("#SCROLLY"));
    ImVec2 cursor_offset(0,0),render_scroll(0,0);
    if (canModifyText)
    {
        edit_state.CursorAnim += g.IO.DeltaTime;

        // We need to:
        // - Display the text (this can be more easily clipped)
        // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
        // - Measure text height (for scrollbar)
        // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
        const ImWchar* text_begin = edit_state.Text.Data;
        ImVec2 select_start_offset;
        int line_count = 0;

        {
            // Count lines + find lines numbers straddling 'cursor' and 'select_start' position.
            const ImWchar* searches_input_ptr[2];
            searches_input_ptr[0] = text_begin + edit_state.StbState.cursor;
            searches_input_ptr[1] = NULL;
            int searches_remaining = 1;
            int searches_result_line_number[2] = { -1, -999 };
            if (edit_state.StbState.select_start != edit_state.StbState.select_end)
            {
                searches_input_ptr[1] = text_begin + ImMin(edit_state.StbState.select_start, edit_state.StbState.select_end);
                searches_result_line_number[1] = -1;
                searches_remaining++;
            }

            // Iterate all lines to find our line numbers
            // In multi-line mode, we never exit the loop until all lines are counted, so add one extra to the searches_remaining counter.
            searches_remaining += 1;
            for (const ImWchar* s = text_begin; *s != 0; s++)
                if (*s == '\n')
                {
                    line_count++;
                    if (searches_result_line_number[0] == -1 && s >= searches_input_ptr[0]) { searches_result_line_number[0] = line_count; if (--searches_remaining <= 0) break; }
                    if (searches_result_line_number[1] == -1 && s >= searches_input_ptr[1]) { searches_result_line_number[1] = line_count; if (--searches_remaining <= 0) break; }
                }
            line_count++;
            if (searches_result_line_number[0] == -1) searches_result_line_number[0] = line_count;
            if (searches_result_line_number[1] == -1) searches_result_line_number[1] = line_count;

            // Calculate 2d position by finding the beginning of the line and measuring distance
            cursor_offset.x = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[0], text_begin), searches_input_ptr[0]).x;
            cursor_offset.y = searches_result_line_number[0] * textLineHeight;
            if (searches_result_line_number[1] >= 0)
            {
                select_start_offset.x = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[1], text_begin), searches_input_ptr[1]).x;
                select_start_offset.y = searches_result_line_number[1] * textLineHeight;
            }

            // Calculate text height
            //text_size = ImVec2(size.x, line_count * textLineHeight);
        }

        // Scroll
        if (edit_state.CursorFollow)
        {       
            // Horizontal scroll in chunks of quarter width
            if (!(flags & ImGuiInputTextFlags_NoHorizontalScroll))
            {
                const float scroll_increment_x = size.x * 0.25f;
                if (cursor_offset.x < edit_state.ScrollX)
                    edit_state.ScrollX = (float)(int)ImMax(0.0f, cursor_offset.x - scroll_increment_x);
                else if (cursor_offset.x - size.x >= edit_state.ScrollX)
                    edit_state.ScrollX = (float)(int)(cursor_offset.x - size.x + scroll_increment_x);
            }
            else
            {
                edit_state.ScrollX = 0.0f;
            }

            /*// Horizontal scroll
            float scroll_x = draw_window->Scroll.x;
            if (cursor_offset.x - textLineHeight < scroll_x)
                scroll_x = ImMax(0.0f, cursor_offset.x - textLineHeight);
            else if (cursor_offset.x - size.x >= scroll_x)
                scroll_x = cursor_offset.x - size.x;
            draw_window->DC.CursorPos.x += (draw_window->Scroll.x - scroll_x);   // To avoid a frame of lag
            draw_window->Scroll.x = scroll_x;
            render_pos.x = draw_window->DC.CursorPos.x;*/


            // Vertical scroll
            float scroll_y = draw_window->Scroll.y;
            if (cursor_offset.y - textLineHeight < scroll_y)
                scroll_y = ImMax(0.0f, cursor_offset.y - textLineHeight);
            else if (cursor_offset.y - size.y >= scroll_y)
                scroll_y = cursor_offset.y - size.y;
            draw_window->DC.CursorPos.y += (draw_window->Scroll.y - scroll_y);   // To avoid a frame of lag
            draw_window->Scroll.y = scroll_y;
            render_pos.y = draw_window->DC.CursorPos.y;
        }
        edit_state.CursorFollow = false;
        render_scroll = ImVec2(edit_state.ScrollX, 0.0f);

        // Draw selection
        if (edit_state.StbState.select_start != edit_state.StbState.select_end)
        {
            const ImWchar* text_selected_begin = text_begin + ImMin(edit_state.StbState.select_start, edit_state.StbState.select_end);
            const ImWchar* text_selected_end = text_begin + ImMax(edit_state.StbState.select_start, edit_state.StbState.select_end);

            float bg_offy_up = 0.0f;
            float bg_offy_dn = 0.0f;
            ImU32 bg_color = GetColorU32(ceStyle.color_text_selected_background);
            ImVec2 rect_pos = render_pos + select_start_offset - render_scroll;
            for (const ImWchar* p = text_selected_begin; p < text_selected_end; )
            {
                if (rect_pos.y > clip_rect.w + textLineHeight)
                    break;
                if (rect_pos.y < clip_rect.y)
                {
                    while (p < text_selected_end)
                        if (*p++ == '\n')
                            break;
                }
                else
                {
                    ImVec2 rect_size = InputTextCalcTextSizeW(p, text_selected_end, &p, NULL, true);
                    if (rect_size.x <= 0.0f) rect_size.x = (float)(int)(g.Font->GetCharAdvance((unsigned short)' ') * 0.50f); // So we can see selected empty lines
                    ImRect rect(rect_pos + ImVec2(0.0f, bg_offy_up - textLineHeight), rect_pos +ImVec2(rect_size.x, bg_offy_dn));
                    rect.Clip(clip_rect);
                    if (rect.Overlaps(clip_rect))
                        draw_window->DrawList->AddRectFilled(rect.Min, rect.Max, bg_color);
                }
                rect_pos.x = render_pos.x - render_scroll.x;
                rect_pos.y += textLineHeight;
            }
        }

    }


    const int firstVisibleLineNumber = (int) (ImGui::GetScrollY()/textLineHeight);
    const int numVisibleLines =  (int) (size.y/textLineHeight);
    const int lastVisibleLineNumber = firstVisibleLineNumber + numVisibleLines;
    int numLines = 0;   // Here we'll count the number of lines too while drawing text (with manual Y-clipping)

    // Render Text Here:
    {
        // Draw with manual Y-clipping-----------
        const float oldCurPosY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(oldCurPosY+firstVisibleLineNumber*textLineHeight);
        const char *lineStart = buf,*nextLineStart=NULL;
        while (lineStart)    {
            nextLineStart=strchr(lineStart,'\n');
            if (numLines>=firstVisibleLineNumber && numLines<=lastVisibleLineNumber) {
		if (numLines==firstVisibleLineNumber)   {
		    // we must set langData.pendingOpenMultilineComment, because the first visible line can be inside a multiline comment
		    // The correct code to set it is costly [but could be cached until the first visible line changes... (ATM I have no storage struct except "buf")]

		    // We''l use two code paths: one more accurate than the other:
            const bool correctlyCheckIfTheFirstVisibleLineIsInAMultilineComment = numLines<=200 &&
		    canModifyText;  // "true" only when cursor is active (= the field is focused)

		    int tokenIndexOut = -1;
		    const char* startComments[3] = {langData.startComments[0],langData.startComments[1],langData.startComments[2]};
		    const char* endComments[3] = {langData.endComments[0],langData.endComments[1],langData.endComments[2]};
		    langData.pendingOpenMultilineComment = false;

		    if (correctlyCheckIfTheFirstVisibleLineIsInAMultilineComment && startComments[0] && startComments[1])  {
			// More accurate version:
			const char *nextToken=buf;
			const int numStringDelimitersChars = langData.stringDelimiterChars ? strlen(langData.stringDelimiterChars) : 0;

			bool isInStringDelimiters = false;
                        bool waitForEndLine = false;
                        while (
                               (nextToken = ImGuiCe::FindNextToken<3>(nextToken,lineStart,startComments,endComments,&tokenIndexOut,langData.stringDelimiterChars,langData.stringEscapeChar,false,true))
                               < lineStart
                               )
                        {
                            if (!nextToken) break;
                            //fprintf(stderr,"%c%c -> %d (%d)\n",nextToken[0],nextToken[1],tokenIndexOut,(int)(lineStart-nextToken));
                            ++nextToken;
                            if (tokenIndexOut==0) {waitForEndLine = true;continue;}
                            if (waitForEndLine && tokenIndexOut!=(3+numStringDelimitersChars)) continue;
                            if (tokenIndexOut==(3+numStringDelimitersChars)) {waitForEndLine = false;continue;}

                            if (langData.pendingOpenMultilineComment)   {
                                if (tokenIndexOut==2) {langData.pendingOpenMultilineComment = false;continue;}
                            }
                            else {
                                if (tokenIndexOut==3) {isInStringDelimiters=!isInStringDelimiters;continue;}
                                if (!isInStringDelimiters && tokenIndexOut==1) {langData.pendingOpenMultilineComment = true;continue;}
                            }

                        }
                    }
		    else if (startComments[1]) {
			// Cheaper alternative:
			if (nextLineStart!=buf) {
			    // Not perfect because it does not check if multiline comments are inside single-line comments or strings, but it should work in most cases.
			    if (ImGuiCe::FindPrevToken<2>(buf,nextLineStart,&startComments[1],&endComments[1],&tokenIndexOut,langData.stringDelimiterChars,langData.stringEscapeChar,false,false) && tokenIndexOut==0) langData.pendingOpenMultilineComment = true;
			}
		    }
                }
                if (render_scroll.x!=0) ImGui::SetCursorPosX(render_pos.x - render_scroll.x - draw_window->Pos.x);
                if (!nextLineStart) {
                    MyTextLineWithSH(langData,"%s",lineStart);
                    break;
                }
                MyTextLineWithSH(langData,"%.*s",(int)(nextLineStart-lineStart),lineStart);
            }
            else if (!nextLineStart) break;
            lineStart = nextLineStart+1;
            ++numLines;
            //if (numLines>=lastVisibleLineNumber) break;    // Nope: numLines is used below
        }
        ImGui::SetCursorPosY(oldCurPosY+numLines*textLineHeight);
        //----------------------------------------

        // Debug: -------------------------------
        /*static int prevFirstVisibleLineNumber=0;
        if (firstVisibleLineNumber!=prevFirstVisibleLineNumber) {
            prevFirstVisibleLineNumber = firstVisibleLineNumber;
            fprintf(stderr,"firstVisibleLineNumber = %d numVisibleLines = %d lastVisibleLineNumber = %d size.y = %1.2f numLines = %d\n",prevFirstVisibleLineNumber,numVisibleLines,lastVisibleLineNumber,size.y,numLines);
        }*/
        // --------------------------------------

        //codeEditorContentWidth = langData.textSizeX;
    }



    if (canModifyText)   {
        // Draw blinking cursor
        ImVec2 cursor_screen_pos = render_pos + cursor_offset - render_scroll;
        bool cursor_is_visible = (g.InputTextState.CursorAnim <= 0.0f) || fmodf(g.InputTextState.CursorAnim, 1.20f) <= 0.80f;
        if (cursor_is_visible)
            draw_window->DrawList->AddLine(cursor_screen_pos + ImVec2(0.0f,-textLineHeight+0.5f), cursor_screen_pos + ImVec2(0.0f,-1.5f), GetColorU32(ImGuiCol_Text));

        // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
        if (is_editable)
            g.OsImePosRequest = ImVec2(cursor_screen_pos.x - 1, cursor_screen_pos.y - textLineHeight);
    }

    size.x += draw_window->ScrollbarSizes.x;                            // reset it
    size.y += draw_window->ScrollbarSizes.y+style.WindowPadding.y;      // reset it

    ImGui::PopFont();


    ImGui::Dummy(ImVec2(/*size.x*/langData.textSizeX,textLineHeight)); // Always add room to Y-scroll an extra line (we also make it "langData.textSizeX" wide to fix horizontal scrollbar
    ImGui::EndChildFrame();
    //ImGui::EndGroup();
    ImGui::PopStyleColor();
    const ImVec2 endCursorPos = ImGui::GetCursorPos();


    if (showLineNumbers) {
        ImGui::SetCursorPos(startCursorPos);
        //ImGui::BeginGroup();

        //CodeEditor::GetStyle().color_line_numbers_background = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2,0,0,0.4));   // TO REMOVE!
        if ((ceStyle.color_line_numbers_background&IM_COL32_A_MASK)!=0)
        {
            // Here we manually blend bg colors, so that we can paint in one pass.
            // Otherwise we have to simply use here: ImGui::PushStyleColor(ImGuiCol_FrameBg,ceStyle.color_background)
            // and activate the code block below named "Draw background"
            const ImVec4& c = ceStyle.color_background;
            ImVec4 r = ImGui::ColorConvertU32ToFloat4(ceStyle.color_line_numbers_background);
            const float rac = 1.f - r.w;
            r.x = (r.x * r.w) + (c.x * rac);
            r.y = (r.y * r.w) + (c.y * rac);
            r.z = (r.z * r.w) + (c.z * rac);
            r.w = c.w;
            ImGui::PushStyleColor(ImGuiCol_FrameBg,r);
        }
        else ImGui::PushStyleColor(ImGuiCol_FrameBg,ceStyle.color_background);
        //ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(style.WindowPadding.x,0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(style.ItemSpacing.x,0));
        if (!ImGui::BeginChildFrame(id*3, ImVec2(lineNumberSize,size.y),/*ImGuiWindowFlags_ForceHorizontalScrollbar|*/ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::EndChildFrame();
            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor();
            ImGui::EndGroup();
            ImGui::SetCursorPos(endCursorPos);
            ImGui::PopID();
            return false;
        }
        // Draw background --------------------------------------
        /*if ((ceStyle.color_line_numbers_background&IM_COL32_A_MASK)!=0)    {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 ssp(windowPos.x-ImGui::GetScrollX(),windowPos.y-ImGui::GetScrollY());
            const ImVec2 sep(ssp.x+lineNumberSize,ssp.y+ size.y);
            drawList->AddRectFilled(ssp,sep,ceStyle.color_line_numbers_background);
            fprintf(stderr,"%1.0f %1.0f %1.0f %1.0f\n",ssp.x,ssp.y,sep.x,sep.y);
        }*/
        //-------------------------------------------------------
        ImGui::PushFont(ImFonts[ceStyle.font_line_numbers]);
        g.FontSize = textLineHeight;
        ImGui::PushStyleColor(ImGuiCol_Text,ceStyle.color_line_numbers);
        for (int i = firstVisibleLineNumber;i<=lastVisibleLineNumber;i++) {
            ImGui::Text("%*d",lineNumberPrecision,(i+1));
            //ImGui::Text("%d",(i+1));
        }
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(lineNumberSize,textLineHeight)); // Always add room to Y-scroll an extra line (we also make it "langData.textSizeX" wide to fix horizontal scrollbar
        ImGui::PopFont();
        //--------------------------------------------------------
        ImGui::EndChildFrame();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(endCursorPos);
    }
    ImGui::EndGroup();
    //if (ImGui::IsItemHovered()) ImGui::SetTooltip("ScrollX = %1.2f",edit_state.ScrollX);

    // Log as text
    if (g.LogEnabled)
        LogRenderedText(render_pos, buf, NULL);

    if (label_size.x > 0)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    ImGui::PopID();

    if ((flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0)
        return enter_pressed;
    else
        return value_changed;

#   undef BAD_CODE_EDITOR_HAS_HORIZONTAL_SCROLLBAR // clean up
}



} // namespace ImGuiCe


#undef IMGUI_NEW
#undef IMGUI_DELETE
