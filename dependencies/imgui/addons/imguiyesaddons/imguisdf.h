#ifndef IMGUISDF_H_
#define IMGUISDF_H_

// WHAT'S THIS
/*

This file is intended to be used to load AngelFont text-based (= not binary or XML-based) Signed Distance Fonts (SDF) files
and to display them onscreen, NOT INSIDE ANY ImGui windows, but as a screen overlay (= not inside the 3D world)
in normal openGL rendering.

This is typically used for title/subtitle/credit screens and things like that...

It just uses the ImGui data structs, but does not interfere with it.

Dependencies:
1) Needs OpenGL WITH SHADER support.
2) Depends on other addons (i.e. imguistring.h)
*/

// HOW TO CREATE COMPATIBLE FONTS:
/*
The best solution I've found is the java program: runnable-hiero.jar, available here: https://libgdx.badlogicgames.com/tools.html

This is how I usually generate the fonts using Hiero:
I strictly follow all the guidelines under "Generating the font" in https://github.com/libgdx/libgdx/wiki/Distance-field-fonts,
with the following changes:
0) I always choose a bold font (and I prefer bold-condensed whenever available).
1) I always use "Rendering" set to "Java".
2) I can add additional codepoints directly in the "Sample Text" edit box (after that I select "Glyph cache" and "reset cache" to update them).
3) I always output a single .png page (support for multiple pages is yet to come).
4) The "Scale" value must be the last to be set: the docs suggest 32, but if you have a fast PC try something bigger (48 or 64).
5) The output .png size can be easily reduced by using 3rd party programs (e.g. pngnq -n 48 myImage.png).
*/

// HOW TO USE IT:
/* See main.cpp compiled with YES_IMGUISDF defined at the project level */

// TODO:
/*
-> Add proper clipping (glScissorTest)
*/

#ifndef IMGUI_API
#include <imgui.h>
#endif //IMGUI_API


namespace ImGui {

// Charsets --------------------------------------------------------------------------------------
struct SdfCharsetProperties {
    bool flipYOffset;
    SdfCharsetProperties(bool _flipYOffset=false) : flipYOffset(_flipYOffset) {}
};
// Tip: load the texture into fntTexture (owned by you), before calling these methods
#if (!defined(NO_IMGUISDF_LOAD) || (defined(IMGUIHELPER_H_) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD)))
struct SdfCharset* SdfAddCharsetFromFile(const char* fntFilePath,ImTextureID fntTexture,const SdfCharsetProperties& properties=SdfCharsetProperties());
#endif // (!defined(NO_IMGUISDF_LOAD) ...)
struct SdfCharset* SdfAddCharsetFromMemory(const void* data,unsigned int data_size,ImTextureID fntTexture,const SdfCharsetProperties& properties=SdfCharsetProperties());
//-------------------------------------------------------------------------------------------------

// TextChunks -------------------------------------------------------------------------------------
enum  SDFTextBufferType {
    SDF_BT_REGULAR=0,
    SDF_BT_OUTLINE=1,
    SDF_BT_SHADOWED=2
};
enum  SDFHAlignment {
    SDF_LEFT=0,
    SDF_CENTER,
    SDF_RIGHT,
    SDF_JUSTIFY
};
enum SDFVAlignment {
    SDF_TOP=0,
    SDF_MIDDLE,
    SDF_BOTTOM
};
struct SdfTextChunkProperties {
    ImVec2 boundsCenter;        // in normalized units relative to the screen size
    ImVec2 boundsHalfSize;      // in normalized units relative to the screen size
    float maxNumTextLines;      // This will determine the font size ( = boundsSizeInPixels/maxNumTextLines )
    float lineHeightOverride;  // Used if > 0.f. Usually LineHeight is something like 1.2f [that means 1.2f times the FontSize(==FontHeight)]
    SDFHAlignment halign;
    SDFVAlignment valign;
    SdfTextChunkProperties(float _maxNumTextLines=20,SDFHAlignment _halign=SDF_CENTER,SDFVAlignment _valign=SDF_MIDDLE,const ImVec2& _boundsCenter=ImVec2(.5f,.5f),const ImVec2& _boundsHalfSize=ImVec2(.5f,.5f),float _lineHeightOverride=0.f) {
        boundsCenter = _boundsCenter;
        boundsHalfSize = _boundsHalfSize;
        maxNumTextLines = _maxNumTextLines;
        lineHeightOverride = _lineHeightOverride;
        halign = _halign;
        valign = _valign;
    }
};
struct SdfTextChunk* SdfAddTextChunk(struct SdfCharset* _charset,int sdfBufferType=SDF_BT_OUTLINE, const SdfTextChunkProperties& properties=SdfTextChunkProperties(),bool preferStreamDrawBufferUsage=false);
SdfTextChunkProperties& SdfTextChunkGetProperties(struct SdfTextChunk* textChunk);
const SdfTextChunkProperties& SdfTextChunkGetProperties(const struct SdfTextChunk* textChunk);
void SdfTextChunkSetStyle(struct SdfTextChunk* textChunk,int sdfTextBufferType=SDF_BT_OUTLINE);
int SdfTextChunkGetStyle(const struct SdfTextChunk* textChunk);
void SdfTextChunkSetMute(struct SdfTextChunk* textChunk,bool flag); // Mute makes it invisible
bool SdfTextChunkGetMute(const struct SdfTextChunk* textChunk);
void SdfRemoveTextChunk(struct SdfTextChunk* chunk);
void SdfRemoveAllTextChunks();
//--------------------------------------------------------------------------------------------------

// Text---------------------------------------------------------------------------------------------
struct SdfTextColor {
    ImVec4 colorTopLeft;
    ImVec4 colorTopRight;
    ImVec4 colorBottomLeft;
    ImVec4 colorBottomRight;
    SdfTextColor(const ImVec4& color=ImVec4(1,1,1,1)) : colorTopLeft(color),colorTopRight(color),colorBottomLeft(color),colorBottomRight(color) {}
    SdfTextColor(const ImVec4& colorTop,const ImVec4& colorBottom) : colorTopLeft(colorTop),colorTopRight(colorTop),colorBottomLeft(colorBottom),colorBottomRight(colorBottom) {}
    SdfTextColor(const ImVec4& _colorTopLeft,const ImVec4& _colorTopRight,const ImVec4& _colorBottomLeft,const ImVec4& _colorBottomRight)
    : colorTopLeft(_colorTopLeft),colorTopRight(_colorTopRight),colorBottomLeft(_colorBottomLeft),colorBottomRight(_colorBottomRight) {}
    static void SetDefault(const SdfTextColor& defaultColor, bool updateAllExistingTextChunks=false);
};
static SdfTextColor SdfTextDefaultColor;
void SdfAddText(struct SdfTextChunk* chunk,const char* startText,bool italic=false,const SdfTextColor* pSdfTextColor=NULL,const ImVec2* textScaling=NULL,const char* endText=NULL,const SDFHAlignment* phalignOverride=NULL,bool fakeBold=false);
void SdfAddTextWithTags(struct SdfTextChunk* chunk,const char* startText,const char* endText=NULL);
void SdfClearText(struct SdfTextChunk* chunk);
//---------------------------------------------------------------------------------------------------


void SdfRender(const ImVec4 *pViewportOverride=NULL);   //pViewportOverride, if provided, is [x,y,width,height] in screen coordinates, not in framebuffer coords.


// Optional/Extra methods:---------------------------------------------------------------
enum SDFAnimationMode {
    SDF_AM_NONE = 0,
    SDF_AM_MANUAL,      // This mode uses a SdfAnimation (= a series of SdfAnimationKeyFrames)

    SDF_AM_FADE_IN,
    SDF_AM_ZOOM_IN,
    SDF_AM_APPEAR_IN,
    SDF_AM_LEFT_IN,
    SDF_AM_RIGHT_IN,
    SDF_AM_TOP_IN,
    SDF_AM_BOTTOM_IN,

    SDF_AM_FADE_OUT,
    SDF_AM_ZOOM_OUT,
    SDF_AM_APPEAR_OUT,
    SDF_AM_LEFT_OUT,
    SDF_AM_RIGHT_OUT,
    SDF_AM_TOP_OUT,
    SDF_AM_BOTTOM_OUT,

    SDF_AM_BLINK,
    SDF_AM_PULSE,
    SDF_AM_TYPING
};
struct SdfAnimationKeyFrame {
    ImVec2 offset;
    ImVec2 scale;   // better not use this, as it's not centered correctly (= it affects offset)
    float alpha;
    int startChar,endChar;
    float timeInSeconds;    // of the transition between the previous one and this
    SdfAnimationKeyFrame(float _timeInSeconds=0.f,float _alpha=1.f,int _startChar=0,int _endChar=-1,const ImVec2& _offset=ImVec2(0,0),const ImVec2& _scale=ImVec2(1,1)) :
    offset(_offset),scale(_scale),alpha(_alpha),startChar(_startChar),endChar(_endChar),timeInSeconds(_timeInSeconds)
    {}
};
struct SdfAnimation* SdfAddAnimation();
void SdfAnimationSetLoopingParams(struct SdfAnimation* animation,bool mustLoop,bool mustHideTextWhenFinishedIfNotLooping=true);
float SdfAnimationAddKeyFrame(struct SdfAnimation* animation,const SdfAnimationKeyFrame& keyFrame); // returns the animation total length in seconds so far
void SdfAnimationClear(struct SdfAnimation* animation);     // clears all SdfKeyFrames
void SdfRemoveAnimation(struct SdfAnimation* animation);    // "animation" no more usable
void SdfRemoveAllAnimations();


struct SdfAnimationParams {
    float speed,timeOffset;
    int startChar,endChar;
    SdfAnimationParams()
    {
        speed = 1.f;timeOffset=0.f;
        startChar=0;endChar=-1;
    }
};
struct SdfGlobalParams {
    ImVec2 offset,scale;
    float alpha;
    int startChar,endChar;
    SdfGlobalParams()
    {
        offset=ImVec2(0,0);
        scale=ImVec2(1,1);
        alpha=1.0f;
        startChar=0;endChar=-1;
    }
};
// Once an animation is active (from its mode), it plays if the text chunk is not mute.
// When it ends the mode can be set to SDF_AM_NONE and the text chunk BAN be set to mute.
// Only a manual animation can have a looping mode.
void SdfTextChunkSetManualAnimation(struct SdfTextChunk* chunk,struct SdfAnimation* animation);
const struct SdfAnimation* SdfTextChunkGetManualAnimation(const struct SdfTextChunk* chunk);
struct SdfAnimation* SdfTextChunkGetManualAnimation(struct SdfTextChunk* chunk);
void SdfTextChunkSetAnimationParams(struct SdfTextChunk* chunk,const SdfAnimationParams& params=SdfAnimationParams());
const SdfAnimationParams& SdfTextChunkGetAnimationParams(const struct SdfTextChunk* chunk);
SdfAnimationParams& SdfTextChunkGetAnimationParams(struct SdfTextChunk* chunk);
void SdfTextChunkSetGlobalParams(struct SdfTextChunk* chunk,const SdfGlobalParams& params=SdfGlobalParams());
const SdfGlobalParams& SdfTextChunkGetGlobalParams(const struct SdfTextChunk* chunk);
SdfGlobalParams& SdfTextChunkGetGlobalParams(struct SdfTextChunk* chunk);
void SdfTextChunkSetAnimationMode(struct SdfTextChunk* chunk,SDFAnimationMode mode=SDF_AM_NONE);
SDFAnimationMode SdfTextChunkGetAnimationMode(const struct SdfTextChunk* chunk);
//------------------------------------------------------------------------------------------



#ifndef NO_IMGUISDF_EDIT
bool SdfTextChunkEdit(SdfTextChunk* sdfTextChunk,char* buffer,int bufferSize);
#endif //NO_IMGUISDF_EDIT

} //namespace


#endif //IMGUISDF_H_

