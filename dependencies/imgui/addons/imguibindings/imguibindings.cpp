#include "imguibindings.h"


// These variables can be declared extern and set at runtime-----------------------------------------------------
bool gImGuiPaused = false;
bool gImGuiDynamicFPSInsideImGui = false;                      // Well, almost...
float gImGuiInverseFPSClampInsideImGui = -1.0f;    // CAN'T BE 0. < 0 = No clamping.
float gImGuiInverseFPSClampOutsideImGui = -1.0f;   // CAN'T BE 0. < 0 = No clamping.
bool gImGuiCapturesInput = false;
bool gImGuiWereOutsideImGui = true;
bool gImGuiBindingMouseDblClicked[5]={false,false,false,false,false};
bool gImGuiFunctionKeyDown[12]={false,false,false,false,false,false,false,false,false,false,false,false};
bool gImGuiFunctionKeyPressed[12]={false,false,false,false,false,false,false,false,false,false,false,false};
bool gImGuiFunctionKeyReleased[12]={false,false,false,false,false,false,false,false,false,false,false,false};
int gImGuiNumTextureBindingsPerFrame;    // read-only
ImImplVoidDelegate gImGuiPostInitGLCallback   = NULL;
ImImplVoidDelegate gImGuiPreDrawGLCallback    = NULL;
ImImplVoidDelegate gImGuiPreDrawGLSwapBuffersCallback = NULL;
ImImplVoidDelegate gImGuiPostDrawGLSwapBuffersCallback = NULL;
// --------------------------------------------------------------------------------------------------------------

#ifdef IMIMPL_BUILD_SDF
#ifndef IMIMPL_USE_SDF_SHADER
#define IMIMPL_USE_SDF_SHADER   // This is required with IMIMPL_USE_SDF_OUTLINE_SHADER too
#endif //IMIMPL_USE_SDF_SHADER
#define EDTAA3FUNC_HAS_IMGUI_SUPPORT
#include "edtaa3func.h"
#endif //IMIMPL_BUILD_SDF

#ifdef IMIMPL_USE_SDF_SHADER_OUTLINE	// Bad spelling fix
#undef IMIMPL_USE_SDF_OUTLINE_SHADER
#define IMIMPL_USE_SDF_OUTLINE_SHADER
#endif //IMIMPL_USE_SDF_SHADER_OUTLINE

#ifdef IMIMPL_USE_SDF_OUTLINE_SHADER // Ensure IMIMPL_USE_SDF_SHADER is defined too
#undef IMIMPL_USE_SDF_SHADER
#define IMIMPL_USE_SDF_SHADER
#endif //IMIMPL_USE_SDF_SHADER

#ifdef IMGUI_USE_DIRECT3D9_BINDING
#ifdef IMIMPL_BUILD_SDF
#error IMIMPL_BUILD_SDF actually works, but DIRECT3D9 shaders to use it are not supported. So we prefer stopping compilation.
#elif IMIMPL_USE_SDF_SHADER
#warning Signed distance font shaders are not supported in the DIRECT3D9 binding.
#endif //IMIMPL_USE_SDF_SHADER
#endif //IMGUI_USE_DIRECT3D9_BINDING

#ifdef IMIMPL_SHADER_NONE
#ifdef IMIMPL_BUILD_SDF
#error IMIMPL_BUILD_SDF actually works, but the shaders to use it are not supported with IMIMPL_SHADER_NONE. So we prefer stopping compilation.
#elif IMIMPL_USE_SDF_SHADER
#warning Signed distance font shaders cannot be used with IMIMPL_SHADER_NONE.
#endif //IMIMPL_USE_SDF_SHADER
#endif //IMGUI_USE_DIRECT3D9_BINDING

struct ImImpl_PrivateParams  {
#if (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))

#ifndef IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS
#define IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS 1
#elif (IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS<=0)
#undef IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS
#define IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS 1
#endif //IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS

    GLuint vertexBuffers[IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS];
    GLuint indexBuffers[IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS];
    GLuint program;
    // gProgram uniform locations:
    GLint uniLocOrthoMatrix;
    GLint uniLocTexture;
    GLint uniLocSdfParams;
    // gProgram attribute locations:
    GLint attrLocPosition;
    GLint attrLocUV;
    GLint attrLocColour;
    // font texture
    ImTextureID fontTex;

    // Default values
    ImVec4 sdfParams;

    ImImpl_PrivateParams() :program(0),uniLocOrthoMatrix(-1),uniLocTexture(-1),uniLocSdfParams(-1),
        attrLocPosition(-1),attrLocUV(-1),attrLocColour(-1),fontTex(0)
    {resetSdfParams();for (int i=0;i<IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS;i++) {vertexBuffers[i]=0;indexBuffers[i]=0;}}
    void resetSdfParams() {
        sdfParams = ImVec4(0.5f,0.4f,0.2f,0.04f);
#       ifndef IMIMPL_USE_SDF_OUTLINE_SHADER
        sdfParams.x=0.5f;
#       endif //IMIMPL_USE_SDF_OUTLINE_SHADER
    }

#else // (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
    // font texture
    ImTextureID fontTex;
    ImImpl_PrivateParams() :fontTex(0) {}
#endif // (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))

};
static ImImpl_PrivateParams gImImplPrivateParams;


#ifndef STBI_INCLUDE_STB_IMAGE_H
#ifndef IMGUI_NO_STB_IMAGE_STATIC
#define STB_IMAGE_STATIC
#endif //IMGUI_NO_STB_IMAGE_STATIC
#ifndef IMGUI_NO_STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif //IMGUI_NO_STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifndef IMGUI_NO_STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif //IMGUI_NO_STB_IMAGE_IMPLEMENTATION
#ifndef IMGUI_NO_STB_IMAGE_STATIC
#undef STB_IMAGE_STATIC
#endif //IMGUI_NO_STB_IMAGE_STATIC
#endif //STBI_INCLUDE_STB_IMAGE_H

static bool gTextureFilteringHintMagFilterNearest = false;  // These internal values can be used by ImImpl_GenerateOrUpdateTexture(...) implementations
static bool gTextureFilteringHintMinFilterNearest = false;


#ifdef IMGUI_USE_AUTO_BINDING_OPENGL
void ImImpl_FreeTexture(ImTextureID& imtexid) {
    GLuint& texid = reinterpret_cast<GLuint&>(imtexid);
    if (texid) {glDeleteTextures(1,&texid);texid=0;}
}
void ImImpl_GenerateOrUpdateTexture(ImTextureID& imtexid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt) {
    IM_ASSERT(pixels);
    IM_ASSERT(channels>0 && channels<=4);
    GLuint& texid = reinterpret_cast<GLuint&>(imtexid);
    if (texid==0) glGenTextures(1, &texid);

    glBindTexture(GL_TEXTURE_2D, texid);
    GLenum clampEnum = 0x2900;    // 0x2900 -> GL_CLAMP; 0x812F -> GL_CLAMP_TO_EDGE
#   ifndef GL_CLAMP
#       ifdef GL_CLAMP_TO_EDGE
        clampEnum = GL_CLAMP_TO_EDGE;
#       else //GL_CLAMP_TO_EDGE
        clampEnum = 0x812F;
#       endif // GL_CLAMP_TO_EDGE
#   else //GL_CLAMP
    clampEnum = GL_CLAMP;
#   endif //GL_CLAMP
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wraps ? GL_REPEAT : clampEnum);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapt ? GL_REPEAT : clampEnum);
    //const GLfloat borderColor[]={0.f,0.f,0.f,1.f};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,borderColor);
    if (gTextureFilteringHintMagFilterNearest) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (useMipmapsIfPossible)   {
#       ifdef NO_IMGUI_OPENGL_GLGENERATEMIPMAP
#           ifndef GL_GENERATE_MIPMAP
#               define GL_GENERATE_MIPMAP 0x8191
#           endif //GL_GENERATE_MIPMAP
        // I guess this is compilable, even if it's not supported:
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);    // This call must be done before glTexImage2D(...) // GL_GENERATE_MIPMAP can't be used with NPOT if there are not supported by the hardware of GL_ARB_texture_non_power_of_two.
#       endif //NO_IMGUI_OPENGL_GLGENERATEMIPMAP
    }
    if (gTextureFilteringHintMinFilterNearest) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmapsIfPossible ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmapsIfPossible ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum luminanceAlphaEnum = 0x190A; // 0x190A -> GL_LUMINANCE_ALPHA [Note that we're FORCING this definition even if when it's not defined! What should we use for 2 channels?]
#   ifdef GL_LUMINANCE_ALPHA
    luminanceAlphaEnum = GL_LUMINANCE_ALPHA;
#   endif //GL_LUMINANCE_ALPHA

    const GLenum ifmt = channels==1 ? GL_ALPHA : channels==2 ? luminanceAlphaEnum : channels==3 ? GL_RGB : GL_RGBA;  // channels == 1 could be GL_LUMINANCE, GL_ALPHA, GL_RED ...
    const GLenum fmt = ifmt;
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);

#       ifndef NO_IMGUI_OPENGL_GLGENERATEMIPMAP
    if (useMipmapsIfPossible) glGenerateMipmap(GL_TEXTURE_2D);
#       endif //NO_IMGUI_OPENGL_GLGENERATEMIPMAP
}
void ImImpl_ClearColorBuffer(const ImVec4& bgColor)  {
    glClearColor(bgColor.x,bgColor.y,bgColor.z,bgColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
}
#elif defined(IMGUI_USE_DIRECT3D9_BINDING)
void ImImpl_FreeTexture(ImTextureID& imtexid) {
    LPDIRECT3DTEXTURE9& texid = reinterpret_cast<LPDIRECT3DTEXTURE9&>(imtexid);
    if (texid) {texid->Release();texid=0;}
}
void ImImpl_GenerateOrUpdateTexture(ImTextureID& imtexid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt) {
    IM_ASSERT(pixels);
    IM_ASSERT(channels>0 && channels<=4);
    LPDIRECT3DTEXTURE9& texid = reinterpret_cast<LPDIRECT3DTEXTURE9&>(imtexid);
    if (texid==0 && D3DXCreateTexture(g_pd3dDevice, width, height,useMipmapsIfPossible ? 0 : 1, 0, channels==1 ? D3DFMT_A8 : channels==2 ? D3DFMT_A8L8 : channels==3 ? D3DFMT_R8G8B8 : D3DFMT_A8R8G8B8,D3DPOOL_MANAGED, &texid) < 0) return;

    D3DLOCKED_RECT tex_locked_rect;
    if (texid->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK) {texid->Release();texid=0;return;}
    if (channels==3 || channels==4) {
        unsigned char* pw = (unsigned char *) tex_locked_rect.Pitch;
        const unsigned char* ppxl = pixels;
        for (int y = 0; y < height; y++)    {
            pw = (unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y;  // each row has Pitch bytes
            ppxl = &pixels[y*width*channels];
            for( int x = 0; x < width; x++ )
            {
                *pw++ = ppxl[2];
                *pw++ = ppxl[1];
                *pw++ = ppxl[0];
                if (channels==4) *pw++ = ppxl[3];
                ppxl+=channels;
            }
        }
    }
    else {
        for (int y = 0; y < height; y++)    {
            memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * channels) * y, (width * channels));
        }
    }
    texid->UnlockRect(0);

    // Sorry, but I've got no idea on how to set wraps and wrapt in Direct3D9....
}
void ImImpl_ClearColorBuffer(const ImVec4& bgColor)  {
    D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(bgColor.x*255.0f), (int)(bgColor.y*255.0f), (int)(bgColor.z*255.0f), (int)(bgColor.w*255.0f));
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET/* | D3DCLEAR_ZBUFFER*/, clear_col_dx, 1.0f, 0);
}
#endif //defined(IMGUI_USE_DIRECT3D9_BINDING)


static void AddFontFromMemoryTTFCloningFontData(ImGuiIO& io, ImVector<char>& buffVec, ImFont*& my_font, const float sizeInPixels, const ImImpl_InitParams::FontData& fd)    {
    char* tempBuffer = NULL;void* bufferToFeedImGui = NULL;
    tempBuffer = (char*)ImGui::MemAlloc(buffVec.size());
    memcpy(tempBuffer,(void*)&buffVec[0],buffVec.size());
    bufferToFeedImGui = tempBuffer;
    ImImpl_InitParams::FontData fd2 = fd;fd2.fontConfig.FontDataOwnedByAtlas=true;
    my_font = io.Fonts->AddFontFromMemoryTTF(bufferToFeedImGui,buffVec.size(),sizeInPixels,fd.useFontConfig?&fd2.fontConfig:NULL,fd.pGlyphRanges);
    if (!my_font) {ImGui::MemFree(tempBuffer);tempBuffer=NULL;bufferToFeedImGui=NULL;}
}

void InitImGuiFontTexture(const ImImpl_InitParams* pOptionalInitParams) {
    if (pOptionalInitParams && pOptionalInitParams->skipBuildingFonts) return;
    ImGuiIO& io = ImGui::GetIO();
    DestroyImGuiFontTexture();	// reentrant

    if (pOptionalInitParams)    {
        const ImImpl_InitParams& P = *pOptionalInitParams;
        if (P.forceAddDefaultFontAsFirstFont) io.Fonts->AddFontDefault();

        const float screenHeight = io.DisplaySize.y;
        for (int i=0,isz=(int)P.fonts.size();i<isz;i++)   {
            const ImImpl_InitParams::FontData& fd = P.fonts[i];
            const float sizeInPixels = fd.sizeInPixels==0.f ? 13.f : (fd.sizeInPixels>0 ? fd.sizeInPixels : (screenHeight/(-fd.sizeInPixels)));
            ImFont* my_font = NULL;
            const bool hasValidPath = strlen(fd.filePath)>0;
            const bool hasValidMemory = fd.pMemoryData && fd.memoryDataSize>0;
            //if (i==0 && !P.forceAddDefaultFontAsFirstFont && (hasValidPath || hasValidMemory) && fd.useFontConfig && fd.fontConfig.MergeMode) io.Fonts->AddFontDefault();
            if (hasValidPath)  {
                char* ttfExt = strrchr((char*) fd.filePath,'.');
                char* innerExt = ttfExt ? strrchr(ttfExt,'.') : NULL;
                const int innerExtLen = innerExt ? (strlen(innerExt)-strlen(ttfExt)) : 0;
                ImVector<char> buffVec;
                // Actually in many of these methods I clone the Font Data to make ImGui own them (see AddFontFromMemoryTTFCloningFontData(...) above):
#               if (defined(IMGUI_USE_ZLIB) && !defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD))
                if (!my_font && ttfExt && (strcmp(ttfExt,".gz")==0 || strcmp(ttfExt,".GZ")==0))   {
                    if (ImGui::GzDecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                }
#               endif // IMGUI_USE_ZLIB
#               if (defined(YES_IMGUIBZ2) && !defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD))
                if (!my_font && ttfExt && (strcmp(ttfExt,".bz2")==0 || strcmp(ttfExt,".BZ2")==0))   {
                    if (ImGui::Bz2DecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                }
#               endif   //YES_IMGUIBZ2
#               if (defined(YES_IMGUISTRINGIFIER) && !defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD))
                if (!my_font && ttfExt && (strcmp(ttfExt,".b64")==0 || strcmp(ttfExt,".B64")==0 || strcmp(ttfExt,".base64")==0 || strcmp(ttfExt,".BASE64")==0))   {
#                   ifdef IMGUI_USE_ZLIB
                    if (!my_font && innerExt && (strncmp(innerExt,".gz",innerExtLen)==0 || strncmp(innerExt,".GZ",innerExtLen)==0))   {
                        if (ImGui::GzBase64DecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0)  AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    }
#                   endif //IMGUI_USE_ZLIB
#                   ifdef YES_IMGUIBZ2
                    if (!my_font && innerExt && (strncmp(innerExt,".bz2",innerExtLen)==0 || strncmp(innerExt,".BZ2",innerExtLen)==0))   {
                        if (ImGui::Bz2Base64DecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0)  AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    }
#                   endif //YES_IMGUIBZ2
                    if (!my_font && ImGui::Base64DecodeFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0)  AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                }
                if (!my_font && ttfExt && (strcmp(ttfExt,".b85")==0 || strcmp(ttfExt,".B85")==0 || strcmp(ttfExt,".base85")==0 || strcmp(ttfExt,".BASE85")==0))   {
#                   ifdef IMGUI_USE_ZLIB
                    if (!my_font && innerExt && (strncmp(innerExt,".gz",innerExtLen)==0 || strncmp(innerExt,".GZ",innerExtLen)==0))   {
                        if (ImGui::GzBase85DecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0)  AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    }
#                   endif //IMGUI_USE_ZLIB
#                   ifdef YES_IMGUIBZ2
                    if (!my_font && innerExt && (strncmp(innerExt,".bz2",innerExtLen)==0 || strncmp(innerExt,".BZ2",innerExtLen)==0))   {
                        if (ImGui::Bz2Base85DecompressFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0)  AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    }
#                   endif //YES_IMGUIBZ2
                    if (!my_font && ImGui::Base85DecodeFromFile((const char*)fd.filePath,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                }
#               endif   //YES_IMGUISTRINGIFIER
                if (!my_font) my_font = io.Fonts->AddFontFromFileTTF(fd.filePath,sizeInPixels,fd.useFontConfig?&fd.fontConfig:NULL,fd.pGlyphRanges);
            }
            else if (hasValidMemory)  {
                ImVector<char> buffVec;
                bool mustCloneMemoryBufferBecauseImGuiDeletesIt = (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_NONE && fd.useFontConfig) ? fd.fontConfig.FontDataOwnedByAtlas : true;//false;//true;
#if             (!defined(NO_IMGUIHELPER) && defined(IMGUI_USE_ZLIB))
                if (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_GZ) mustCloneMemoryBufferBecauseImGuiDeletesIt = false;
#if                 (defined(YES_IMGUISTRINGIFIER))
                    if (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_GZBASE64 ||
                    fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_GZBASE85) mustCloneMemoryBufferBecauseImGuiDeletesIt = false;
#                   endif
#               endif
#if             (defined(YES_IMGUIBZ2))
                if (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_BZ2) mustCloneMemoryBufferBecauseImGuiDeletesIt = false;
#if                 (defined(YES_IMGUISTRINGIFIER))
                    if (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_BZ2BASE64 ||
                    fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_BZ2BASE85) mustCloneMemoryBufferBecauseImGuiDeletesIt = false;
#                   endif
#               endif
#if             (defined(YES_IMGUISTRINGIFIER))
                if (fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_BASE64 ||
                fd.memoryDataCompression==ImImpl_InitParams::FontData::COMP_BASE85) mustCloneMemoryBufferBecauseImGuiDeletesIt = false;
#               endif
                char* tempBuffer = NULL;void* bufferToFeedImGui = NULL;
                if (!mustCloneMemoryBufferBecauseImGuiDeletesIt) bufferToFeedImGui = (void*) fd.pMemoryData;
                else {
                    tempBuffer = (char*)ImGui::MemAlloc(fd.memoryDataSize);
                    memcpy(tempBuffer,(void*)fd.pMemoryData,fd.memoryDataSize);
                    bufferToFeedImGui = tempBuffer;
                }
                switch (fd.memoryDataCompression)   {
                case ImImpl_InitParams::FontData::COMP_NONE:
                    my_font = io.Fonts->AddFontFromMemoryTTF(bufferToFeedImGui,fd.memoryDataSize,sizeInPixels,fd.useFontConfig?&fd.fontConfig:NULL,fd.pGlyphRanges);
                    break;
                case ImImpl_InitParams::FontData::COMP_STB:
                    my_font = io.Fonts->AddFontFromMemoryCompressedTTF(bufferToFeedImGui,fd.memoryDataSize,sizeInPixels,fd.useFontConfig?&fd.fontConfig:NULL,fd.pGlyphRanges);
                    break;
                case ImImpl_InitParams::FontData::COMP_STBBASE85:
                    my_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF((const char*)bufferToFeedImGui,sizeInPixels,fd.useFontConfig?&fd.fontConfig:NULL,fd.pGlyphRanges);
                    break;
#if             (!defined(NO_IMGUIHELPER) && defined(IMGUI_USE_ZLIB))
                case ImImpl_InitParams::FontData::COMP_GZ:
                    if (ImGui::GzDecompressFromMemory((const char*)fd.pMemoryData,fd.memoryDataSize,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
#   ifdef           YES_IMGUISTRINGIFIER
                case ImImpl_InitParams::FontData::COMP_GZBASE64:
                    if (ImGui::GzBase64DecompressFromMemory((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
                case ImImpl_InitParams::FontData::COMP_GZBASE85:
                    if (ImGui::GzBase85DecompressFromMemory((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
#   endif           //YES_IMGUISTRINGIFIER
#               endif   //IMGUI_USE_ZLIB
#if             (defined(YES_IMGUIBZ2))
                case ImImpl_InitParams::FontData::COMP_BZ2:
                    if (ImGui::Bz2DecompressFromMemory((const char*)fd.pMemoryData,fd.memoryDataSize,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
#   ifdef           YES_IMGUISTRINGIFIER
                case ImImpl_InitParams::FontData::COMP_BZ2BASE64:
                    if (ImGui::Bz2Base64Decode((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
                case ImImpl_InitParams::FontData::COMP_BZ2BASE85:
                    if (ImGui::Bz2Base85Decode((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
#   endif           //YES_IMGUISTRINGIFIER
#               endif   //IMGUI_USE_ZLIB
#   ifdef       YES_IMGUISTRINGIFIER
                case ImImpl_InitParams::FontData::COMP_BASE64:
                    if (ImGui::Base64Decode((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
                case ImImpl_InitParams::FontData::COMP_BASE85:
                    if (ImGui::Base85Decode((const char*)fd.pMemoryData,buffVec) && buffVec.size()>0) AddFontFromMemoryTTFCloningFontData(io, buffVec, my_font, sizeInPixels, fd);
                    break;
#   endif       //YES_IMGUISTRINGIFIER
                default:
                    IM_ASSERT(true);    //Unsupported font compression
                    break;
                }
            }

            if (!my_font)   fprintf(stderr,"An error occurred while trying to load font %d\n",i);
        }

        if (!P.forceAddDefaultFontAsFirstFont && P.fonts.size()==0) io.Fonts->AddFontDefault();

    }
    else io.Fonts->AddFontDefault();

    // Load font texture
    unsigned char* pixels;
    int width, height;
    //fprintf(stderr,"Loading font texture\n");
#	ifdef IMIMPL_BUILD_SDF
	if (!io.Fonts->TexPixelsAlpha8) {
#   	ifndef YES_IMGUIFREETYPE
		io.Fonts->GetTexDataAsAlpha8(&io.Fonts->TexPixelsAlpha8,NULL,NULL);
#		else //YES_IMGUIFREETYPE
		ImGuiFreeType::GetTexDataAsAlpha8(io.Fonts,&io.Fonts->TexPixelsAlpha8,NULL,NULL,NULL,ImGuiFreeType::DefaultRasterizationFlags,&ImGuiFreeType::DefaultRasterizationFlagVector);
#		endif //YES_IMGUIFREETYPE
	}    	
	ImGui::PostBuildForSignedDistanceFontEffect(io.Fonts);
#	endif
#   ifndef YES_IMGUIFREETYPE
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
#   else //YES_IMGUIFREETYPE
    ImGuiFreeType::GetTexDataAsRGBA32(io.Fonts,&pixels, &width, &height,NULL,ImGuiFreeType::DefaultRasterizationFlags,&ImGuiFreeType::DefaultRasterizationFlagVector);
#   endif //YES_IMGUIFREETYPE

#   if ((!defined(IMIMPL_USE_SDF_SHADER) && !defined(IMIMPL_USE_ALPHA_SHARPENER_SHADER) && !defined(IMIMPL_USE_FONT_TEXTURE_LINEAR_FILTERING)) || defined(IMIMPL_USE_FONT_TEXTURE_NEAREST_FILTERING))
    gTextureFilteringHintMagFilterNearest = true;
    //printf("Using nearest filtering for ImGui Font Texture\n");
#   endif // (!defined(IMGUI_USE_SDL_SHADER) && ! !defined(IMGUI_USE_ALPHA_SHARPENER_SHADER))
    gTextureFilteringHintMinFilterNearest = false;
    ImImpl_GenerateOrUpdateTexture(gImImplPrivateParams.fontTex,width,height,4,pixels,false,true,true);
    gTextureFilteringHintMagFilterNearest = gTextureFilteringHintMinFilterNearest = false;


    // Store our identifier
    io.Fonts->TexID = gImImplPrivateParams.fontTex;

#   ifdef IMGUIBINDINGS_CLEAR_INPUT_DATA_SOON
    // Cleanup (don't clear the input data if you want to append new fonts later)    
    io.Fonts->ClearInputData();
    io.Fonts->ClearTexData();
#   endif //IMGUIBINDINGS_CLEAR_INPUT_DATA_SOON

    //fprintf(stderr,"Loaded font texture\n");

// We overuse this method to load textures from other imgui addons
#   ifndef NO_IMGUITABWINDOW
    if (!ImGui::TabWindow::DockPanelIconTextureID)  {
        int dockPanelImageBufferSize = 0;
        const unsigned char* dockPanelImageBuffer = ImGui::TabWindow::GetDockPanelIconImagePng(&dockPanelImageBufferSize);
        ImGui::TabWindow::DockPanelIconTextureID = ImImpl_LoadTextureFromMemory(dockPanelImageBuffer,dockPanelImageBufferSize);
    }
#   endif //NO_IMGUITABWINDOW

}

void DestroyImGuiFontTexture()	{
    if (gImImplPrivateParams.fontTex)	{
#       ifndef IMGUIBINDINGS_CLEAR_INPUT_DATA_SOON
        ImGuiIO& io = ImGui::GetIO();
        // Cleanup (don't clear the input data if you want to append new fonts later)
        if (io.Fonts) io.Fonts->ClearInputData();
        if (io.Fonts) io.Fonts->ClearTexData();
#       endif //IMGUIBINDINGS_CLEAR_INPUT_DATA_SOON
        ImImpl_FreeTexture(gImImplPrivateParams.fontTex);
        gImImplPrivateParams.fontTex = 0;
    }

// We overuse this method to delete textures from other imgui addons
#   ifndef NO_IMGUITABWINDOW
    if (ImGui::TabWindow::DockPanelIconTextureID) {
        ImImpl_FreeTexture(ImGui::TabWindow::DockPanelIconTextureID);
        ImGui::TabWindow::DockPanelIconTextureID = NULL;
    }
#   endif //NO_IMGUITABWINDOW
}

#ifndef _WIN32
#include <unistd.h>
#else //_WIN32
// Is there a header with ::Sleep(...) ?
#endif //_WIN32
void WaitFor(unsigned int ms)    {
#ifdef _WIN32
  if (ms > 0) Sleep(ms);
#else
  // delta in microseconds
  useconds_t delta = (useconds_t) ms * 1000;
  // On some systems, the usleep argument must be < 1000000
  while (delta > 999999L)   {
    usleep(999999);
    delta -= 999999L;
  }
  if (delta > 0L) usleep(delta);
#endif
}


void ImImpl_FlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip)   {
    stbi_set_flip_vertically_on_load(flag_true_if_should_flip);
}

ImTextureID ImImpl_LoadTextureFromMemory(const unsigned char* filenameInMemory,int filenameInMemorySize,int req_comp,bool useMipmapsIfPossible,bool wraps,bool wrapt)  {
    int w,h,n;
    unsigned char* pixels = stbi_load_from_memory(filenameInMemory,filenameInMemorySize,&w,&h,&n,req_comp);
    if (!pixels) {
        fprintf(stderr,"Error: can't load texture from memory\n");
        return 0;
    }
    if (req_comp>0 && req_comp<=4) n = req_comp;

    ImTextureID texId = NULL;
    ImImpl_GenerateOrUpdateTexture(texId,w,h,n,pixels,useMipmapsIfPossible,wraps,wrapt);

    stbi_image_free(pixels);

    return texId;
}


ImTextureID ImImpl_LoadTexture(const char* filename, int req_comp, bool useMipmapsIfPossible, bool wraps, bool wrapt)  {
    // We avoid using stbi_load(...), because we support UTF8 paths under Windows too.
    int file_size = 0;
    unsigned char* file = (unsigned char*) ImLoadFileToMemory(filename,"rb",&file_size,0);
    ImTextureID texId = NULL;
    if (file)   {
        texId = ImImpl_LoadTextureFromMemory(file,file_size,req_comp,useMipmapsIfPossible,wraps,wrapt);
        ImGui::MemFree(file);file=NULL;
    }
    return texId;
}

#ifdef IMGUI_USE_AUTO_BINDING_OPENGL

#ifndef IMIMPL_SHADER_NONE
#ifndef NO_IMGUISTRING
void ImImpl_CompileShaderStruct::addPreprocessorDefinition(const ImString& name,const ImString& value) {
    if (value.size()>0) mPreprocessorDefinitionsWithValue.put(name,value);
    else mPreprocessorDefinitions.push_back(name);
}
void ImImpl_CompileShaderStruct::removePreprocessorDefinition(const ImString& name)   {
    for (int i=0,sz=mPreprocessorDefinitions.size();i<sz;i++)   {
        if (mPreprocessorDefinitions[i] == name)    {
            mPreprocessorDefinitions[i] = mPreprocessorDefinitions[sz-1];
            mPreprocessorDefinitions.resize(sz-1);
            break;
        }
    }
    mPreprocessorDefinitionsWithValue.remove(name);
}
void ImImpl_CompileShaderStruct::updatePreprocessorDefinitions()   {
    mNumPreprocessorAdditionalLines = 0;
    mPreprocessorAdditionalShaderCode = "";
    ImString name="",value="";int bucketIndex=0;void* node = NULL;
    while ( (node = mPreprocessorDefinitionsWithValue.getNextPair(bucketIndex,node,name,value)) )  {
        mPreprocessorAdditionalShaderCode+="#define "+name+" "+value+"\n";
        ++mNumPreprocessorAdditionalLines;
    }
    for (int i=0,sz=mPreprocessorDefinitions.size();i<sz;i++)   {
        const ImString& name = mPreprocessorDefinitions[i];
        mPreprocessorAdditionalShaderCode+="#define "+name+"\n";++mNumPreprocessorAdditionalLines;
    }
    //fprintf(stderr,"mPreprocessorAdditionalShaderCode:\n\"%s\"\n",mPreprocessorAdditionalShaderCode.c_str());
}
void ImImpl_CompileShaderStruct::resetPreprocessorDefinitions()    {
    mPreprocessorAdditionalShaderCode="";
    mNumPreprocessorAdditionalLines=0;
    mPreprocessorDefinitions.clear();
    mPreprocessorDefinitionsWithValue.clear();
}

void ImImpl_CompileShaderStruct::bindAttributeLocation(const ImString &attribute, GLuint bindingLocation) {
    mBindAttributeMap.push_back(ImPair<ImString,GLuint>(attribute,bindingLocation));
    mBindAttributes=true;
}
void ImImpl_CompileShaderStruct::resetBindAttributeLocations() {mBindAttributeMap.clear();mBindAttributes=false;}
void ImImpl_CompileShaderStruct::processBindAttributeLocationsOn(GLuint program) const {
    if (mBindAttributes && program) {
        for (int i=0,isz=mBindAttributeMap.size();i<isz;i++)    {
            const ImPair<ImString,GLuint>& p = mBindAttributeMap[i];
            glBindAttribLocation(program,p.second,p.first.c_str());
        }
    }
}
#endif //NO_IMGUISTRING
static GLuint ImImpl_CreateShader(GLenum type,const GLchar** shaderSource, ImImpl_CompileShaderStruct *pOptionalOptions, const char* shaderTypeName="Shader",const int prefixFileNumLines=0)    {

    const char* sourcePrefix = NULL;
    int numPrefixLines =  0;
    if (pOptionalOptions)   {
#       ifndef NO_IMGUISTRING
        int lenPd = strlen(pOptionalOptions->getPreprocessorDefinitionAdditionalCode());
        //if (lenPd==0) {pOptionalOptions->updatePreprocessorDefinitions();lenPd=strlen(pOptionalOptions->getPreprocessorDefinitionAdditionalCode());}
        if (lenPd>0)  {
            sourcePrefix = pOptionalOptions->getPreprocessorDefinitionAdditionalCode();
            numPrefixLines = pOptionalOptions->getNumPreprocessorDefinitionAdditionalLines();
        }
#       endif //NO_IMGUISTRING
    }

    //Preprocess shader source
    const GLchar* pSource = *shaderSource;
#   ifndef NO_IMGUISTRING
    ImString tmp = "";
    if (sourcePrefix) {
        if (numPrefixLines==0)  {
            for (const char* p=sourcePrefix;*p!='\0';p++)   {
                if (*p=='\n') ++numPrefixLines;
            }
        }
        // "#version" (and maybe "precision" or "#extension" ?) must be preserved at the top
        const char* rv = NULL;
        if (pSource && pSource[0]=='#' && pSource[1]=='v' && (rv = strchr(pSource,'\n'))) {
            const size_t beg = (rv-pSource);
            ImString tmp2 = ImString(pSource);
            tmp = tmp2.substr(0,beg+1)+sourcePrefix+tmp2.substr(beg+1);
        }
        else {
            tmp = sourcePrefix;
            tmp+= ImString(pSource);
        }
        pSource=tmp.c_str();
    }
#   endif //NO_IMGUISTRING

    //Compile shader
    GLuint shader( glCreateShader( type ) );
    glShaderSource( shader, 1, &pSource, NULL );
    glCompileShader( shader );

    // check
    GLint bShaderCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &bShaderCompiled);

    if (!bShaderCompiled)        {
        int i32InfoLogLength, i32CharsWritten;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

        ImVector<char> pszInfoLog; pszInfoLog.resize(i32InfoLogLength+2);pszInfoLog[0]='\0';
        glGetShaderInfoLog(shader, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);
        if (numPrefixLines==0 && prefixFileNumLines==0) printf("********%s %s\n", shaderTypeName, &pszInfoLog[0]);
        else {            
#           ifndef NO_IMGUISTRING
            printf("\n********%sLog\n",shaderTypeName);
            ImString logString = &pszInfoLog[0];
            int beg,beg2=1;ImVector<char> tmp; tmp.resize(16);int netLineNumber = 0;
            while ( (beg = logString.find('(',beg2))!=ImString::npos && logString[beg-1]=='0' ) {
                beg2 = beg+1;   // beg2 is after '('
                if ( (beg = logString.find(')',beg2))!=ImString::npos && beg>beg2 && beg-beg2<6 )   {
                    int originalLineNumber = -1;
                    sscanf(logString.substr(beg2,beg-beg2).c_str(),"%d",&originalLineNumber);
                    if (originalLineNumber < numPrefixLines) printf("ImImpl_CreateShader(...). Error in autogenerated preprocessor definition code\n");
                    else {
                        tmp[0]='\0';
                        netLineNumber = (originalLineNumber - numPrefixLines);
                        const bool errorIsInPrefixFile = netLineNumber<=prefixFileNumLines;
                        if (!errorIsInPrefixFile) netLineNumber-=prefixFileNumLines;
                        const int tmpSz = sprintf(&tmp[0],"%d",netLineNumber);
                        IM_ASSERT(tmpSz>0 && tmpSz<15);
                        if (tmpSz>0) tmp[tmpSz]='\0';
                        logString = logString.substr(0,beg2) + ImString(&tmp[0]) + (errorIsInPrefixFile?"i":"") + logString.substr(beg);
                        beg2+= strlen(&tmp[0]) + 1 + (errorIsInPrefixFile?1:0);
                    }
                }
            }
            printf("%s\n", logString.c_str());
#           else //NO_IMGUISTRING
            printf("********%s %s\n", shaderTypeName, &pszInfoLog[0]);
#           endif //NO_IMGUISTRING
        }
        fflush(stdout);
        glDeleteShader(shader);shader=0;
    }

    return shader;
}
static inline void ImImpl_MergeTwoSourceFiles(ImVector<char>& rv,const char* firstFile,const char* secondFile,int* pFirstFileNumLinesOut)    {
    rv.clear();
    const int lenff = strlen(firstFile);
    const int lensf = strlen(secondFile);
    rv.resize(lenff+lensf+1);
    strcpy(&rv[0],firstFile);strcat(&rv[0],secondFile);
    if (pFirstFileNumLinesOut) {
        *pFirstFileNumLinesOut = 0;
        for (const char* p =firstFile,*pEnd=firstFile+lenff;p!=pEnd;++p) {
            if (*p=='\n') ++(*pFirstFileNumLinesOut);
        }        
    }
}
GLuint ImImpl_CompileShadersFromMemory(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource , ImImpl_CompileShaderStruct *pOptionalOptions,const GLchar** optionalVertexShaderCodePrefix, const GLchar** optionalFragmentShaderCodePrefix)
{
    GLuint vertexShader = 0, fragmentShader = 0;
    bool mustProcessVertexShaderSource = true, mustProcessFragmentShaderSource = true;
    if (pOptionalOptions)   {
        mustProcessVertexShaderSource = false;
        if (pOptionalOptions->vertexShaderOverride) vertexShader = pOptionalOptions->vertexShaderOverride;
        else if (!pOptionalOptions->programOverride) mustProcessVertexShaderSource = true;

        mustProcessFragmentShaderSource = false;
        if (pOptionalOptions->fragmentShaderOverride) fragmentShader = pOptionalOptions->fragmentShaderOverride;
        else if (!pOptionalOptions->programOverride) mustProcessFragmentShaderSource = true;
    }


    int vertexShaderSourcePrefixNumLines = 0;
    if (mustProcessVertexShaderSource)  {
        const GLchar* sourcePrefix = (optionalVertexShaderCodePrefix && *optionalVertexShaderCodePrefix) ? *optionalVertexShaderCodePrefix : NULL;
        ImVector<char> tmp;
        if (sourcePrefix) {
            ImImpl_MergeTwoSourceFiles(tmp,sourcePrefix,*vertexShaderSource,&vertexShaderSourcePrefixNumLines);
            const GLchar* pSource = &tmp[0];
            vertexShader = ImImpl_CreateShader(GL_VERTEX_SHADER,&pSource,pOptionalOptions,"VertexShader",vertexShaderSourcePrefixNumLines);
        }
        else vertexShader = ImImpl_CreateShader(GL_VERTEX_SHADER,vertexShaderSource,pOptionalOptions,"VertexShader");
    }

    int fragmentShaderSourcePrefixNumLines = 0;
    if (mustProcessFragmentShaderSource)    {
        const GLchar* sourcePrefix = (optionalFragmentShaderCodePrefix && *optionalFragmentShaderCodePrefix) ? *optionalFragmentShaderCodePrefix : NULL;
        ImVector<char> tmp;
        if (sourcePrefix) {
            ImImpl_MergeTwoSourceFiles(tmp,sourcePrefix,*fragmentShaderSource,&fragmentShaderSourcePrefixNumLines);
            const GLchar* pSource = &tmp[0];
            fragmentShader = ImImpl_CreateShader(GL_FRAGMENT_SHADER,&pSource,pOptionalOptions,"FragmentShader",fragmentShaderSourcePrefixNumLines);
        }
        else fragmentShader = ImImpl_CreateShader(GL_FRAGMENT_SHADER,fragmentShaderSource,pOptionalOptions,"FragmentShader");
    }

    //Link vertex and fragment shader together
    GLuint program = 0;
    if (pOptionalOptions && pOptionalOptions->programOverride!=0) program = pOptionalOptions->programOverride;
    else program = glCreateProgram();

    if (vertexShader)   glAttachShader( program, vertexShader );
    if (fragmentShader) glAttachShader( program, fragmentShader );

    if (pOptionalOptions && pOptionalOptions->dontLinkProgram)  {
        pOptionalOptions->programOverride = program;
        if (pOptionalOptions->dontDeleteAttachedShaders) {
            pOptionalOptions->vertexShaderOverride   = vertexShader;
            pOptionalOptions->fragmentShaderOverride = fragmentShader;
        }
        return program;
    }

#   ifndef NO_IMGUISTRING
    if (pOptionalOptions) pOptionalOptions->processBindAttributeLocationsOn(program);
#   endif //NO_IMGUISTRING

    glLinkProgram( program );

    //check
    GLint bLinked;
    glGetProgramiv(program, GL_LINK_STATUS, &bLinked);
    if (!bLinked)        {
        int i32InfoLogLength, i32CharsWritten;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

        ImVector<char> pszInfoLog; pszInfoLog.resize(i32InfoLogLength+2);pszInfoLog[0]='\0';
        glGetProgramInfoLog(program, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);

#       ifndef NO_IMGUISTRING
        printf("\n********ShaderLinkerLog\n");
        ImString logString = &pszInfoLog[0];
        int beg,beg2=1;ImVector<char> tmp; tmp.resize(16);int netLineNumber = 0;
        int numPrefixLines = 0;
        int prefixFileNumLines = vertexShaderSourcePrefixNumLines;
        if (pOptionalOptions) numPrefixLines = pOptionalOptions->getNumPreprocessorDefinitionAdditionalLines();

        //int vertexInfoStart = logString.find("Vertex info");
        int fragmentInfoStart = logString.find("Fragment info");
        if (fragmentInfoStart==ImString::npos) fragmentInfoStart = 10*logString.size();

        while ( (beg = logString.find('(',beg2))!=ImString::npos && logString[beg-1]=='0' ) {
            if (beg>fragmentInfoStart) prefixFileNumLines = fragmentShaderSourcePrefixNumLines;
            beg2 = beg+1;   // beg2 is after '('
            if ( (beg = logString.find(')',beg2))!=ImString::npos && beg>beg2 && beg-beg2<6 )   {
                int originalLineNumber = -1;
                sscanf(logString.substr(beg2,beg-beg2).c_str(),"%d",&originalLineNumber);
                if (originalLineNumber < numPrefixLines) printf("ImImpl_CompileShadersFromMemory(...). Error in autogenerated preprocessor definition code\n");
                else {
                    tmp[0]='\0';
                    netLineNumber = (originalLineNumber - numPrefixLines);
                    const bool errorIsInPrefixFile = netLineNumber<=prefixFileNumLines;
                    if (!errorIsInPrefixFile) netLineNumber-=prefixFileNumLines;
                    const int tmpSz = sprintf(&tmp[0],"%d",netLineNumber);
                    IM_ASSERT(tmpSz>0 && tmpSz<15);
                    if (tmpSz>0) tmp[tmpSz]='\0';
                    logString = logString.substr(0,beg2) + ImString(&tmp[0]) + (errorIsInPrefixFile?"i":"") + logString.substr(beg);
                    beg2+= strlen(&tmp[0]) + 1 + (errorIsInPrefixFile?1:0);
                }
            }
        }
        printf("%s\n", logString.c_str());
#       else //NO_IMGUISTRING
        printf("********ShaderLinkerLog:\n%s\n", &pszInfoLog[0]);
#       endif //NO_IMGUISTRING
        fflush(stdout);
        if (!pOptionalOptions || !pOptionalOptions->dontDeleteAttachedShaders) {
            glDetachShader(program,vertexShader);glDeleteShader(vertexShader);vertexShader=0;
            glDetachShader(program,fragmentShader);glDeleteShader(fragmentShader);fragmentShader=0;
        }
        glDeleteProgram(program);program=0;
    }

    //Delete shaders objects
    if (!pOptionalOptions || !pOptionalOptions->dontDeleteAttachedShaders)  {
        if (program)    {
            GLuint attachedShaders[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};GLsizei attachedShaderCount = 0;
            glGetAttachedShaders(program,sizeof(attachedShaders)/sizeof(attachedShaders[0]),&attachedShaderCount,attachedShaders);
            for (GLsizei i=0;i<attachedShaderCount;i++) glDeleteShader( attachedShaders[i] );
        }
    }

    return program;
}
#   ifndef NO_IMGUIHELPER
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
static inline const char* ImImpl_GetIncludeFileName(const char* pStart,const char * pEnd,ImVector<char>& includeNameOut)    {
    includeNameOut.clear();
    if (!pEnd) pEnd = pStart + strlen(pStart);
    const char* p = pStart;while (p!=pEnd && (*p==' ' || *p=='\t')) ++p;
    if (p==pEnd || *p!='#') return pStart;
    ++p;while (p!=pEnd && (*p==' ' || *p=='\t')) ++p;
    if (p==pEnd) return pStart;
    if (strncmp(p,"include",7)!=0) return pStart;
    p+=7;while (p!=pEnd && (*p==' ' || *p=='\t')) ++p;
    if (p==pEnd || (*p!='"' && *p!='<')) return pStart;
    const char nextMatch = (*p=='<') ? '>' : '"';
    ++p;if (p==pEnd) return pStart;
    const char* startName = p;
    while (p!=pEnd && *p!='\n' && *p!=nextMatch) ++p;
    if (p==pEnd || *p!=nextMatch) return pStart;
    const char* endName = p;
    const int nameLen = endName - startName;
    includeNameOut.resize(nameLen+1);strncpy(&includeNameOut[0],startName,nameLen);includeNameOut[nameLen]='\0';
    //printf("ImImpl_GetIncludeFileName(...): Found include file named: \"%.*s\" (\"%s\")\n",nameLen,startName,&includeNameOut[0]);fflush(stdout);
    //while (p!=pEnd && (*p!='\n' || *p!='\r')) ++p;
    ++p;
    //fprintf(stderr,"[p=\"%s\"]\n",p);
    return p;
}
static inline void ImImpl_MergePathInternal(const char* basePath,const char* relativePath,ImVector<char>& fullPathOut)  {
    IM_ASSERT(basePath && relativePath);
    fullPathOut.clear();
    const char* r1 = strrchr(basePath,'/');
    const char* r2 = strrchr(basePath,'\\');
    if (r1==NULL) {r1 = r2;r2 = NULL;}
    if (r1 && r2) {
        if (r1<r2) r1 = r2;
        r2 = NULL;
    }
    int length = strlen(relativePath);
    if (!r1)    {
        fullPathOut.resize(length+1);strcpy(&fullPathOut[0],relativePath);
        return;
    }
    int baselen = (r1-basePath)+1;
    fullPathOut.resize(baselen+length+1);
    strncpy(&fullPathOut[0],basePath,baselen);fullPathOut[baselen]='\0';
    //strcat(&fullPathOut[0],"/");
    strcat(&fullPathOut[0],relativePath);
    //printf("ImImpl_MergePathInternal(...): Shader \"%s\" included \"%s\": the full include path is: \"%s\"\n",basePath,relativePath,&fullPathOut[0]);fflush(stdout);
}
GLuint ImImpl_CompileShadersFromFile(const char* vertexShaderFilePath, const char* fragmentShaderFilePath, ImImpl_CompileShaderStruct* pOptionalOptions, bool allowProcessingASingleIncludeDirectivePlacedAtTheFirstLineOfTheShaderCode) {
    ImVector<char> vertexShaderBuffer,fragmentShaderBuffer,prefixVertexShaderBuffer,prefixFragmentShaderBuffer;
    // bool GetFileContent(const char* filePath,ImVector<char>& contentOut,bool clearContentOutBeforeUsage=true,const char* modes="rb",bool appendTrailingZeroIfModesIsNotBinary=true);
    bool vok = vertexShaderFilePath ? ImGuiHelper::GetFileContent(vertexShaderFilePath,    vertexShaderBuffer,     true,"r",true) : true;
    bool fok = fragmentShaderFilePath ? ImGuiHelper::GetFileContent(fragmentShaderFilePath,  fragmentShaderBuffer,   true,"r",true) : true;
    if (!vok || !fok)    {
        printf("Error in ImImpl_CompileShadersFromFile(\"%s\",\"%s\"). ",vertexShaderFilePath,fragmentShaderFilePath);
        if (!vok) printf("Vertex shader file not found. ");
        if (!fok) printf("Fragment shader file not found. ");
        fflush(stdout);
        return 0;
    }
    const char *pStartVertexShaderBuffer   = vertexShaderFilePath ? &vertexShaderBuffer[0] : NULL;
    const char *pStartFragmentShaderBuffer = fragmentShaderFilePath ? &fragmentShaderBuffer[0] : NULL;
    const char *pStartPrefixVertexShaderBuffer = NULL;
    const char *pStartPrefixFragmentShaderBuffer = NULL;

    if (allowProcessingASingleIncludeDirectivePlacedAtTheFirstLineOfTheShaderCode)  {
        ImVector<char> includeNameVS,includeNameFS,fullPathIncludeNameVS,fullPathIncludeNameFS;vok=fok=true;
        if (pStartVertexShaderBuffer)   {
            pStartVertexShaderBuffer = ImImpl_GetIncludeFileName(pStartVertexShaderBuffer,pStartVertexShaderBuffer+vertexShaderBuffer.size()-1,includeNameVS);
            if (includeNameVS.size()>0)   {
                ImImpl_MergePathInternal(vertexShaderFilePath,&includeNameVS[0],fullPathIncludeNameVS);
                vok = ImGuiHelper::GetFileContent(&fullPathIncludeNameVS[0],prefixVertexShaderBuffer,true,"r",true);
                if (vok) {
                    pStartPrefixVertexShaderBuffer = &prefixVertexShaderBuffer[0];
                    //fprintf(stderr,"VERTEX INCLUDE CODE:\n%s\n%s\n",pStartPrefixVertexShaderBuffer,pStartVertexShaderBuffer);
                }
            }
        }
        if (pStartFragmentShaderBuffer) {
            pStartFragmentShaderBuffer = ImImpl_GetIncludeFileName(pStartFragmentShaderBuffer,pStartFragmentShaderBuffer+fragmentShaderBuffer.size()-1,includeNameFS);
            if (includeNameFS.size()>0)   {
                ImImpl_MergePathInternal(fragmentShaderFilePath,&includeNameFS[0],fullPathIncludeNameFS);
                if (fullPathIncludeNameVS.size()>0 && strcmp(&fullPathIncludeNameVS[0],&fullPathIncludeNameFS[0])==0)   {fok=vok;pStartPrefixFragmentShaderBuffer=pStartPrefixVertexShaderBuffer;}
                else {
                    fok = ImGuiHelper::GetFileContent(&fullPathIncludeNameFS[0],prefixFragmentShaderBuffer,true,"r",true);
                    if (fok) {
                        pStartPrefixFragmentShaderBuffer = &prefixFragmentShaderBuffer[0];
                        //fprintf(stderr,"FRAGMENT INCLUDE CODE:\n%s\n%s\n",pStartPrefixFragmentShaderBuffer,pStartFragmentShaderBuffer);
                    }
                }
            }
        }
        if (!vok || !fok)    {
            printf("Error in ImImpl_CompileShadersFromFile(\"%s\",\"%s\"). ",vertexShaderFilePath,fragmentShaderFilePath);
            if (!vok) printf("File included by the Vertex shader \"%s\" not found. ",&includeNameVS[0]);
            if (!fok) printf("File included by the Fragment shader \"%s\" not found. ",&includeNameFS[0]);
            fflush(stdout);
            return 0;
        }
    }

    return ImImpl_CompileShadersFromMemory(&pStartVertexShaderBuffer,&pStartFragmentShaderBuffer,pOptionalOptions,&pStartPrefixVertexShaderBuffer,&pStartPrefixFragmentShaderBuffer);
}
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
#   endif //NO_IMGUIHELPER
#endif //IMIMPL_SHADER_NONE


void InitImGuiProgram()  {
#ifndef IMIMPL_SHADER_NONE
// -----------------------------------------------------------------------
// START SHADER CODE
//------------------------------------------------------------------------
// shaders
#ifdef IMIMPL_SHADER_GL3
static const GLchar* gVertexShaderSource[] = {
#ifdef IMIMPL_SHADER_GLES
      "#version 300 es\n"
#else  //IMIMPL_SHADER_GLES
      "#version 330\n"
#endif //IMIMPL_SHADER_GLES
      "precision highp float;\n"
      "uniform mat4 ortho;\n"
      "layout (location = 0 ) in vec2 Position;\n"
      "layout (location = 1 ) in vec2 UV;\n"
      "layout (location = 2 ) in vec4 Colour;\n"
      "out vec2 Frag_UV;\n"
      "out vec4 Frag_Colour;\n"
      "void main()\n"
      "{\n"
      " Frag_UV = UV;\n"
      " Frag_Colour = Colour;\n"
      "\n"
      " gl_Position = ortho*vec4(Position.xy,0,1);\n"
      "}\n"
    };

static const GLchar* gFragmentShaderSource[] = {
#ifdef IMIMPL_SHADER_GLES
      "#version 300 es\n"
#else //IMIMPL_SHADER_GLES
      "#version 330\n"
#endif //IMIMPL_SHADER_GLES
      "precision mediump float;\n"
      "uniform lowp sampler2D Texture;\n"
      "uniform vec4 SdfParams;\n"           // it should get culled out when not used
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Colour;\n"
      "out vec4 FragColor;\n"      
      "void main()\n"
      "{\n"
#ifdef IMIMPL_USE_SDF_SHADER
       "vec4 texColor = texture(Texture, Frag_UV.st);\n"
       "float width = fwidth(texColor.a);\n"
#	ifndef IMIMPL_USE_SDF_OUTLINE_SHADER
       "float alpha = smoothstep(SdfParams.x - width, SdfParams.x + width, texColor.a);\n"
       "FragColor = vec4(Frag_Colour.rgb*texColor.rgb,Frag_Colour.a*alpha);\n"
#	else //IMIMPL_USE_SDF_OUTLINE_SHADER
    //"float outlineThreshold = 0.455;\n"              // 0.5 -> threshold for glyph border -> SdfParams.x
    //"float alphaThreshold = 0.415;\n"             // 0.425//0.2  -> threshold for glyph outline -> SdfParams.y
    //"float outlineDarkeningFactor = 0.2;\n"     // 0.3 -> SdfParams.z
    "\n"
    "float inside = smoothstep(SdfParams.x - width, SdfParams.x + width, texColor.a) ;\n"
    "vec3 insidecolor = Frag_Colour.rgb*texColor.rgb;\n"
    "vec3 outlinecolor = insidecolor.rgb*SdfParams.z;\n"
    "vec3 fragcolor = mix ( outlinecolor , insidecolor , inside ) ;\n"
    "float alpha = smoothstep(SdfParams.y - width, SdfParams.y + width, texColor.a);\n"
    "\n"
    "FragColor = vec4(fragcolor,Frag_Colour.a*alpha);\n"
#	endif //IMIMPL_USE_SDF_OUTLINE_SHADER
#elif IMIMPL_USE_ALPHA_SHARPENER_SHADER
       "vec4 texColor = texture(Texture, Frag_UV.st);\n"
       "FragColor = vec4(Frag_Colour.rgb*texColor.rgb,Frag_Colour.a*step(0.5,texColor.a));\n"
#else //IMIMPL_USE_SDF_SHADER            
      " FragColor = Frag_Colour * texture( Texture, Frag_UV.st);\n"
#endif //IMIMPL_USE_SDF_SHADER
      "}\n"
    };
#else //NO IMIMPL_SHADER_GL3
static const GLchar* gVertexShaderSource[] = {
#ifdef IMIMPL_SHADER_GLES
      "#version 100\n"
      "precision highp float;\n"
#endif //IMIMPL_SHADER_GLES
      "uniform mat4 ortho;\n"
      "attribute vec2 Position;\n"
      "attribute vec2 UV;\n"
      "attribute vec4 Colour;\n"
      "varying vec2 Frag_UV;\n"
      "varying vec4 Frag_Colour;\n"
      "void main()\n"
      "{\n"
      " Frag_UV = UV;\n"
      " Frag_Colour = Colour;\n"
      "\n"
      " gl_Position = ortho*vec4(Position.xy,0,1);\n"
      "}\n"
    };

static const GLchar* gFragmentShaderSource[] = {
#ifdef IMIMPL_SHADER_GLES
      "#version 100\n"
      "precision mediump float;\n"
      "uniform lowp sampler2D Texture;\n"
#ifdef IMIMPL_USE_SDF_SHADER
	  "#extension GL_OES_standard_derivatives : enable\n" // fwidth            
#endif //IMIMPL_USE_SDF_SHADER
#else //IMIMPL_SHADER_GLES
#   ifdef __EMSCRIPTEN__
    "precision mediump float;\n"
#		ifdef IMIMPL_USE_SDF_SHADER
	  "#extension GL_OES_standard_derivatives : enable\n" // fwidth            
#		endif //IMIMPL_USE_SDF_SHADER
#   endif //__EMSCRIPTEN__
      "uniform sampler2D Texture;\n"
#endif //IMIMPL_SHADER_GLES
      "uniform vec4 SdfParams;\n"           // it should get culled out when not used
      "varying vec2 Frag_UV;\n"
      "varying vec4 Frag_Colour;\n"
      "void main()\n"
      "{\n"
#ifdef IMIMPL_USE_SDF_SHADER   
       "vec4 texColor = texture2D(Texture, Frag_UV.st);\n"
#       if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
        "#ifdef GL_OES_standard_derivatives\n"
        "float width = fwidth(texColor.a);\n"
        "#else //GL_OES_standard_derivatives\n"
        "float width = SdfParams.w;\n"
        "#endif //GL_OES_standard_derivatives\n"
#       else // (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
       "float width = fwidth(texColor.a);\n"
#       endif //(defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
#	ifndef IMIMPL_USE_SDF_OUTLINE_SHADER
       "float alpha = smoothstep(SdfParams.x - width, SdfParams.x + width, texColor.a);\n"
       "gl_FragColor = vec4(Frag_Colour.rgb*texColor.rgb,Frag_Colour.a*alpha);\n"
#	else //IMIMPL_USE_SDF_OUTLINE_SHADER
    //"float outlineThreshold = 0.455;\n"              // 0.5 -> threshold for glyph border -> SdfParams.x
    //"float alphaThreshold = 0.415;\n"             // 0.425//0.2  -> threshold for glyph outline -> SdfParams.y
    //"float outlineDarkeningFactor = 0.2;\n"     // 0.3 -> SdfParams.z
    "\n"
    "float inside = smoothstep(SdfParams.x - width, SdfParams.x + width, texColor.a) ;\n"
    "vec3 insidecolor = Frag_Colour.rgb*texColor.rgb;\n"
    "vec3 outlinecolor = insidecolor.rgb*SdfParams.z;\n"
    "vec3 fragcolor = mix ( outlinecolor , insidecolor , inside ) ;\n"
    "float alpha = smoothstep(SdfParams.y - width, SdfParams.y + width, texColor.a);\n"
    "\n"
    "gl_FragColor = vec4(fragcolor,Frag_Colour.a*alpha);\n"
#	endif //IMIMPL_USE_SDF_OUTLINE_SHADER
#elif IMIMPL_USE_ALPHA_SHARPENER_SHADER
       "vec4 texColor = texture2D(Texture, Frag_UV.st);\n"
       "gl_FragColor = vec4(Frag_Colour.rgb*texColor.rgb,Frag_Colour.a*step(0.5,texColor.a));\n"
#else //IMIMPL_USE_SDF_SHADER            
      " gl_FragColor = Frag_Colour * texture2D( Texture, Frag_UV.st);\n"
#endif //IMIMPL_USE_SDF_SHADER
      "}\n"
    };
#endif //IMIMPL_SHADER_GL3
//------------------------------------------------------------------------
// END SHADER CODE
//------------------------------------------------------------------------

    if (gImImplPrivateParams.program==0)    {
        gImImplPrivateParams.program = ImImpl_CompileShadersFromMemory(gVertexShaderSource, gFragmentShaderSource );
        if (gImImplPrivateParams.program==0) {
            fprintf(stderr,"Error compiling shaders.\n");
            return;
        }
        //Get Uniform locations
        gImImplPrivateParams.uniLocTexture = glGetUniformLocation(gImImplPrivateParams.program,"Texture");
        gImImplPrivateParams.uniLocOrthoMatrix = glGetUniformLocation(gImImplPrivateParams.program,"ortho");
        gImImplPrivateParams.uniLocSdfParams = glGetUniformLocation(gImImplPrivateParams.program,"SdfParams");

        //Get Attribute locations
        gImImplPrivateParams.attrLocPosition  = glGetAttribLocation(gImImplPrivateParams.program,"Position");
        gImImplPrivateParams.attrLocUV  = glGetAttribLocation(gImImplPrivateParams.program,"UV");
        gImImplPrivateParams.attrLocColour  = glGetAttribLocation(gImImplPrivateParams.program,"Colour");

        // Debug

        /*printf("gUniLocTexture = %d\n",gImImplPrivateParams.uniLocTexture);
        printf("gUniLocLayers = %d\n",gImImplPrivateParams.uniLocOrthoMatrix);
        printf("uniLocSdfParams = %d\n",gImImplPrivateParams.uniLocSdfParams);
        printf("gAttrLocPosition = %d\n",gImImplPrivateParams.attrLocPosition);
        printf("gAttrLocUV = %d\n",gImImplPrivateParams.attrLocUV);
        printf("gAttrLocColour = %d\n",gImImplPrivateParams.attrLocColour);*/

    }
#endif //IMIMPL_SHADER_NONE
}

void DestroyImGuiProgram()	{
#ifndef IMIMPL_SHADER_NONE
    if (gImImplPrivateParams.program)	{
        glDeleteProgram(gImImplPrivateParams.program);
        gImImplPrivateParams.program = 0;
    }
#endif //IMIMPL_SHADER_NONE
}
void InitImGuiBuffer()	{
#ifndef IMIMPL_SHADER_NONE
    if (gImImplPrivateParams.vertexBuffers[0]==0) glGenBuffers(IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS, &gImImplPrivateParams.vertexBuffers[0]);
    if (gImImplPrivateParams.indexBuffers[0]==0) glGenBuffers(IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS, &gImImplPrivateParams.indexBuffers[0]);
#endif //IMIMPL_SHADER_NONE
}
void DestroyImGuiBuffer()	{
#ifndef IMIMPL_SHADER_NONE
    if (gImImplPrivateParams.vertexBuffers[0]) {
        glDeleteBuffers( IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS, &gImImplPrivateParams.vertexBuffers[0] );
        gImImplPrivateParams.vertexBuffers[0] = 0;
    }
    if (gImImplPrivateParams.indexBuffers[0]) {
        glDeleteBuffers( IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS, &gImImplPrivateParams.indexBuffers[0] );
        gImImplPrivateParams.indexBuffers[0] = 0;
    }
#endif //IMIMPL_SHADER_NONE
}

#ifdef IMIMPL_FORCE_DEBUG_CONTEXT
extern "C" void GLDebugMessageCallback(GLenum source, GLenum type,
    GLuint id, GLenum severity,GLsizei length, const GLchar *msg,const void *userParam)
{
    char sourceStr[32];
    const char *sourceFmt = "UNDEFINED(0x%04X)";
    switch(source)

    {
    case GL_DEBUG_SOURCE_API_ARB:             sourceFmt = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   sourceFmt = "WINDOW_SYSTEM"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: sourceFmt = "SHADER_COMPILER"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     sourceFmt = "THIRD_PARTY"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:     sourceFmt = "APPLICATION"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB:           sourceFmt = "OTHER"; break;
    }

    snprintf(sourceStr, 32, sourceFmt, source);

    char typeStr[32];
    const char *typeFmt = "UNDEFINED(0x%04X)";
    switch(type)
    {

    case GL_DEBUG_TYPE_ERROR_ARB:               typeFmt = "ERROR"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: typeFmt = "DEPRECATED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  typeFmt = "UNDEFINED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB:         typeFmt = "PORTABILITY"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:         typeFmt = "PERFORMANCE"; break;
    case GL_DEBUG_TYPE_OTHER_ARB:               typeFmt = "OTHER"; break;
    }
    snprintf(typeStr, 32, typeFmt, type);


    char severityStr[32];
    const char *severityFmt = "UNDEFINED";
    switch(severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB:   severityFmt = "HIGH";   break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB: severityFmt = "MEDIUM"; break;
    case GL_DEBUG_SEVERITY_LOW_ARB:    severityFmt = "LOW"; break;
    }

    snprintf(severityStr, 32, severityFmt, severity);

    fprintf(stderr,"OpenGL: %s [source=%s type=%s severity=%s(%d) id=%d]\n",msg, sourceStr, typeStr, severityStr,severity, id);

    if (strcmp(severityFmt,"UNDEFINED")!=0) {
        fprintf(stderr,"BREAKPOINT TRIGGERED\n");
    }
}
#endif //IMIMPL_FORCE_DEBUG_CONTEXT

const ImVec4* ImImpl_SdfShaderGetParams() {
#if (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
#ifdef IMIMPL_USE_SDF_SHADER
return &gImImplPrivateParams.sdfParams;
#endif // IMIMPL_USE_SDF_SHADER
#endif // (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
return NULL;
}
bool ImImpl_SdfShaderSetParams(const ImVec4& sdfParams) {
#if (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
#ifdef IMIMPL_USE_SDF_SHADER
gImImplPrivateParams.sdfParams = sdfParams;
return true;
#endif // IMIMPL_USE_SDF_SHADER
#endif // (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
return false;
}
bool ImImpl_EditSdfParams() {
using namespace ImGui;
bool changed = false;
#if (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
#ifdef IMIMPL_USE_SDF_SHADER
const float maxValue = 0.75f;
const float step = 0.005f;
#ifdef IMIMPL_USE_SDF_OUTLINE_SHADER
PushItemWidth(GetWindowWidth()*0.15f);
changed|=DragFloat("###ImImpl_EditSdfParams_X",&gImImplPrivateParams.sdfParams.x,step,gImImplPrivateParams.sdfParams.y,maxValue);
if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Glyph Extension");
ImGui::SameLine();
changed|=DragFloat("###ImImpl_EditSdfParams_Y",&gImImplPrivateParams.sdfParams.y,step,0.f,gImImplPrivateParams.sdfParams.x);
if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Outline Extension");
ImGui::SameLine();
changed|=DragFloat("Global Font Params###ImImpl_EditSdfParams_Z",&gImImplPrivateParams.sdfParams.z,step,0.f,maxValue);
if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Outline Darkness");
PopItemWidth();
#else //IMIMPL_USE_SDF_OUTLINE_SHADER
PushItemWidth(GetWindowWidth()*0.5f);
changed|=DragFloat("Global Font Params###ImImpl_EditSdfParams_X",&gImImplPrivateParams.sdfParams.x,step,0.f,maxValue);
if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Glyph Extension");
PopItemWidth();
#endif //IMIMPL_USE_SDF_OUTLINE_SHADER
ImGui::SameLine();
if (ImGui::SmallButton("Reset###ImImpl_EditSdfParams_R")) {gImImplPrivateParams.resetSdfParams();changed=true;}
#endif // IMIMPL_USE_SDF_SHADER
#endif // (defined(IMGUI_USE_AUTO_BINDING_OPENGL) && !defined(IMIMPL_SHADER_NONE))
return changed;
}

void ImImpl_RenderDrawLists(ImDrawData* draw_data)
{
    ImGuiIO& io = ImGui::GetIO();

#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    GLint oldViewport[4];glGetIntegerv(GL_VIEWPORT, oldViewport);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE
    const float width = io.DisplaySize.x;
    const float height = io.DisplaySize.y;
    const float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;   // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
    const float fb_width = io.DisplaySize.x * io.DisplayFramebufferScale.x;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);

#ifndef IMIMPL_SHADER_NONE
    // Setup render state: alpha-blending enabled, no face culling (or GL_FRONT face culling), no depth testing, scissor enabled
    //GLint last_texture=0;
#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT /*| GL_DEPTH_BUFFER_BIT*/);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE

    //glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glDisable(GL_CULL_FACE);
    glCullFace(GL_FRONT);       // with this I can leave GL_CULL_FACE as it is
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Setup texture
    glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, gImImplPrivateParams.fontTex);
    // Setup program and uniforms
    glUseProgram(gImImplPrivateParams.program);
    glUniform1i(gImImplPrivateParams.uniLocTexture, 0);
#   ifdef IMIMPL_USE_SDF_SHADER
    glUniform4fv(gImImplPrivateParams.uniLocSdfParams,1,&gImImplPrivateParams.sdfParams.x);
#   endif //IMIMPL_USE_SDF_SHADER

    // Setup orthographic projection matrix
    const float ortho[4][4] = {
        { 2.0f/width,   0.0f,           0.0f, 0.0f },
        { 0.0f,         2.0f/-height,   0.0f, 0.0f },
        { 0.0f,         0.0f,          -1.0f, 0.0f },
        {-1.0f,         1.0f,           0.0f, 1.0f },
    };
    glUniformMatrix4fv(gImImplPrivateParams.uniLocOrthoMatrix, 1, GL_FALSE, &ortho[0][0]);

    static int bufferNum = 0;
#   if IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS > 1
    if (++bufferNum == IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS) bufferNum = 0;
    //fprintf(stderr,"Using buffer: %d\n",bufferNum);
#   endif //IMIMPL_NUM_ROUND_ROBIN_VERTEX_BUFFERS

    IM_ASSERT(gImImplPrivateParams.vertexBuffers[bufferNum]!=0);
    IM_ASSERT(gImImplPrivateParams.indexBuffers[bufferNum]!=0);

    glBindBuffer(GL_ARRAY_BUFFER, gImImplPrivateParams.vertexBuffers[bufferNum]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gImImplPrivateParams.indexBuffers[bufferNum]);

    glEnableVertexAttribArray(gImImplPrivateParams.attrLocPosition);
    glEnableVertexAttribArray(gImImplPrivateParams.attrLocUV);
    glEnableVertexAttribArray(gImImplPrivateParams.attrLocColour);

    glVertexAttribPointer(gImImplPrivateParams.attrLocPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const void*)(0));
    glVertexAttribPointer(gImImplPrivateParams.attrLocUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (const void*)(0 + 8));
    glVertexAttribPointer(gImImplPrivateParams.attrLocColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (const void*)(0 + 16));


    gImGuiNumTextureBindingsPerFrame = 0;
    GLuint lastTex = 0,tex=0;
    glBindTexture(GL_TEXTURE_2D, lastTex);
    for (int n = 0; n < draw_data->CmdListsCount; n++)  {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

        //fprintf(stderr,"%d/%d) cmd_list->VtxBuffer.size() = %d",n+1,draw_data->CmdListsCount,cmd_list->VtxBuffer.size());
        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)    {
            if (pcmd->UserCallback) pcmd->UserCallback(cmd_list, pcmd);
            else    {
                tex = (GLuint)(intptr_t)pcmd->TextureId;
                if (tex!=lastTex)   {
                    glBindTexture(GL_TEXTURE_2D, tex);
                    lastTex = tex;
                    ++gImGuiNumTextureBindingsPerFrame;
                }
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                //fprintf(stderr,"    pcmd->ElemCount = %d    idx_buffer_offset = %d\n",pcmd->ElemCount,idx_buffer_offset);
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer_offset);
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }

    glDisableVertexAttribArray(gImImplPrivateParams.attrLocPosition);
    glDisableVertexAttribArray(gImImplPrivateParams.attrLocUV);
    glDisableVertexAttribArray(gImImplPrivateParams.attrLocColour);
    glUseProgram(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D,last_texture);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE
#else //IMIMPL_SHADER_NONE
    // We are using the OpenGL fixed pipeline to make the example code simpler to read!
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, vertex/texcoord/color pointers.
    GLint last_texture=0;
#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glDisable(GL_CULL_FACE);
    glCullFace(GL_FRONT);       // with this I can leave GL_CULL_FACE as it is
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnable(GL_TEXTURE_2D);
    //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context

    // Setup orthographic projection matrix
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Render command lists
    gImGuiNumTextureBindingsPerFrame = 0;
    GLuint lastTex = 0,tex=0;
    glBindTexture(GL_TEXTURE_2D, lastTex);
    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
        glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, pos)));
        glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, uv)));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, col)));

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                tex = (GLuint)(intptr_t)pcmd->TextureId;
                if (tex!=lastTex)   {
                    glBindTexture(GL_TEXTURE_2D, tex);
                    lastTex = tex;
                    ++gImGuiNumTextureBindingsPerFrame;
                }
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer);
            }
            idx_buffer += pcmd->ElemCount;
        }
    }
    #undef OFFSETOF

    // Restore modified state
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // Restore some main common stuff manually (for people not defining IMGUIBINDINGS_RESTORE_GL_STATE)
    glDisable(GL_BLEND);
    //glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);       // with this I can leave GL_CULL_FACE as it is
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, last_texture);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE
#endif //IMIMPL_SHADER_NONE

#   ifdef IMGUIBINDINGS_RESTORE_GL_STATE
    glViewport(oldViewport[0], oldViewport[1], (GLsizei)oldViewport[2], (GLsizei)oldViewport[3]);
#   endif //IMGUIBINDINGS_RESTORE_GL_STATE
}

#elif defined(IMGUI_USE_DIRECT3D9_BINDING)


struct CUSTOMVERTEX
{
    D3DXVECTOR3 pos;
    D3DCOLOR    col;
    D3DXVECTOR2 uv;
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;
void ImImpl_RenderDrawLists(ImDrawData* draw_data)
{
    // Create and grow buffers if needed
    if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (g_pVB) { g_pVB->Release(); g_pVB = NULL; }
        g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (g_pd3dDevice->CreateVertexBuffer(g_VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL) < 0)
            return;
    }
    if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (g_pIB) { g_pIB->Release(); g_pIB = NULL; }
        g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (g_pd3dDevice->CreateIndexBuffer(g_IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB, NULL) < 0)
            return;
    }

    // Copy and convert all vertices into a single contiguous buffer
    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    if (g_pVB->Lock(0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (void**)&vtx_dst, D3DLOCK_DISCARD) < 0)
        return;
    if (g_pIB->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (void**)&idx_dst, D3DLOCK_DISCARD) < 0)
        return;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_src = &cmd_list->VtxBuffer[0];
        for (int i = 0; i < cmd_list->VtxBuffer.size(); i++)
        {
            vtx_dst->pos.x = vtx_src->pos.x;
            vtx_dst->pos.y = vtx_src->pos.y;
            vtx_dst->pos.z = 0.0f;
            vtx_dst->col = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000)>>16) | ((vtx_src->col & 0xFF) << 16);     // RGBA --> ARGB for DirectX9
            vtx_dst->uv.x = vtx_src->uv.x;
            vtx_dst->uv.y = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, &cmd_list->IdxBuffer[0], cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
        idx_dst += cmd_list->IdxBuffer.size();
    }
    g_pVB->Unlock();
    g_pIB->Unlock();
    g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOMVERTEX ) );
    g_pd3dDevice->SetIndices( g_pIB );
    g_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetVertexShader( NULL );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, false );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, false );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, true );
    g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, false );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, true );

    //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_ADD );
    //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    //g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );

    /*g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_BLENDDIFFUSEALPHA);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);*/

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    // Setup orthographic projection matrix
    //D3DXMATRIXA16 mat;D3DXMatrixIdentity(&mat);
    D3DXMATRIX mat(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &mat );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &mat );
    D3DXMatrixOrthoOffCenterLH( &mat, 0.5f, ImGui::GetIO().DisplaySize.x+0.5f, ImGui::GetIO().DisplaySize.y+0.5f, 0.5f, -1.0f, +1.0f );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &mat );

    // Render command lists
    gImGuiNumTextureBindingsPerFrame = 0;
    LPDIRECT3DTEXTURE9 lastTex = NULL,tex=NULL;
    //g_pd3dDevice->SetTexture(0, tex);     // Not sure this work with tex == NULL
    int vtx_offset = 0;
    int idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                tex = (LPDIRECT3DTEXTURE9)pcmd->TextureId;
                if (tex!=lastTex)   {
                    g_pd3dDevice->SetTexture( 0,  tex);
                    lastTex = tex;
                    ++gImGuiNumTextureBindingsPerFrame;
                }
                const RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
                g_pd3dDevice->SetScissorRect( &r );
                g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, vtx_offset, 0, (UINT)cmd_list->VtxBuffer.size(), idx_offset, pcmd->ElemCount/3 );
            }
            idx_offset += pcmd->ElemCount;
        }
        vtx_offset += cmd_list->VtxBuffer.size();
    }
}

#endif// IMGUI_USE_DIRECT3D9_BINDING

void ImImpl_NewFramePaused()    {
    ImGuiContext& g = *GImGui;
    g.Time += g.IO.DeltaTime;

    // Update inputs state
    if (g.IO.MousePos.x < 0 && g.IO.MousePos.y < 0)
        g.IO.MousePos = ImVec2(-9999.0f, -9999.0f);
    if ((g.IO.MousePos.x < 0 && g.IO.MousePos.y < 0) || (g.IO.MousePosPrev.x < 0 && g.IO.MousePosPrev.y < 0))   // if mouse just appeared or disappeared (negative coordinate) we cancel out movement in MouseDelta
        g.IO.MouseDelta = ImVec2(0.0f, 0.0f);
    else
        g.IO.MouseDelta = g.IO.MousePos - g.IO.MousePosPrev;
    g.IO.MousePosPrev = g.IO.MousePos;
    for (int i = 0; i < IM_ARRAYSIZE(g.IO.MouseDown); i++)
    {
        g.IO.MouseClicked[i] = g.IO.MouseDown[i] && g.IO.MouseDownDuration[i] < 0.0f;
        g.IO.MouseReleased[i] = !g.IO.MouseDown[i] && g.IO.MouseDownDuration[i] >= 0.0f;
        g.IO.MouseDownDurationPrev[i] = g.IO.MouseDownDuration[i];
        g.IO.MouseDownDuration[i] = g.IO.MouseDown[i] ? (g.IO.MouseDownDuration[i] < 0.0f ? 0.0f : g.IO.MouseDownDuration[i] + g.IO.DeltaTime) : -1.0f;
        g.IO.MouseDoubleClicked[i] = false;
        if (g.IO.MouseClicked[i])
        {
            if (g.Time - g.IO.MouseClickedTime[i] < g.IO.MouseDoubleClickTime)
            {
                if (ImLengthSqr(g.IO.MousePos - g.IO.MouseClickedPos[i]) < g.IO.MouseDoubleClickMaxDist * g.IO.MouseDoubleClickMaxDist)
                    g.IO.MouseDoubleClicked[i] = true;
                g.IO.MouseClickedTime[i] = -FLT_MAX;    // so the third click isn't turned into a double-click
            }
            else
            {
                g.IO.MouseClickedTime[i] = g.Time;
            }
            g.IO.MouseClickedPos[i] = g.IO.MousePos;
            g.IO.MouseDragMaxDistanceSqr[i] = 0.0f;
        }
        else if (g.IO.MouseDown[i])
        {
            g.IO.MouseDragMaxDistanceSqr[i] = ImMax(g.IO.MouseDragMaxDistanceSqr[i], ImLengthSqr(g.IO.MousePos - g.IO.MouseClickedPos[i]));
        }
    }
    memcpy(g.IO.KeysDownDurationPrev, g.IO.KeysDownDuration, sizeof(g.IO.KeysDownDuration));
    for (int i = 0; i < IM_ARRAYSIZE(g.IO.KeysDown); i++)
        g.IO.KeysDownDuration[i] = g.IO.KeysDown[i] ? (g.IO.KeysDownDuration[i] < 0.0f ? 0.0f : g.IO.KeysDownDuration[i] + g.IO.DeltaTime) : -1.0f;

    // Calculate frame-rate for the user, as a purely luxurious feature
    g.FramerateSecPerFrameAccum += g.IO.DeltaTime - g.FramerateSecPerFrame[g.FramerateSecPerFrameIdx];
    g.FramerateSecPerFrame[g.FramerateSecPerFrameIdx] = g.IO.DeltaTime;
    g.FramerateSecPerFrameIdx = (g.FramerateSecPerFrameIdx + 1) % IM_ARRAYSIZE(g.FramerateSecPerFrame);
    g.IO.Framerate = 1.0f / (g.FramerateSecPerFrameAccum / (float)IM_ARRAYSIZE(g.FramerateSecPerFrame));

    g.IO.WantCaptureKeyboard = g.IO.WantCaptureMouse = g.IO.WantTextInput = false;


}
