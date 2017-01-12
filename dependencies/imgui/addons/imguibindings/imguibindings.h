#ifndef IMGUIBINDINGS_H_
#define IMGUIBINDINGS_H_

#ifdef IMGUI_USE_GLEW
#   if (defined(IMGUI_USE_GLAD) || defined(IMGUI_USE_GL3W))
#       error Only one between IMGUI_USE_GLEW IMGUI_USE_GLAD and IMGUI_USE_GL3W can optionally be defined
#   endif // (defined(IMGUI_USE_GLAD) || defined(IMGUI_USE_GL3W))
#elif (defined(IMGUI_USE_GLAD) && defined(IMGUI_USE_GL3W))
#   error Only one between IMGUI_USE_GLEW IMGUI_USE_GLAD and IMGUI_USE_GL3W can optionally be defined
#endif // IMGUI_USE_GLEW

#ifndef IMGUI_API
#include <imgui.h>
#endif //IMGUI_API

// include openGL headers here:
#ifdef _WIN32
#pragma warning (disable: 4996)         // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#include <windows.h>
#include <imm.h>
#endif //_WIN32

//# define GLEW_STATIC		// Optional, depending on which glew lib you want to use
#ifdef IMGUI_USE_GLEW
#   ifdef __APPLE__   // or __MACOSX__ ?
#       include <OpenGL/glew.h>     // guessing...
#   else //__APPLE
#       include <GL/glew.h>
#   endif //__APPLE
#elif IMGUI_USE_GLAD
#   include <glad/glad.h>
#elif IMGUI_USE_GL3W
#   include <GL/gl3w.h>
#else //IMGUI_USE_GLEW
#   define GL_GLEXT_PROTOTYPES
#endif //IMGUI_USE_GLEW


#if (!defined(IMGUI_USE_WINAPI_BINDING) && !defined(IMGUI_USE_GLFW_BINDING) && !defined(IMGUI_USE_SDL2_BINDING) && !defined(IMGUI_USE_GLUT_BINDING))
#   ifdef _WIN32
#       define IMGUI_USE_WINAPI_BINDING
#   else //_WIN32
#       define IMGUI_USE_GLFW_BINDING
#   endif //_WIN32
#endif // !defined(...)

#ifdef IMGUI_USE_GLUT_BINDING
//-------------------------------------------------------------------------------
#   ifdef __APPLE__   // or __MACOSX__ ?
#       include <OpenGL/glut.h>             // guessing...
#   else //__APPLE
#       include <GL/glut.h>
#   endif //__APPLE
#ifndef __EMSCRIPTEN__
#   ifdef __FREEGLUT_STD_H__
#       ifdef __APPLE__   // or __MACOSX__ ?
#           include <OpenGL/freeglut_ext.h>     // guessing...
#       else //__APPLE
#           include <GL/freeglut_ext.h>
#       endif //__APPLE
#   endif //__FREEGLUT_STD_H__
#endif //__EMSCRIPTEN__
#ifdef _WIN32
typedef char GLchar;    // Is this needed for all GL bindings ?
#endif // _WIN32
//-------------------------------------------------------------------------------
#elif IMGUI_USE_SDL2_BINDING
//-------------------------------------------------------------------------------
#   if (!defined(IMGUI_USE_GLEW) && !defined(IMGUI_USE_GLAD) && !defined(IMGUI_USE_GL3W))
#       if (!defined(IMIMPL_SHADER_GLES) || defined (IMIMPL_SHADER_NONE))
#           include <SDL2/SDL_opengl.h>
#       else //IMIMPL_SHADER_GLES
#           include <SDL2/SDL_opengles2.h>
#       endif //IMIMPL_SHADER_GLES
#   endif //(!defined(IMGUI_USE_GLEW) && !defined(IMGUI_USE_GLAD) && !defined(IMGUI_USE_GL3W))
#   include <SDL2/SDL.h>
#ifdef _WIN32
typedef char GLchar;    // Is this needed for all GL bindings ?
#endif // _WIN32
//-------------------------------------------------------------------------------
#elif IMGUI_USE_GLFW_BINDING
//-------------------------------------------------------------------------------
//# define GLFW_STATIC   //// Optional, depending on which glfw lib you want to use
#   include <GLFW/glfw3.h>
#   if (GLFW_VERSION_MAJOR<3)
#       error GLFW_VERSION_MAJOR < 3 is not supported
#   endif //GLFW_VERSION_MAJOR<3
#   ifdef _WIN32
#       define GLFW_EXPOSE_NATIVE_WIN32
#       define GLFW_EXPOSE_NATIVE_WGL
#       include <GLFW/glfw3native.h>    // glfwGetWin32Window(...) used by ImImpl_ImeSetInputScreenPosFn(...)
#   endif //_WIN32
#ifdef _WIN32
typedef char GLchar;    // Is this needed for all GL bindings ?
#endif // _WIN32
//-------------------------------------------------------------------------------
#elif (defined(IMGUI_USE_DIRECT3D9_BINDING))
//-------------------------------------------------------------------------------
#include <d3dx9.h>
//#define DIRECTINPUT_VERSION 0x0800
//#include <dinput.h>
//-------------------------------------------------------------------------------
#elif (defined(_WIN32) || defined(IMGUI_USE_WINAPI_BINDING))
//-------------------------------------------------------------------------------
#   if (!defined(IMGUI_USE_GLEW) && !defined(IMGUI_USE_GLAD) && !defined(IMGUI_USE_GL3W))
//      I've never managed to make this branch work => when using Windows, ALWAYS use glew (on Linux it's much easier)
#       define GL_GLEXT_PROTOTYPES
#       ifdef __APPLE__   // or __MACOSX__ ?
#           include <OpenGL/glext.h>     // guessing...
#       else //__APPLE
#           include <GL/glext.h>
#       endif //__APPLE
#   endif //(!defined(IMGUI_USE_GLEW) && !defined(IMGUI_USE_GLAD) && !defined(IMGUI_USE_GL3W))
#   ifdef __APPLE__   // or __MACOSX__ ?
#       include <OpenGL/gl.h>       // guessing...
#   else //__APPLE
#       include <GL/gl.h>
#   endif //__APPLE
//--------------------------------------------------------------------------------
#else // IMGUI_USE_SOME_BINDING
#error: No IMGUI_USE_XXX_BINDING defined
#include "./addons/imguibindings/ImImpl_Binding_Glfw3.h"
#endif // IMGUI_USE_SOME_BINDING

#include <string.h>
#include <stdio.h>

extern void InitGL();
extern void ResizeGL(int w,int h);
extern void DrawGL();
extern void DestroyGL();

// These variables can be declared extern and set at runtime-----------------------------------------------------
extern bool gImGuiPaused;// = false;
extern float gImGuiInverseFPSClampInsideImGui;// = -1.0f;    // CAN'T BE 0. < 0 = No clamping.
extern float gImGuiInverseFPSClampOutsideImGui;// = -1.0f;    // CAN'T BE 0. < 0 = No clamping.
extern bool gImGuiDynamicFPSInsideImGui;                   // Dynamic FPS inside ImGui: from 5 to gImGuiInverseFPSClampInsideImGui
extern bool gImGuiCapturesInput;             // When false the input events can be directed somewhere else
extern bool gImGuiWereOutsideImGui;
extern bool gImGuiBindingMouseDblClicked[5];
extern bool gImGuiFunctionKeyDown[12];
extern bool gImGuiFunctionKeyPressed[12];
extern bool gImGuiFunctionKeyReleased[12];
extern int gImGuiNumTextureBindingsPerFrame;    // read-only
typedef void (*ImImplVoidDelegate)();
extern ImImplVoidDelegate gImGuiPostInitGLCallback;
extern ImImplVoidDelegate gImGuiPreDrawGLCallback;
extern ImImplVoidDelegate gImGuiPreDrawGLSwapBuffersCallback;
extern ImImplVoidDelegate gImGuiPostDrawGLSwapBuffersCallback;
// --------------------------------------------------------------------------------------------------------------

struct ImImpl_InitParams	{
    friend struct FontData;
    // Holds info for physic or memory file (used to load TTF)
    struct FontData {
        char filePath[2048];
        const unsigned char* pMemoryData;
        size_t memoryDataSize;
        enum Compression {
            COMP_NONE=0
#ifdef      YES_IMGUISTRINGIFIER
            ,COMP_BASE64
            ,COMP_BASE85
#endif      //YES_IMGUISTRINGIFIER
            ,COMP_STB
            ,COMP_STBBASE85
#if         (!defined(NO_IMGUIHELPER) && defined(IMGUI_USE_ZLIB))
            ,COMP_GZ
#           ifdef   YES_IMGUISTRINGIFIER
            ,COMP_GZBASE64
            ,COMP_GZBASE85
#           endif   //YES_IMGUISTRINGIFIER
#           endif   //IMGUI_USE_ZLIB
#if         (defined(YES_IMGUIBZ2))
            ,COMP_BZ2
#   ifdef   YES_IMGUISTRINGIFIER
            ,COMP_BZ2BASE64
            ,COMP_BZ2BASE85
#   endif   //YES_IMGUISTRINGIFIER
#           endif   //YES_IMGUIBZ2
        };
        Compression memoryDataCompression;
        float sizeInPixels;//=15.0f,
        const ImWchar* pGlyphRanges;
        ImFontConfig fontConfig;
        bool useFontConfig;

        FontData(const unsigned char* _pMemoryData,size_t _memoryDataFile,Compression _memoryDataCompression=COMP_NONE,
                 float _sizeInPixels=15.f,const ImWchar* pOptionalGlyphRanges=NULL,ImFontConfig* pOptionalFontConfig=NULL)
        : pMemoryData(_pMemoryData) , memoryDataSize(_memoryDataFile), memoryDataCompression(_memoryDataCompression),
        sizeInPixels(_sizeInPixels),pGlyphRanges(pOptionalGlyphRanges?pOptionalGlyphRanges:ImImpl_InitParams::GetGlyphRangesDefault())
        ,useFontConfig(false)
        {IM_ASSERT(pMemoryData);IM_ASSERT(_memoryDataFile);
        //IM_ASSERT(sizeInPixels>0);
        filePath[0]='\0';
        if (pOptionalFontConfig) {useFontConfig=true;fontConfig=*pOptionalFontConfig;}}
        FontData(const char* _filePath,float _sizeInPixels=15.f,const ImWchar* pOptionalGlyphRanges=NULL,ImFontConfig* pOptionalFontConfig=NULL)
        : pMemoryData(NULL) , memoryDataSize(0),sizeInPixels(_sizeInPixels),pGlyphRanges(pOptionalGlyphRanges?pOptionalGlyphRanges:ImImpl_InitParams::GetGlyphRangesDefault())
        ,useFontConfig(false)
        {IM_ASSERT(_filePath && strlen(_filePath)>0);
         const size_t len = strlen(_filePath);IM_ASSERT(len>0 && len<2047);
         if (len<2047) strcpy(filePath,_filePath);//IM_ASSERT(sizeInPixels>0);
         if (pOptionalFontConfig) {useFontConfig=true;fontConfig=*pOptionalFontConfig;}}
    };
	ImVec2 gWindowSize;
	char gWindowTitle[1024];
    float gFpsClampInsideImGui;	// <0 -> no clamp
    float gFpsClampOutsideImGui;	// <0 -> no clamp
    bool gFpsDynamicInsideImGui;    // false
    ImVector<FontData> fonts;
    bool forceAddDefaultFontAsFirstFont;
    bool skipBuildingFonts;
    ImImpl_InitParams(
            int windowWidth=1270,
            int windowHeight=720,
            const char* windowTitle=NULL,
            const char* OptionalTTFFilePath=NULL,
            const unsigned char*    _pOptionalReferenceToTTFFileInMemory=NULL,
            size_t                  _pOptionalSizeOfTTFFileInMemory=0,
            const float OptionalTTFFontSizeInPixels=15.0f,
            const ImWchar* OptionalTTFGlyphRanges=NULL,
            ImFontConfig* pFontConfig=NULL,
            bool _forceAddDefaultFontAsFirstFont = false,
            bool _skipBuildingFonts = false
    ) :
    gFpsClampInsideImGui(-1.0f),
    gFpsClampOutsideImGui(-1.0f),
    gFpsDynamicInsideImGui(false),
    forceAddDefaultFontAsFirstFont(_forceAddDefaultFontAsFirstFont),
    skipBuildingFonts(_skipBuildingFonts)
	{
        gWindowSize.x = windowWidth<=0?1270:windowWidth;gWindowSize.y = windowHeight<=0?720:windowHeight;

		gWindowTitle[0]='\0';
		if (windowTitle)	{
			const size_t len = strlen(windowTitle);
			if (len<1023) strcat(gWindowTitle,windowTitle);
			else		  {
				memcpy(gWindowTitle,windowTitle,1023);
				gWindowTitle[1023]='\0';
			}
		}
		else strcat(gWindowTitle,"ImGui OpenGL Example");

        if (OptionalTTFFilePath && strlen(OptionalTTFFilePath)>0)
            fonts.push_back(FontData(OptionalTTFFilePath,OptionalTTFFontSizeInPixels,OptionalTTFGlyphRanges,pFontConfig));
        else if (_pOptionalReferenceToTTFFileInMemory && _pOptionalSizeOfTTFFileInMemory>0)
            fonts.push_back(FontData(_pOptionalReferenceToTTFFileInMemory,_pOptionalSizeOfTTFFileInMemory,FontData::COMP_NONE,OptionalTTFFontSizeInPixels,OptionalTTFGlyphRanges,pFontConfig));
	}
    ImImpl_InitParams(
            int windowWidth,
            int windowHeight,
            const char* windowTitle,
            const ImVector<ImImpl_InitParams::FontData> & _fonts,
            bool _forceAddDefaultFontAsFirstFont = false
    ) :
    gFpsClampInsideImGui(-1.0f),
    gFpsClampOutsideImGui(-1.0f),
    gFpsDynamicInsideImGui(false),
    //fonts(_fonts),    // Hehe: this crashes the program on exit (I guess ImVector can't handle operator= correctly)
    forceAddDefaultFontAsFirstFont(_forceAddDefaultFontAsFirstFont),
    skipBuildingFonts(false)
    {
        gWindowSize.x = windowWidth<=0?1270:windowWidth;gWindowSize.y = windowHeight<=0?720:windowHeight;
        fonts.reserve(_fonts.size());for (int i=0,isz=_fonts.size();i<isz;i++) fonts.push_back(_fonts[i]);  // Manual workaround that works

        gWindowTitle[0]='\0';
        if (windowTitle)	{
            const size_t len = strlen(windowTitle);
            if (len<1023) strcat(gWindowTitle,windowTitle);
            else		  {
                memcpy(gWindowTitle,windowTitle,1023);
                gWindowTitle[1023]='\0';
            }
        }
        else strcat(gWindowTitle,"ImGui OpenGL Example");
    }

    private:
    // Retrieve list of range (2 int per range, values are inclusive)
    static const ImWchar*   GetGlyphRangesDefault()
    {
        static const ImWchar ranges[] =
        {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x20AC, 0x20AC,	// €
            0x2122, 0x2122,	// ™
            0x263A, 0x263A, // ☺
            0x266A, 0x266A, // ♪
            0,
        };
        return &ranges[0];
    }
};

#ifdef IMGUI_USE_AUTO_BINDING_WINDOWS
extern int ImImpl_WinMain(const ImImpl_InitParams* pOptionalInitParams,HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int iCmdShow);
#else //IMGUI_USE_WINAPI_BINDING
extern int ImImpl_Main(const ImImpl_InitParams* pOptionalInitParams=NULL,int argc=0, char** argv=NULL);
#endif //IMGUI_USE_WINAPI_BINDING


extern void InitImGuiFontTexture(const ImImpl_InitParams* pOptionalInitParams=NULL);
extern void DestroyImGuiFontTexture();

#ifdef IMGUI_USE_AUTO_BINDING_OPENGL
extern void InitImGuiProgram();
extern void DestroyImGuiProgram();

extern void InitImGuiBuffer();
extern void DestroyImGuiBuffer();
#endif //IMGUI_USE_AUTO_BINDING_OPENGL
extern void ImImpl_RenderDrawLists(ImDrawData* draw_data);


extern void WaitFor(unsigned int ms);
extern void ImImpl_FreeTexture(ImTextureID& imtexid);
extern void ImImpl_GenerateOrUpdateTexture(ImTextureID& imtexid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
extern void ImImpl_ClearColorBuffer(const ImVec4& bgColor=ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

extern void ImImpl_FlipTexturesVerticallyOnLoad(bool flag_true_if_should_flip);
extern ImTextureID ImImpl_LoadTexture(const char* filename,int req_comp=0,bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);
extern ImTextureID ImImpl_LoadTextureFromMemory(const unsigned char* filenameInMemory, int filenameInMemorySize, int req_comp=0, bool useMipmapsIfPossible=false,bool wraps=true,bool wrapt=true);

#ifdef IMGUI_USE_AUTO_BINDING_OPENGL

#ifndef IMIMPL_SHADER_NONE
class ImImpl_CompileShaderStruct {
public:
    GLuint vertexShaderOverride,fragmentShaderOverride; // when !=0, the shader codes will be ignored, and these will be used.
    GLuint programOverride;             // when !=0, the shaders will be ignored completely.
    bool dontLinkProgram;               // when "true", shaders are attached to the program, but not linked, the program is returned to "programOverride" and the shaders are returned to "vertexShaderOverride" and "fragmentShaderOverride" if "dontDeleteAttachedShaders" is true.
                                        // however, when "false" and linking is performed, then the shaders are detached and deleted IN ANY CASE.
    bool dontDeleteAttachedShaders;     // After the program is linked, by default all the attached shaders are deleted IN ANY CASE.

#ifndef NO_IMGUISTRING
protected:
    ImStringImStringMap mPreprocessorDefinitionsWithValue;
    ImVectorEx < ImString > mPreprocessorDefinitions; // Without values (don't remember why I split them here)
    int mNumPreprocessorAdditionalLines;
    ImString mPreprocessorAdditionalShaderCode;

    ImVectorEx<ImPair<ImString,GLuint> > mBindAttributeMap;
    bool mBindAttributes;
public:
    // This interface allows prepending the shader source with runtime definition (e.g. #define MY_VAR 12 [untested], or just #define MY_VAR [tested]):
    void addPreprocessorDefinition(const ImString& name,const ImString& value="");  // TODO: [Untested value!=""]
    void removePreprocessorDefinition(const ImString& name);
    void updatePreprocessorDefinitions();   // mandatory after a set of add/remove calls to generate an usable "mPreprocessorAdditionalShaderCode"
    inline const char* getPreprocessorDefinitionAdditionalCode() const {return mPreprocessorAdditionalShaderCode.c_str();}
    inline int getNumPreprocessorDefinitionAdditionalLines() const {return mNumPreprocessorAdditionalLines;}
    void resetPreprocessorDefinitions();

    // This interface allows binding attribute locations before linking the program:
    void bindAttributeLocation(const ImString& attribute,GLuint bindingLocation);
    void resetBindAttributeLocations();
    inline void processBindAttributeLocationsOn(GLuint program) const;

    void reset() {resetShaderOptions();resetPreprocessorDefinitions();resetBindAttributeLocations();}
#else  //NO_IMGUISTRING
public:
    void reset()  {resetShaderOptions();}
#endif //NO_IMGUISTRING
public:
    void resetShaderOptions() {vertexShaderOverride = fragmentShaderOverride = programOverride = 0;dontLinkProgram = dontDeleteAttachedShaders = false;}
    ImImpl_CompileShaderStruct() {reset();}
};
// returns the shader program ID. "optionalShaderCodePrefix" (if present) is just copied before the source of both shaders. "pOptionalOptions" can be used to tune dynamic definitions inside the shader code and some shader compilation and linking processing steps.
extern GLuint ImImpl_CompileShadersFromMemory(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource,ImImpl_CompileShaderStruct* pOptionalOptions=NULL, const GLchar** optionalVertexShaderCodePrefix=NULL, const GLchar** optionalFragmentShaderCodePrefix=NULL);
#   ifndef NO_IMGUIHELPER_SERIALIZATION
#       ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
// returns the shader program ID. "pOptionalOptions" can be used to tune dynamic definitions inside the shader code and some shader compilation and linking processing steps.
extern GLuint ImImpl_CompileShadersFromFile(const char* vertexShaderFilePath, const char* fragmentShaderFilePath,ImImpl_CompileShaderStruct* pOptionalOptions=NULL, bool allowProcessingASingleIncludeDirectivePlacedAtTheFirstLineOfTheShaderCode=false);
#       endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#   endif //NO_IMGUIHELPER_SERIALIZATION
#endif //IMIMPL_SHADER_NONE

#ifdef IMIMPL_FORCE_DEBUG_CONTEXT
extern "C" void GLDebugMessageCallback(GLenum source, GLenum type,
    GLuint id, GLenum severity,GLsizei length, const GLchar *msg,const void *userParam);
#endif //IMIMPL_FORCE_DEBUG_CONTEXT

#endif //IMGUI_USE_AUTO_BINDING_OPENGL

extern void ImImpl_NewFramePaused();

// Returns NULL in all cases where Sdf fonts are not used or built (always check!)
// Default is (0.5,0.4,0.25,0.04). All numbers in [0,1].
// When no outline shader is used: x -> shrinks/grows the font.
// otherwise: (x-y) controls the outline width, and shifting both values shrinks/grows the font.
// w -> this is just a fallback value to set fwidth when standard derivates are not supported.
extern const ImVec4* ImImpl_SdfShaderGetParams();
extern bool ImImpl_SdfShaderSetParams(const ImVec4& sdfParams);
extern bool ImImpl_EditSdfParams();

#endif //IMGUIBINDINGS_H_

