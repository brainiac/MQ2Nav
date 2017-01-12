#ifndef IMIMPL_BINDING_H
#define IMIMPL_BINDING_H


#include "imguibindings.h"

#if (GLFW_VERSION_MAJOR>=3 && GLFW_VERSION_MINOR>0)
#define GLFW_HAS_MOUSE_CURSOR_SUPPORT
#endif //GLFW_VERSION

#if (defined(GLFW_HAS_MOUSE_CURSOR_SUPPORT) && defined(IMGUI_GLFW_NO_NATIVE_CURSORS))
#undef IMGUI_GLFW_NO_NATIVE_CURSORS
#warning IMGUI_GLFW_NO_NATIVE_CURSORS must be undefined globally, because your GLFW version is >= 3.1 and has embedded mouse cursor support.
#endif //IMGUI_GLFW_NO_NATIVE_CURSORS

#ifdef GLFW_HAS_MOUSE_CURSOR_SUPPORT
    static const int glfwCursorIds[ImGuiMouseCursor_Count_+1] = {
        GLFW_ARROW_CURSOR,
        GLFW_IBEAM_CURSOR,
        GLFW_HAND_CURSOR,      //SDL_SYSTEM_CURSOR_HAND,    // or SDL_SYSTEM_CURSOR_SIZEALL  //ImGuiMouseCursor_Move,                  // Unused by ImGui
        GLFW_VRESIZE_CURSOR,       //ImGuiMouseCursor_ResizeNS,              // Unused by ImGui
        GLFW_HRESIZE_CURSOR,       //ImGuiMouseCursor_ResizeEW,              // Unused by ImGui
        GLFW_CROSSHAIR_CURSOR,     //ImGuiMouseCursor_ResizeNESW,
        GLFW_CROSSHAIR_CURSOR,     //ImGuiMouseCursor_ResizeNWSE,          // Unused by ImGui
        GLFW_ARROW_CURSOR         //,ImGuiMouseCursor_Arrow
    };
    static GLFWcursor* glfwCursors[ImGuiMouseCursor_Count_+1];

#else //GLFW_HAS_MOUSE_CURSOR_SUPPORT
#ifndef IMGUI_GLFW_NO_NATIVE_CURSORS
#   ifdef _WIN32
#       define IMGUI_USE_WIN32_CURSORS     // Optional, but needs at window creation: wc.hCursor = LoadCursor( NULL, NULL); // Now the window class is inside glfw3... Not sure how I can access it...
#       ifdef IMGUI_USE_WIN32_CURSORS
    static const LPCTSTR win32CursorIds[ImGuiMouseCursor_Count_+1] = {
        IDC_ARROW,
        IDC_IBEAM,
        IDC_SIZEALL,      //SDL_SYSTEM_CURSOR_HAND,    // or SDL_SYSTEM_CURSOR_SIZEALL  //ImGuiMouseCursor_Move,                  // Unused by ImGui
        IDC_SIZENS,       //ImGuiMouseCursor_ResizeNS,              // Unused by ImGui
        IDC_SIZEWE,       //ImGuiMouseCursor_ResizeEW,              // Unused by ImGui
        IDC_SIZENESW,     //ImGuiMouseCursor_ResizeNESW,
        IDC_SIZENWSE,     //ImGuiMouseCursor_ResizeNWSE,          // Unused by ImGui
        IDC_ARROW         //,ImGuiMouseCursor_Arrow
    };
    static HCURSOR win32Cursors[ImGuiMouseCursor_Count_+1];
#       endif //IMGUI_USE_WIN32_CURSORS
#   else //_WIN32
#       define IMGUI_USE_X11_CURSORS      // Optional (feel free to comment it out if you don't have X11)
#       ifdef IMGUI_USE_X11_CURSORS
    // Before the inclusion of @ref glfw3native.h, you must define exactly one
    // window API macro and exactly one context API macro.
#           define GLFW_EXPOSE_NATIVE_X11
#           define GLFW_EXPOSE_NATIVE_GLX
#           include <GLFW/glfw3native.h>        // GLFWAPI Display* glfwGetX11Display(void); //GLFWAPI Window glfwGetX11Window(GLFWwindow* window);
#           include <X11/cursorfont.h>
    // 52 (closed hand)   58 (hand with forefinger) 124 (spray)   86  (pencil)  150 (wait)
    static const unsigned int x11CursorIds[ImGuiMouseCursor_Count_+1] = {
        2,//SDL_SYSTEM_CURSOR_ARROW,
        152,//SDL_SYSTEM_CURSOR_IBEAM,
        30,//SDL_SYSTEM_CURSOR_SIZEALL,      //SDL_SYSTEM_CURSOR_HAND,    // or SDL_SYSTEM_CURSOR_SIZEALL  //ImGuiMouseCursor_Move,                  // Unused by ImGui
        116,//42,//SDL_SYSTEM_CURSOR_SIZENS,       //ImGuiMouseCursor_ResizeNS,              // Unused by ImGui
        108,//SDL_SYSTEM_CURSOR_SIZEWE,       //ImGuiMouseCursor_ResizeEW,              // Unused by ImGui
        13,//SDL_SYSTEM_CURSOR_SIZENESW,     //ImGuiMouseCursor_ResizeNESW,
        15,//SDL_SYSTEM_CURSOR_SIZENWSE,     //ImGuiMouseCursor_ResizeNWSE,          // Unused by ImGui
        2//SDL_SYSTEM_CURSOR_ARROW         //,ImGuiMouseCursor_Arrow
    };
    static Cursor x11Cursors[ImGuiMouseCursor_Count_+1];
#       endif //IMGUI_USE_X11_CURSORS
#   endif //_WIN32
#endif //IMGUI_GLFW_NO_NATIVE_CURSORS
#endif //GLFW_HAS_MOUSE_CURSOR_SUPPORT




//static
GLFWwindow* window;
//static bool mousePressed[2] = { false, false };
static ImVec2 mousePosScale(1.0f, 1.0f);


// NB: ImGui already provide OS clipboard support for Windows so this isn't needed if you are using Windows only.
static const char* ImImpl_GetClipboardTextFn(void*)
{
    return glfwGetClipboardString(window);
}

static void ImImpl_SetClipboardTextFn(void*,const char* text)
{
    glfwSetClipboardString(window, text);
}

#ifdef _WIN32
// Notify OS Input Method Editor of text input position (e.g. when using Japanese/Chinese inputs, otherwise this isn't needed)
static void ImImpl_ImeSetInputScreenPosFn(int x, int y)
{
    HWND hwnd = glfwGetWin32Window(window);
    if (HIMC himc = ImmGetContext(hwnd))
    {
        COMPOSITIONFORM cf;
        cf.ptCurrentPos.x = x;
        cf.ptCurrentPos.y = y;
        cf.dwStyle = CFS_FORCE_POSITION;
        ImmSetCompositionWindow(himc, &cf);
    }
}
#endif


// GLFW callbacks to get events
static void glfw_error_callback(int /*error*/, const char* description)	{
    fputs(description, stderr);
}
static bool gImGuiAppIsIconified = false;
static void glfw_window_iconify_callback(GLFWwindow* /*window*/,int iconified)    {
    gImGuiAppIsIconified = iconified == GL_TRUE;
}
static void glfw_framebuffer_size_callback(GLFWwindow* /*window*/,int fb_w,int fb_h)  {
    int w, h;glfwGetWindowSize(window, &w, &h);
    mousePosScale.x = (float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = (float)fb_h / h;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);
}
static void glfw_window_size_callback(GLFWwindow* /*window*/,int w,int h)  {
    int fb_w, fb_h;glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = (float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = (float)fb_h / h;
    ResizeGL(w,h);
}
static void glfw_mouse_button_callback(GLFWwindow* /*window*/, int button, int action, int mods)	{
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && button < 5) {
        io.MouseDown[button] = (action == GLFW_PRESS);
        // Manual double click handling:
        static double dblClickTimes[6]={-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};  // seconds
        if (action == GLFW_PRESS)   {
            double time = glfwGetTime();
            double& oldTime = dblClickTimes[button];
            bool& mouseDoubleClicked = gImGuiBindingMouseDblClicked[button];
            if (time - oldTime < io.MouseDoubleClickTime) {
                mouseDoubleClicked = true;
                oldTime = -FLT_MAX;
                //fprintf(stderr,"Double Clicked button %d\n",button);
            }
            else {
                //fprintf(stderr,"Not Double Clicked button %d (%1.4f < %1.4f)\n",button,(float)time-(float)oldTime,io.MouseDoubleClickTime);
                mouseDoubleClicked = false;
                oldTime = time;
            }
        }
    }
    io.KeyCtrl = (mods & GLFW_MOD_CONTROL);
    io.KeyShift = (mods & GLFW_MOD_SHIFT);
    io.KeyAlt = (mods & GLFW_MOD_ALT);

}
static void glfw_scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)	{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel = (yoffset != 0.0f) ? yoffset > 0.0f ? 1 : - 1 : 0;           // Mouse wheel: -1,0,+1
}
static bool gImGuiCapsLockDown = false;
static void glfw_key_callback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int mods)	{
    ImGuiIO& io = ImGui::GetIO();
    io.KeyCtrl = (mods & GLFW_MOD_CONTROL);
    io.KeyShift = (mods & GLFW_MOD_SHIFT);
    io.KeyAlt = (mods & GLFW_MOD_ALT);
    const bool down = (action!=GLFW_RELEASE);
    // (action == GLFW_PRESS);
    if (key==GLFW_KEY_LEFT_CONTROL || key==GLFW_KEY_RIGHT_CONTROL)  io.KeyCtrl = down;
    else if (key==GLFW_KEY_LEFT_SHIFT || key==GLFW_KEY_RIGHT_SHIFT) io.KeyShift = down;
    else if (key==GLFW_KEY_LEFT_ALT || key==GLFW_KEY_RIGHT_ALT)     io.KeyAlt = down;
    if (key == GLFW_KEY_CAPS_LOCK) gImGuiCapsLockDown = down;
    else if (key>=GLFW_KEY_A && key<=GLFW_KEY_Z && !io.KeyShift && !gImGuiCapsLockDown) {
        if (!(io.KeyCtrl && (key==GLFW_KEY_X || key==GLFW_KEY_C || key==GLFW_KEY_V ||
            key==GLFW_KEY_A || key==GLFW_KEY_Y || key==GLFW_KEY_Z)))    // Preserve copy/paste etc.
                key+= ((const int)'a'-GLFW_KEY_A);
    }
    if (key>=GLFW_KEY_F1 && key<=GLFW_KEY_F12) {
        const int i = key-GLFW_KEY_F1;
        const bool prevState = gImGuiFunctionKeyDown[i];
        gImGuiFunctionKeyDown[i] = down;
        if (down!=prevState)    {
            if (down) gImGuiFunctionKeyPressed[i] = true;
            else gImGuiFunctionKeyReleased[i] = true;
        }
        //fprintf(stderr,"%d) D:%d P:%d R:%d\n",i,(int)gImGuiFunctionKeyDown[i],(int)gImGuiFunctionKeyPressed[i],(int)gImGuiFunctionKeyReleased[i]);
    }
    else if (key>=0 && key<512)  io.KeysDown[key] = down;
}
static void glfw_char_callback(GLFWwindow* /*window*/, unsigned int c)	{
    if (c > 0 && c < 0x10000 && !ImGui::GetIO().KeyCtrl) ImGui::GetIO().AddInputCharacter((unsigned short)c);
}
static void glfw_mouse_enter_leave_callback(GLFWwindow* /*window*/, int entered)	{
    if (entered==GL_FALSE) {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos.x=io.MousePos.y=-1.f;
    }
}
static void glfw_mouse_move_callback(GLFWwindow* /*window*/, double x,double y)	{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2((float)x * mousePosScale.x, (float)y * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
}
static void InitImGui(const ImImpl_InitParams* pOptionalInitParams=NULL)	{
    int w, h;
    int fb_w, fb_h;
    glfwGetWindowSize(window, &w, &h);
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = (float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = (float)fb_h / h;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);  // Display size, in pixels. For clamping windows positions.
    io.DeltaTime = 1.0f/60.0f;                          // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    //io.PixelCenterOffset = 0.0f;                        // Align OpenGL texels
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;             // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
    io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;
#ifdef _WIN32
    io.ImeSetInputScreenPosFn = ImImpl_ImeSetInputScreenPosFn;
#endif

    // 3 common init steps
    InitImGuiFontTexture(pOptionalInitParams);
    InitImGuiProgram();
    InitImGuiBuffer();

}


static bool InitBinding(const ImImpl_InitParams* pOptionalInitParams=NULL,int argc=0, char** argv=NULL)	{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())    {
        fprintf(stderr, "Could not call glfwInit(...) successfully.\n");
        return false;
    }

//-ENABLE-OPENGLES COMPATIBILITY PROFILES----------------------------------
#ifndef IMIMPL_SHADER_NONE
#ifdef IMIMPL_SHADER_GLES
#   ifndef IMIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);          // 1 => ES1.1   2 => ES2.0  3 => ES3.0
#   else //IMIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#   endif //MIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif // IMIMPL_SHADER_GLES
#endif //IMIMPL_SHADER_NONE
//--------------------------------------------------------------------------
    //glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);

#ifdef IMIMPL_FORCE_DEBUG_CONTEXT
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,GL_TRUE);
#endif //IMIMPL_FORCE_DEBUG_CONTEXT

/*
//-ENABLE-OPENGLES COMPATIBILITY PROFILES----------------------------------
#ifndef IMIMPL_SHADER_NONE
#ifdef IMIMPL_SHADER_GLES
#   ifndef IMIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);          // 1 => ES1.1   2 => ES2.0  3 => ES3.0
#   ifdef IMIMPL_SHADER_GLES_MINOR_VERSION
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, IMIMPL_SHADER_GLES_MINOR_VERSION);
#   endif //IMIMPL_SHADER_GLES_MINOR_VERSION
#   else //IMIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#   ifdef IMIMPL_SHADER_GLES_MINOR_VERSION
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, IMIMPL_SHADER_GLES_MINOR_VERSION);
#   endif //IMIMPL_SHADER_GLES_MINOR_VERSION
#   endif //MIMPL_SHADER_GL3
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif // IMIMPL_SHADER_GLES
#endif //IMIMPL_SHADER_NONE

#ifndef IMIMPL_SHADER_GLES
#ifdef IMIMPL_FORWARD_COMPAT
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
#ifdef IMIMPL_CORE_PROFILE
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#endif
#endif

#ifdef IMIMPL_SRGB_CAPABLE
    glfwWindowHint(GLFW_SRGB_CAPABLE,GL_TRUE);
#endif
if (pOptionalInitParams && pOptionalInitParams->useOpenGLDebugContext) glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,GL_TRUE);
//--------------------------------------------------------------------------
*/

    if (pOptionalInitParams && pOptionalInitParams->gWindowTitle[0]!='\0')  window = glfwCreateWindow(pOptionalInitParams ? pOptionalInitParams->gWindowSize.x : 1270, pOptionalInitParams ? pOptionalInitParams->gWindowSize.y : 720,(const char*) &pOptionalInitParams->gWindowTitle[0], NULL, NULL);
    else		window = glfwCreateWindow(pOptionalInitParams ? pOptionalInitParams->gWindowSize.x : 1270, pOptionalInitParams ? pOptionalInitParams->gWindowSize.y : 720, "ImGui Glfw3 OpenGL example", NULL, NULL);
    if (!window)    {
        fprintf(stderr, "Could not call glfwCreateWindow(...) successfully.\n");
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    //glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetCharCallback(window, glfw_char_callback);
    glfwSetCursorPosCallback(window, glfw_mouse_move_callback);
    glfwSetCursorEnterCallback(window, glfw_mouse_enter_leave_callback);

    glfwSetWindowSizeCallback(window, glfw_window_size_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSetWindowIconifyCallback(window, glfw_window_iconify_callback);

#ifdef IMGUI_USE_GLAD
   if(!gladLoadGL()) {
        fprintf(stderr,"Error initializing GLAD!\n");
        return false;
    }
    // gladLoadGLLoader(&glfwGetProcAddress);
#endif //IMGUI_USE_GLAD
#ifdef IMGUI_USE_GL3W
   if (gl3wInit()) {
       fprintf(stderr, "Error initializing GL3W!\n");
       return false;
   }
#endif //IMGUI_USE_GL3W

        //OpenGL info
    {
        printf("GLFW Version: %d.%d.%d\n",GLFW_VERSION_MAJOR,GLFW_VERSION_MINOR,GLFW_VERSION_REVISION);
        printf("GL Vendor: %s\n", glGetString( GL_VENDOR ));
        printf("GL Renderer : %s\n", glGetString( GL_RENDERER ));
        printf("GL Version (string) : %s\n",  glGetString( GL_VERSION ));
#       ifndef IMIMPL_SHADER_NONE
        printf("GLSL Version : %s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ));
#       endif //IMIMPL_SHADER_NONE
        //printf("GL Extensions:\n%s\n",(char *) glGetString(GL_EXTENSIONS));
    }

#ifdef IMGUI_USE_GLEW
    GLenum err = glewInit();
    if( GLEW_OK != err )
    {
        fprintf(stderr, "Error initializing GLEW: %s\n",
                glewGetErrorString(err) );
        return false;
    }
#endif //IMGUI_USE_GLEW

#ifdef IMIMPL_FORCE_DEBUG_CONTEXT
    glDebugMessageCallback(GLDebugMessageCallback,NULL);    // last is userParam
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glEnable(GL_DEBUG_OUTPUT);   // use glDisable(GL_DEBUG_OUTPUT); at runtime to disable it

    // Test:
    //glEnable(GL_DEPTH); // instead of glEnable(GL_DEPTH_TEST); => Produces: GL_INVALID_ENUM error generated. <cap> enum is invalid; expected GL_ALPHA_TEST, GL_BLEND, GL_COLOR_MATERIAL, GL_CULL_FACE, GL_DEPTH_TEST, GL_DITHER, GL_FOG, etc. (136 others).
#endif //IMIMPL_FORCE_DEBUG_CONTEXT

	return true;
}

struct ImImplMainLoopFrameStruct {
int done;
#ifndef GLFW_HAS_MOUSE_CURSOR_SUPPORT
#ifndef IMGUI_GLFW_NO_NATIVE_CURSORS
#if (!defined(IMGUI_USE_WIN32_CURSORS) && defined(IMGUI_USE_X11_CURSORS))
Display* x11Display;
Window x11Window;
#endif //IMGUI_USE_CURSORS
#endif //IMGUI_GLFW_NO_NATIVE_CURSORS
#endif //GLFW_HAS_MOUSE_CURSOR_SUPPORT
ImImplMainLoopFrameStruct() : done(false) {}
};



static void ImImplMainLoopFrame(void* userPtr)	{
    ImImplMainLoopFrameStruct& mainLoopFrameStruct = *((ImImplMainLoopFrameStruct*) userPtr);

    static double time = 0.0f;
    ImGuiIO& io = ImGui::GetIO();

    for (size_t i = 0; i < 5; i++) gImGuiBindingMouseDblClicked[i] = false;   // We manually set it (otherwise it won't work with low frame rates)
    if (!gImGuiPaused)	{
        static ImGuiMouseCursor oldCursor = ImGuiMouseCursor_Arrow;
        static bool oldMustHideCursor = io.MouseDrawCursor;
        if (oldMustHideCursor!=io.MouseDrawCursor) {
            glfwSetInputMode(window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
            oldMustHideCursor = io.MouseDrawCursor;
            oldCursor = ImGuiMouseCursor_Count_;
        }
        if (!io.MouseDrawCursor) {
            if (oldCursor!=ImGui::GetMouseCursor()) {
                oldCursor=ImGui::GetMouseCursor();
#               ifdef GLFW_HAS_MOUSE_CURSOR_SUPPORT
                glfwSetCursor(window,glfwCursors[oldCursor]);
#               else //GLFW_HAS_MOUSE_CURSOR_SUPPORT
#                   ifndef IMGUI_GLFW_NO_NATIVE_CURSORS
#                       ifdef IMGUI_USE_WIN32_CURSORS
                            SetCursor(win32Cursors[oldCursor]);           // If this does not work, it's bacause the native Window must be created with a NULL cursor (but how to tell glfw about it?)
#                       elif defined IMGUI_USE_X11_CURSORS
                            XDefineCursor(mainLoopFrameStruct.x11Display,mainLoopFrameStruct.x11Window,x11Cursors[oldCursor]);
#                       endif
#                   endif //IMGUI_GLFW_NO_NATIVE_CURSORS
#               endif //GLFW_HAS_MOUSE_CURSOR_SUPPORT
            }
        }
    }

    static const int numFramesDelay = 12;
    static int curFramesDelay = -1;
    if (gImGuiAppIsIconified || (gImGuiWereOutsideImGui ? (gImGuiInverseFPSClampOutsideImGui==0) : (gImGuiInverseFPSClampInsideImGui==0))) {
        //fprintf(stderr,"glfwWaitEvents() Start %1.4f\n",glfwGetTime());
        glfwWaitEvents();
        //fprintf(stderr,"glfwWaitEvents() End %1.4f\n",glfwGetTime());
    }
    else glfwPollEvents();

    // Setup timestep
    const double current_time =  glfwGetTime();
    static float deltaTime = (float)(current_time -time);
    deltaTime = (float) (current_time - time);
    time = current_time;

    // Start the frame
    {
        io.DeltaTime = (float) deltaTime;
        for (size_t i = 0; i < 5; i++) io.MouseDown[i]=(glfwGetMouseButton(window, i)!=GLFW_RELEASE);
        if (!gImGuiPaused) ImGui::NewFrame();
        else {
            ImImpl_NewFramePaused();    // Enables some ImGui queries regardless ImGui::NewFrame() not being called.
            gImGuiCapturesInput = false;
        }
        for (size_t i = 0; i < 5; i++) io.MouseDoubleClicked[i]=gImGuiBindingMouseDblClicked[i];   // We manually set it (otherwise it won't work with low frame rates)
    }

    if (gImGuiPreDrawGLCallback) gImGuiPreDrawGLCallback();
    DrawGL();


    if (!gImGuiPaused)	{
        gImGuiWereOutsideImGui = !ImGui::IsMouseHoveringAnyWindow() && !ImGui::IsAnyItemActive();
        const bool imguiNeedsInputNow = !gImGuiWereOutsideImGui && (io.WantTextInput || io.MouseDelta.x!=0 || io.MouseDelta.y!=0 || io.MouseWheel!=0);// || io.MouseDownOwned[0] || io.MouseDownOwned[1] || io.MouseDownOwned[2]);
        if (gImGuiCapturesInput != imguiNeedsInputNow) {
            gImGuiCapturesInput = imguiNeedsInputNow;
             if (gImGuiDynamicFPSInsideImGui) {
                if (!gImGuiCapturesInput && !gImGuiWereOutsideImGui) curFramesDelay = 0;
                else curFramesDelay = -1;                
            }            
        }
        if (gImGuiWereOutsideImGui) curFramesDelay = -1;
        //fprintf(stderr,"gImGuiCapturesInput=%s curFramesDelay=%d wereOutsideImGui=%s\n",gImGuiCapturesInput?"true":"false",curFramesDelay,wereOutsideImGui?"true":"false");

        // Rendering
        ImGui::Render();
    }
    else {gImGuiWereOutsideImGui=true;curFramesDelay = -1;}

    if (gImGuiPreDrawGLSwapBuffersCallback) gImGuiPreDrawGLSwapBuffersCallback();
    glfwSwapBuffers(window);
    if (gImGuiPostDrawGLSwapBuffersCallback) gImGuiPostDrawGLSwapBuffersCallback();

    // Reset additional special keys composed states (mandatory):
    for (int i=0;i<12;i++) {gImGuiFunctionKeyPressed[i] = gImGuiFunctionKeyReleased[i]= false;}

    // Handle clamped FPS:
    if (curFramesDelay>=0 && ++curFramesDelay>numFramesDelay) WaitFor(200);     // 200 = 5 FPS - frame rate when ImGui is inactive
    else {
        const float& inverseFPSClamp = gImGuiWereOutsideImGui ? gImGuiInverseFPSClampOutsideImGui : gImGuiInverseFPSClampInsideImGui;
        if (inverseFPSClamp==0.f) WaitFor(500);
        // If needed we must wait (inverseFPSClamp-deltaTime) seconds (=> honestly I shouldn't add the * 2.0f factor at the end, but ImGui tells me the wrong FPS otherwise... why? <=)
        else if (inverseFPSClamp>0 && deltaTime < inverseFPSClamp)  WaitFor((unsigned int) ((inverseFPSClamp-deltaTime)*1000.f * 2.0f) );
    }

#	ifdef __EMSCRIPTEN__
    if ((mainLoopFrameStruct.done=!glfwWindowShouldClose(window))==0) emscripten_cancel_main_loop();
#   endif //__EMSCRIPTEN__
}


// Application code
int ImImpl_Main(const ImImpl_InitParams* pOptionalInitParams,int argc, char** argv)
{
    if (!InitBinding(pOptionalInitParams,argc,argv)) return -1;
    InitImGui(pOptionalInitParams);
    ImGuiIO& io = ImGui::GetIO();        

ImImplMainLoopFrameStruct mainLoopFrameStruct;
    // New: create cursors-------------------------------------------
#ifdef GLFW_HAS_MOUSE_CURSOR_SUPPORT
        for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
            glfwCursors[i] = glfwCreateStandardCursor(glfwCursorIds[i]);
            if (i==0) glfwSetCursor(window,glfwCursors[i]);
        }
#else //GLFW_HAS_MOUSE_CURSOR_SUPPORT
#   ifndef IMGUI_GLFW_NO_NATIVE_CURSORS
#       ifdef IMGUI_USE_WIN32_CURSORS
        for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
            win32Cursors[i] = LoadCursor(NULL,(LPCTSTR) win32CursorIds[i]);
            if (i==0) SetCursor(win32Cursors[i]);
        }
#       elif defined IMGUI_USE_X11_CURSORS
        mainLoopFrameStruct.x11Display = glfwGetX11Display();
        mainLoopFrameStruct.x11Window = glfwGetX11Window(window);
        //XColor white;white.red=white.green=white.blue=255;
        //XColor black;black.red=black.green=black.blue=0;
        for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
            x11Cursors[i] = XCreateFontCursor(mainLoopFrameStruct.x11Display,x11CursorIds[i]);
            //XRecolorCursor(x11Display, x11Cursors[i], &white,&black);
            if (i==0) XDefineCursor(mainLoopFrameStruct.x11Display,mainLoopFrameStruct.x11Window,x11Cursors[i]);
        }
#       endif
#   endif //IMGUI_GLFW_NO_NATIVE_CURSORS
#endif //GLFW_HAS_MOUSE_CURSOR_SUPPORT
    //---------------------------------------------------------------

    InitGL();
    if (gImGuiPostInitGLCallback) gImGuiPostInitGLCallback();
 	ResizeGL(io.DisplaySize.x,io.DisplaySize.y);
	
    gImGuiInverseFPSClampInsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampInsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampInsideImGui) : 1.0f) : -1.0f;
    gImGuiInverseFPSClampOutsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampOutsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampOutsideImGui) : 1.0f) : -1.0f;
    gImGuiDynamicFPSInsideImGui = pOptionalInitParams ? pOptionalInitParams->gFpsDynamicInsideImGui : false;

    mainLoopFrameStruct.done = 0;
#	ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(ImImplMainLoopFrame,&mainLoopFrameStruct, 0, 1);
#	else
    while ((mainLoopFrameStruct.done=!glfwWindowShouldClose(window)))   {
        ImImplMainLoopFrame((void*)&mainLoopFrameStruct);
    }
#	endif //__EMSCRIPTEN__


    DestroyGL();
    ImGui::Shutdown();
    DestroyImGuiFontTexture();
    DestroyImGuiProgram();
    DestroyImGuiBuffer();

    // New: delete cursors-------------------------------------------
#   ifdef  GLFW_HAS_MOUSE_CURSOR_SUPPORT
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        glfwDestroyCursor(glfwCursors[i]);
    }
#   else   //GLFW_HAS_MOUSE_CURSOR_SUPPORT
#       ifndef IMGUI_GLFW_NO_NATIVE_CURSORS
#           ifdef IMGUI_USE_WIN32_CURSORS
                // Nothing to do
#           elif defined IMGUI_USE_X11_CURSORS
                XUndefineCursor(mainLoopFrameStruct.x11Display,mainLoopFrameStruct.x11Window);
                for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
                    XFreeCursor(mainLoopFrameStruct.x11Display,x11Cursors[i]);
                }
#           endif
#       endif //IMGUI_GLFW_NO_NATIVE_CURSORS
#   endif //GLFW_HAS_MOUSE_CURSOR_SUPPORT
    //---------------------------------------------------------------

    glfwTerminate();
    return 0;
}

#endif //#ifndef IMIMPL_BINDING_H

