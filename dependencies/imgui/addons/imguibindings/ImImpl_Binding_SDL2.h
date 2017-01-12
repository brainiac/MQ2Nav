#ifndef IMIMPL_BINDING_H
#define IMIMPL_BINDING_H

#include "imguibindings.h"


static SDL_Window* window = NULL;
static ImVec2 mousePosScale(1.0f, 1.0f);

static const SDL_SystemCursor sdlCursorIds[ImGuiMouseCursor_Count_+1] = {
    SDL_SYSTEM_CURSOR_ARROW,
    SDL_SYSTEM_CURSOR_IBEAM,
    SDL_SYSTEM_CURSOR_HAND,//SDL_SYSTEM_CURSOR_SIZEALL,      //SDL_SYSTEM_CURSOR_HAND,    // or SDL_SYSTEM_CURSOR_SIZEALL  //ImGuiMouseCursor_Move,                  // Unused by ImGui
    SDL_SYSTEM_CURSOR_SIZENS,       //ImGuiMouseCursor_ResizeNS,              // Unused by ImGui
    SDL_SYSTEM_CURSOR_SIZEWE,       //ImGuiMouseCursor_ResizeEW,              // Unused by ImGui
    SDL_SYSTEM_CURSOR_SIZENESW,     //ImGuiMouseCursor_ResizeNESW,
    SDL_SYSTEM_CURSOR_SIZENWSE,     //ImGuiMouseCursor_ResizeNWSE,          // Unused by ImGui
    SDL_SYSTEM_CURSOR_ARROW         //,ImGuiMouseCursor_Arrow
};
static SDL_Cursor* sdlCursors[ImGuiMouseCursor_Count_+1];

// NB: ImGui already provide OS clipboard support for Windows so this isn't needed if you are using Windows only.
static const char* ImImpl_GetClipboardTextFn(void*)
{
    //return SDL_GetClipboardText();	// Wrong!!! Gets UTF-8 text from the clipboard, which must be freed with SDL_free(), otherwise leaks memory!
    const char* text = SDL_GetClipboardText();
    if (!text) return NULL;
    const int len = strlen(text);
    if (len==0) {SDL_free((void*)text);return "";}
    static ImVector<char> clipboardBuffer;
    // Optional branch to keep clipboardBuffer.capacity() low:
    if (len<=clipboardBuffer.capacity() && clipboardBuffer.capacity()>512)  {
        ImVector<char> emptyBuffer;clipboardBuffer.swap(emptyBuffer);
    }
    clipboardBuffer.resize(len+1);
    strcpy(&clipboardBuffer[0],text);
    SDL_free((void*)text);
    return (const char*) &clipboardBuffer[0];
}

static void ImImpl_SetClipboardTextFn(void*,const char* text)
{
    SDL_SetClipboardText(text);
}
// TODO: once we can find out how to get a HWND from SDL2
/*
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
*/


static void InitImGui(const ImImpl_InitParams* pOptionalInitParams=NULL)	{
    //int w, h;
    int fb_w, fb_h;
    SDL_GetWindowSize(window, &fb_w, &fb_h);
    //glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = 1.f;//(float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = 1.f;//(float)fb_h / h;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);  // Display size, in pixels. For clamping windows positions.
    io.DeltaTime = 1.0f/60.0f;                          // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    //io.PixelCenterOffset = 0.0f;                        // Align OpenGL texels

    io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDLK_a;
    io.KeyMap[ImGuiKey_C] = SDLK_c;
    io.KeyMap[ImGuiKey_V] = SDLK_v;
    io.KeyMap[ImGuiKey_X] = SDLK_x;
    io.KeyMap[ImGuiKey_Y] = SDLK_y;
    io.KeyMap[ImGuiKey_Z] = SDLK_z;

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
    io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;
#ifdef _MSC_VER
    //io.ImeSetInputScreenPosFn = ImImpl_ImeSetInputScreenPosFn;
#endif

    // 3 common init steps
    InitImGuiFontTexture(pOptionalInitParams);
    InitImGuiProgram();
    InitImGuiBuffer();
}

static bool InitBinding(const ImImpl_InitParams* pOptionalInitParams=NULL,int argc=0, char** argv=NULL)	{
/*  // From the web:
SDL_INIT_EVERYTHING & ~(SDL_INIT_TIMER | SDL_INIT_HAPTIC) succeeds in Firefox. Timer doesn't work because it requires threads, and haptic support also isn't included. SDL_INIT_AUDIO requires Web Audio, and causes failure in IE. There is no flag for threads; they are simply initialized by default if they are enabled.
In the SDL2 version of my DOSBox port, I simply don't use SDL_INIT_TIMER and call SDL_GetTicks() anyways. Yes, that is wrong according to SDL documentation, but it works. Availability of that function is probably important for games. For audio, I use SDL_InitSubSystem(SDL_INIT_AUDIO), and run in no audio mode if that fails.
I think failing in SDL_Init() when a requested subsystem doesn't work properly is reasonable. When porting it's a lot easier to understand that failure than than failures later on when trying to use it.
*/
#ifndef IMIMPL_SDL2_INIT_FLAGS
#   ifndef __EMSCRIPTEN__
#		define IMIMPL_SDL2_INIT_FLAGS (SDL_INIT_EVERYTHING)
#	else //__EMSCRIPTEN__
#		if (defined(WITH_SDL) || defined(WITH_SDL_STATIC) || defined(WITH_SDL2) || defined(WITH_SDL2_STATIC)) // YES_IMGUISOLOUD
#			define IMIMPL_SDL2_INIT_FLAGS (SDL_INIT_VIDEO|SDL_INIT_AUDIO)
#		else // WITH_SDL2_STATIC // YES_IMGUISOLOUD
#			define IMIMPL_SDL2_INIT_FLAGS (SDL_INIT_VIDEO)
#		endif // WITH_SDL2_STATIC // YES_IMGUISOLOUD
#	endif //__EMSCRIPTEN__
#endif //IMIMPL_SDL2_INIT_FLAGS

    if (SDL_Init(IMIMPL_SDL2_INIT_FLAGS) < 0)	{
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    // Init OpenGL
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

//-ENABLE-OPENGLES COMPATIBILITY PROFILES----------------------------------
#ifndef IMIMPL_SHADER_NONE
#ifdef IMIMPL_SHADER_GLES
#   ifndef IMIMPL_SHADER_GL3
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);          // 1 => ES1.1   2 => ES2.0  3 => ES3.0
#   else //IMIMPL_SHADER_GL3
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#   endif //MIMPL_SHADER_GL3
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
#endif // IMIMPL_SHADER_GLES
#endif //IMIMPL_SHADER_NONE
//--------------------------------------------------------------------------

    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,);
    //SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,1);

    window = SDL_CreateWindow((pOptionalInitParams && pOptionalInitParams->gWindowTitle[0]!='\0') ? (const char*) &pOptionalInitParams->gWindowTitle[0] : "ImGui SDL2 OpenGL example",
                          SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED,
                          pOptionalInitParams ? pOptionalInitParams->gWindowSize.x : 1270,
                          pOptionalInitParams ? pOptionalInitParams->gWindowSize.y : 720,
                          SDL_WINDOW_RESIZABLE |
                          SDL_WINDOW_OPENGL
                          );
    if (!window)
    {
        fprintf(stderr, "Could not call SDL_CreateWindow(...) successfully.\n");
        return false;
    }

    SDL_GL_CreateContext(window);


#ifdef IMGUI_USE_GLAD
   if(!gladLoadGL()) {
        fprintf(stderr,"Error initializing GLAD!\n");
        return false;
    }
    // gladLoadGLLoader(&SDL_GL_GetProcAddress);
#endif //IMGUI_USE_GLAD
#ifdef IMGUI_USE_GL3W
   if (gl3wInit()) {
       fprintf(stderr, "Error initializing GL3W!\n");
       return false;
   }
#endif //IMGUI_USE_GL3W

    //OpenGL info
    {
        printf("GL Vendor: %s\n", glGetString( GL_VENDOR ));
        printf("GL Renderer : %s\n", glGetString( GL_RENDERER ));
        printf("GL Version (string) : %s\n",  glGetString( GL_VERSION ));
#       ifndef IMIMPL_SHADER_NONE
        printf("GLSL Version : %s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ));
#       endif // IMIMPL_SHADER_NONE
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

	return true;
}


#   include <SDL2/SDL.h>
static void ImImplMainLoopFrame(void* pDone)	{
    ImGuiIO& io = ImGui::GetIO();
    int& done = *((int*) pDone);
    static SDL_Event event;

    for (size_t i = 0; i < 5; i++) gImGuiBindingMouseDblClicked[i] = false;   // We manually set it (otherwise it won't work with low frame rates)
    if (!gImGuiPaused) {
        static ImGuiMouseCursor oldCursor = ImGuiMouseCursor_Arrow;
        static bool oldMustHideCursor = io.MouseDrawCursor;
        if (oldMustHideCursor!=io.MouseDrawCursor) {
            SDL_ShowCursor(io.MouseDrawCursor?0:1);
            oldMustHideCursor = io.MouseDrawCursor;
            oldCursor = ImGuiMouseCursor_Count_;
        }
        if (!io.MouseDrawCursor) {
            if (oldCursor!=ImGui::GetMouseCursor()) {
                oldCursor=ImGui::GetMouseCursor();
                SDL_SetCursor(sdlCursors[oldCursor]);
            }
        }
    }

    static bool gImGuiAppIsIconified = false;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_WINDOWEVENT: {
            if (event.window.event<=SDL_WINDOWEVENT_RESTORED) gImGuiAppIsIconified = (event.window.event==SDL_WINDOWEVENT_MINIMIZED || event.window.event==SDL_WINDOWEVENT_HIDDEN);
            //printf("%d\n",event.window.event);fflush(stdout);
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
            {
                int width = event.window.data1;
                int height = event.window.data2;
                int fb_w, fb_h;
                fb_w = width;fb_h = height;
                mousePosScale.x = 1.f;//(float)fb_w / w;              // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
                mousePosScale.y = 1.f;//(float)fb_h / h;
                io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);    // Display size, in pixels. For clamping windows positions.

                ResizeGL(width,height);
            }
                break;
            }
        }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            const SDL_Keymod mod = SDL_GetModState();
            io.KeyCtrl = (mod & (KMOD_LCTRL|KMOD_RCTRL)) != 0;
            io.KeyShift = (mod & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0;
            io.KeyAlt = (mod & (KMOD_LALT|KMOD_RALT)) != 0;

            const bool down = (event.type == SDL_KEYDOWN);
            int key = (event.key.keysym.sym);
            static bool gImGuiCapsLockDown = false;
            if (key==SDLK_CAPSLOCK) gImGuiCapsLockDown = down;
            else if (key>=SDLK_a && key<=SDLK_z && (io.KeyShift || gImGuiCapsLockDown)) key-= ((const int)'a'-(const int)'A');
            if (key>=SDLK_F1 && key<=SDLK_F12) {
                const int i = key-SDLK_F1;
                const bool prevState = gImGuiFunctionKeyDown[i];
                gImGuiFunctionKeyDown[i] = down;
                if (down!=prevState)    {
                    if (down) gImGuiFunctionKeyPressed[i] = true;
                    else gImGuiFunctionKeyReleased[i] = true;
                }
                //fprintf(stderr,"%d) D:%d P:%d R:%d\n",i,(int)gImGuiFunctionKeyDown[i],(int)gImGuiFunctionKeyPressed[i],(int)gImGuiFunctionKeyReleased[i]);
            }
            key&=~SDLK_SCANCODE_MASK;
            // Don't remember what these 3 lines are for... removed because they prevent arrows key to work (TODO: understand what these lines were for).
            // Found these 3 lines here: https://github.com/ocornut/imgui/issues/729
            /*if (event.key.keysym.sym & (1<<30)) {
                //fprintf(stderr,"SDL Highbit remask %x -> %x\n", event.key.keysym.sym, key);
                key |= 0x100;
            }*/
            if (key>=0 && key<512)  io.KeysDown[key] = down;
        }
            break;
            //case SDL_TEXTEDITING:   break;
        case SDL_TEXTINPUT:
        {
            io.AddInputCharactersUTF8(event.text.text);
        }
            break;
        case SDL_MOUSEBUTTONDOWN:        /**< Mouse button pressed */
        case SDL_MOUSEBUTTONUP: {
            SDL_Keymod mod = SDL_GetModState();
            io.KeyCtrl = (mod & (KMOD_LCTRL|KMOD_RCTRL)) != 0;
            io.KeyShift = (mod & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0;
            io.KeyAlt = (mod & (KMOD_LALT|KMOD_RALT)) != 0;
            if (event.button.button>0 && event.button.button<6) {
                static const int evBtnMap[5]={0,2,1,3,4};
                io.MouseDown[ evBtnMap[event.button.button-1] ] = (event.button.type == SDL_MOUSEBUTTONDOWN);
                if (event.button.clicks==2 && event.button.type == SDL_MOUSEBUTTONDOWN) gImGuiBindingMouseDblClicked[evBtnMap[event.button.button-1]] = true;
                //else gImGuiBindingMouseDblClicked[event.button.button-1] = false;
            }
            //fprintf(stderr,"mousePressed[%d] = %s\n",event.button.button-1,(event.button.type == SDL_MOUSEBUTTONDOWN)?"true":"false");
        }
            break;
        case SDL_MOUSEWHEEL:
            // positive away from the user and negative toward the user
            io.MouseWheel = (event.wheel.y != 0) ? event.wheel.y > 0 ? 1 : - 1 : 0;           // Mouse wheel: -1,0,+1
            //fprintf(stderr,"io.MouseWheel = %d (%d,%d)\n",io.MouseWheel,event.wheel.x,event.wheel.y); // set correctly, but why it does not seem to work ?
            break;
        case SDL_MOUSEMOTION:
            io.MousePos = ImVec2((float)event.motion.x * mousePosScale.x, (float)event.motion.y * mousePosScale.y);
            //fprintf(stderr,"io.MousePos (%1.2f,%1.2f)\n",io.MousePos.x,io.MousePos.y);
            break;
        case SDL_QUIT:
            done = 1;
#				ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#				endif //
            break;
        default:
            break;
        }
    }

    // Setup io.DeltaTime
    static Uint32  time = SDL_GetTicks();
    const Uint32  current_time =  SDL_GetTicks();
    static float deltaTime = (float)(0.001*(double)(current_time - time));
    deltaTime = (float)(0.001*(double)(current_time - time));
    if (deltaTime<=0) deltaTime=1.0f/60.0f;
    time = current_time;

    // Start the frame
    {
        io.DeltaTime = deltaTime;
        if (!gImGuiPaused) ImGui::NewFrame();
        else {
            ImImpl_NewFramePaused();    // Enables some ImGui queries regardless ImGui::NewFrame() not being called.
            gImGuiCapturesInput = false;
        }
        for (size_t i = 0; i < 5; i++) io.MouseDoubleClicked[i]=gImGuiBindingMouseDblClicked[i];   // We manually set it (otherwise it won't work with low frame rates)
    }

    if (gImGuiPreDrawGLCallback) gImGuiPreDrawGLCallback();
    DrawGL();


    static const int numFramesDelay = 12;
    static int curFramesDelay = -1;
    if (!gImGuiPaused)	{
        gImGuiWereOutsideImGui = !ImGui::IsMouseHoveringAnyWindow() && !ImGui::IsAnyItemActive();
        const bool imguiNeedsInputNow = !gImGuiWereOutsideImGui && (io.WantTextInput || io.MouseDelta.x!=0 || io.MouseDelta.y!=0 || io.MouseWheel!=0);// || io.MouseDownOwned[0] || io.MouseDownOwned[1] || io.MouseDownOwned[2]);
        if (gImGuiCapturesInput != imguiNeedsInputNow) {
            gImGuiCapturesInput = imguiNeedsInputNow;
            //fprintf(stderr,"gImGuiCapturesInput=%s\n",gImGuiCapturesInput?"true":"false");
            if (gImGuiDynamicFPSInsideImGui) {
                if (!gImGuiCapturesInput && !gImGuiWereOutsideImGui) curFramesDelay = 0;
                else curFramesDelay = -1;
            }
        }
        if (gImGuiWereOutsideImGui) curFramesDelay = -1;

        ImGui::Render();
    }
    else {gImGuiWereOutsideImGui=true;curFramesDelay = -1;}

    if (gImGuiPreDrawGLSwapBuffersCallback) gImGuiPreDrawGLSwapBuffersCallback();
    SDL_GL_SwapWindow(window);
    if (gImGuiPostDrawGLSwapBuffersCallback) gImGuiPostDrawGLSwapBuffersCallback();

    // Reset additional special keys composed states (mandatory):
    for (int i=0;i<12;i++) {gImGuiFunctionKeyPressed[i] = gImGuiFunctionKeyReleased[i]= false;}

    // Handle clamped FPS:
    if (gImGuiAppIsIconified) {WaitFor(500);/*printf("Minimized\n");fflush(stdout);*/}
    else if (curFramesDelay>=0 && ++curFramesDelay>numFramesDelay) WaitFor(200);     // 200 = 5 FPS - frame rate when ImGui is inactive
    else {
        const float& inverseFPSClamp = gImGuiWereOutsideImGui ? gImGuiInverseFPSClampOutsideImGui : gImGuiInverseFPSClampInsideImGui;
        if (inverseFPSClamp==0.f) WaitFor(500);
        // If needed we must wait (gImGuiInverseFPSClamp-deltaTime) seconds (=> honestly I shouldn't add the * 2.0f factor at the end, but ImGui tells me the wrong FPS otherwise... why? <=)
        else if (inverseFPSClamp>0.f && deltaTime < inverseFPSClamp)  WaitFor((unsigned int) ((inverseFPSClamp-deltaTime)*1000.f * 2.0f) );
    }
}

// Application code
int ImImpl_Main(const ImImpl_InitParams* pOptionalInitParams,int argc, char** argv)
{
    if (!InitBinding(pOptionalInitParams,argc,argv)) return -1;
    // New: create cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        sdlCursors[i] = SDL_CreateSystemCursor(sdlCursorIds[i]);
    }
    //---------------------------------------------------------------

    InitImGui(pOptionalInitParams);
    ImGuiIO& io = ImGui::GetIO();           
    
    InitGL();
    if (gImGuiPostInitGLCallback) gImGuiPostInitGLCallback();
    ResizeGL((int) io.DisplaySize.x,(int) io.DisplaySize.y);
	
    gImGuiInverseFPSClampInsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampInsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampInsideImGui) : 1.0f) : -1.0f;
    gImGuiInverseFPSClampOutsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampOutsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampOutsideImGui) : 1.0f) : -1.0f;
    gImGuiDynamicFPSInsideImGui = pOptionalInitParams ? pOptionalInitParams->gFpsDynamicInsideImGui : false;

	int done = 0;
#	ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(ImImplMainLoopFrame,&done, 0, 1);
#	else
	while (!done)	{
		ImImplMainLoopFrame((void*)&done);
	}
#	endif //__EMSCRIPTEN__

    DestroyGL();
    ImGui::Shutdown();
    DestroyImGuiFontTexture();
    DestroyImGuiProgram();
    DestroyImGuiBuffer();

    // New: delete cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        SDL_FreeCursor(sdlCursors[i]);
    }
    //---------------------------------------------------------------
    if (window) {SDL_DestroyWindow(window);window = NULL;}
    SDL_Quit();
    return 0;
}

#endif //#ifndef IMIMPL_BINDING_H

