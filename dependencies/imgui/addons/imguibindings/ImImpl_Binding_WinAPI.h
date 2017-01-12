#ifndef IMIMPL_BINDING_H
#define IMIMPL_BINDING_H

#include <tchar.h>


#include "imguibindings.h"



static HWND window = NULL;
static ImVec2 mousePosScale(1.0f, 1.0f);

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


// Notify OS Input Method Editor of text input position (e.g. when using Japanese/Chinese inputs, otherwise this isn't needed)
static void ImImpl_ImeSetInputScreenPosFn(int x, int y)
{
    HWND hwnd = window;
    if (HIMC himc = ImmGetContext(hwnd))
    {
        COMPOSITIONFORM cf;
        cf.ptCurrentPos.x = x;
        cf.ptCurrentPos.y = y;
        cf.dwStyle = CFS_FORCE_POSITION;
        ImmSetCompositionWindow(himc, &cf);
    }
}

//static bool gImGuiBindingMouseDblClicked[5]={false,false,false,false,false};  // moved
static bool gImGuiAppIconized = false;

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (message)
    {
        case WM_SIZE:

            switch (wParam)
            {
                case SIZE_MINIMIZED:
                //fprintf(stderr,"SIZE_MINIMIZED %d %d\n",wParam,lParam);
                return 0;
            }
        ResizeGL(LOWORD (lParam),HIWORD (lParam));
        break;

    case WM_CREATE:
        return 0;

    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;

    case WM_DESTROY:
        return 0;
    case WM_ACTIVATE:
        gImGuiAppIconized = (HIWORD(wParam) == SIZE_MINIMIZED); // SIZE_MINIMIZED == 1
        //fprintf(stderr,"WM_ACTIVATE %d %d %d gImGuiAppIconized=%d\n",wParam,LOWORD(wParam),HIWORD(wParam),gImGuiAppIconized);
        return 0;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam>=0 && wParam<0x10000) io.AddInputCharacter((unsigned short)wParam);
        return 0;
    case WM_MOUSEMOVE:
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_MOUSELEAVE:
        io.MousePos.x = io.MousePos.y = -1.0f;
        return 0;
    case WM_MOUSEWHEEL:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[0] = (wParam&MK_LBUTTON);
        io.MouseDown[2] = (wParam&MK_MBUTTON);
        io.MouseDown[1] = (wParam&MK_RBUTTON);
        io.MouseWheel = GET_WHEEL_DELTA_WPARAM(wParam) > 0  ? 1 : -1;   // it's 120 or -120
        return 0;
    case WM_LBUTTONDBLCLK:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        gImGuiBindingMouseDblClicked[0] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_MBUTTONDBLCLK:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        gImGuiBindingMouseDblClicked[2] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_RBUTTONDBLCLK:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        gImGuiBindingMouseDblClicked[1] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_LBUTTONDOWN:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[0] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_LBUTTONUP:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[0] = false;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_MBUTTONDOWN:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[2] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_MBUTTONUP:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[2] = false;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_RBUTTONDOWN:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[1] = true;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    case WM_RBUTTONUP:
        io.KeyCtrl = (wParam&MK_CONTROL);
        io.KeyShift = (wParam&MK_SHIFT);
        io.KeyAlt = (wParam&MK_ALT);
        io.MouseDown[1] = false;
        io.MousePos = ImVec2((float)LOWORD(lParam) * mousePosScale.x, (float)HIWORD(lParam) * mousePosScale.y);      // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        return 0;
    default:
        return DefWindowProc( hWnd, message, wParam, lParam );

    }
    return 0;
}




static void InitImGui(const ImImpl_InitParams* pOptionalInitParams=NULL)	{
    //int w, h;
    int fb_w, fb_h;
    //glfwGetWindowSize(window, &w, &h);
    //glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = 1.f;//(float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = 1.f;//(float)fb_h / h;
    RECT rect = {0};::GetClientRect(window,&rect);
    fb_w = rect.right - rect.left;
    fb_h = rect.bottom - rect.top;


    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);  // Display size, in pixels. For clamping windows positions.
    io.DeltaTime = 1.0f/60.0f;                          // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    //io.PixelCenterOffset = 0.0f;                        // Align OpenGL texels

    io.KeyMap[ImGuiKey_Tab] = VK_TAB;             // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;

    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.ImeSetInputScreenPosFn = ImImpl_ImeSetInputScreenPosFn;

    // 3 common init steps
    InitImGuiFontTexture(pOptionalInitParams);
    InitImGuiProgram();
    InitImGuiBuffer();
}




// Application code
int ImImpl_WinMain(const ImImpl_InitParams* pOptionalInitParams,HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int iCmdShow)
{
    WNDCLASS wc;
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
    MSG msg;
    BOOL quit = FALSE;


    // register window class
    wc.style = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
    wc.hCursor = LoadCursor( NULL, NULL);//IDC_ARROW );
    wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("ImGuiApp");
    RegisterClass( &wc );

    const int width = pOptionalInitParams ? pOptionalInitParams->gWindowSize.x : 1270;
    const int height = pOptionalInitParams ? pOptionalInitParams->gWindowSize.y : 720;

    // create main window
    window = hWnd = CreateWindow(
        _T("ImGuiApp"),
        (pOptionalInitParams && pOptionalInitParams->gWindowTitle[0]!='\0') ? (TCHAR*) &pOptionalInitParams->gWindowTitle[0] : _T("ImGui WinApi OpenGL Example"),
        WS_CAPTION | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        0, 0,
        width,
        height,
        NULL, NULL, hInstance, NULL );

    // enable OpenGL for the window-------------------------------------------------
    PIXELFORMATDESCRIPTOR pfd;
    int format;

    // get the device context (DC)
    hDC = GetDC( hWnd );

    // set the pixel format for the DC
    ZeroMemory( &pfd, sizeof( pfd ) );
    pfd.nSize = sizeof( pfd );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.cStencilBits = 1;
    pfd.iLayerType = PFD_MAIN_PLANE;
    format = ChoosePixelFormat( hDC, &pfd );
    SetPixelFormat( hDC, format, &pfd );

    // create and enable the render context (RC)
    hRC = wglCreateContext( hDC );
    wglMakeCurrent( hDC, hRC );
    //----------------------------------------------------------------------------------

    // New: create cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        win32Cursors[i] = LoadCursor(NULL,(LPCTSTR) win32CursorIds[i]);
        if (i==0) SetCursor(win32Cursors[i]);
    }
    //---------------------------------------------------------------

#ifdef IMGUI_USE_GLAD
   if(!gladLoadGL()) {
        fprintf(stderr,"Error initializing GLAD!\n");
        return false;
    }
    // gladLoadGLLoader(&GetProcAddress);
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
   }
#endif //IMGUI_USE_GLEW

    static double time = 0.0f;
    gImGuiInverseFPSClampInsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampInsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampInsideImGui) : 1.0f) : -1.0f;
    gImGuiInverseFPSClampOutsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampOutsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampOutsideImGui) : 1.0f) : -1.0f;
    gImGuiDynamicFPSInsideImGui = pOptionalInitParams ? pOptionalInitParams->gFpsDynamicInsideImGui : false;

    InitImGui(pOptionalInitParams);
    InitGL();
    if (gImGuiPostInitGLCallback) gImGuiPostInitGLCallback();
    ResizeGL(width,height);

    ImGuiIO& io = ImGui::GetIO();
    // program main loop
    while ( !quit )
    {
        for (size_t i = 0; i < 5; i++) gImGuiBindingMouseDblClicked[i] = false;   // We manually set it (otherwise it won't work with low frame rates)
        // check for messages
        if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE )  )
        {

            // handle or dispatch messages
            if ( msg.message == WM_QUIT )
            {
                quit = TRUE;
            }
            else
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }


        };


        // Setup timestep
        const double current_time =  ::GetTickCount();
        static float deltaTime = (float)(0.001*(double)(current_time - time));
        deltaTime = (float)(0.001*(double)(current_time - time));
        time = current_time;
        if (deltaTime<=0) deltaTime=1.0f/60.0f;


        // Start the frame
        {
            io.DeltaTime = deltaTime;

            // Setup inputs (why I didn't use the event loop?)
            BYTE keystate[256];
            ::GetKeyboardState(keystate);
            io.KeyCtrl = (keystate[VK_CONTROL] & 0x80) != 0;
            io.KeyShift = (keystate[VK_SHIFT] & 0x80) != 0;
            io.KeyAlt = (keystate[VK_MENU] & 0x80) != 0;
            const bool capsLockDown = (keystate[VK_CAPITAL] & 0x80) != 0;
            const bool lowercaseWithoutCtrl = !io.KeyCtrl && (!(io.KeyShift || capsLockDown));
            for (int i = 0; i < 256; i++)   io.KeysDown[i] = (keystate[i] & 0x80) != 0;
            if (lowercaseWithoutCtrl)   {
                for (int i='A';i<='Z';i++) {
                    io.KeysDown[i-'A'+'a'] = io.KeysDown[i];
                    io.KeysDown[i] = false;
                }
            }
            for (int key=VK_F1;key<VK_F12;key++)  {
                const int i = key-VK_F1;
                const bool prevState = gImGuiFunctionKeyDown[i];
                const bool down = (keystate[key] & 0x80) != 0;  // Or just query "key" only, if (key>=256)
                gImGuiFunctionKeyDown[i] = down;
                if (down!=prevState)    {
                    if (down) gImGuiFunctionKeyPressed[i] = true;
                    else gImGuiFunctionKeyReleased[i] = true;
                }
            }

            if (!gImGuiPaused)	{
                static ImGuiMouseCursor oldCursor = ImGuiMouseCursor_Arrow;
                static bool oldMustHideCursor = io.MouseDrawCursor;
                if (oldMustHideCursor!=io.MouseDrawCursor) {
                    ShowCursor(!io.MouseDrawCursor);
                    oldMustHideCursor = io.MouseDrawCursor;
                    oldCursor = ImGuiMouseCursor_Count_;
                }
                if (!io.MouseDrawCursor) {
                    if (oldCursor!=ImGui::GetMouseCursor()) {
                        oldCursor=ImGui::GetMouseCursor();
                        SetCursor(win32Cursors[oldCursor]);
                        //fprintf(stderr,"SetCursor(%d);\n",oldCursor);
                    }
                }
                ImGui::NewFrame();
            }
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

            // Rendering
            ImGui::Render();
        }
        else {gImGuiWereOutsideImGui=true;curFramesDelay = -1;}

        if (gImGuiPreDrawGLSwapBuffersCallback) gImGuiPreDrawGLSwapBuffersCallback();
        SwapBuffers( hDC );
        if (gImGuiPostDrawGLSwapBuffersCallback) gImGuiPostDrawGLSwapBuffersCallback();

        // Reset additional special keys composed states (mandatory):
        for (int i=0;i<12;i++) {gImGuiFunctionKeyPressed[i] = gImGuiFunctionKeyReleased[i]= false;}

        // Handle clamped FPS:
        if (gImGuiAppIconized) WaitFor(500);
        else if (curFramesDelay>=0 && ++curFramesDelay>numFramesDelay) WaitFor(200);     // 200 = 5 FPS - frame rate when ImGui is inactive
        else {
            const float& inverseFPSClamp = gImGuiWereOutsideImGui ? gImGuiInverseFPSClampOutsideImGui : gImGuiInverseFPSClampInsideImGui;
            if (inverseFPSClamp==0.f) WaitFor(500);
            // If needed we must wait (gImGuiInverseFPSClamp-deltaTime) seconds (=> honestly I shouldn't add the * 2.0f factor at the end, but ImGui tells me the wrong FPS otherwise... why? <=)
            else if (inverseFPSClamp>0.f && deltaTime < inverseFPSClamp)  WaitFor((unsigned int) ((inverseFPSClamp-deltaTime)*1000.f * 2.0f) );
        }

    }

    DestroyGL();
    ImGui::Shutdown();
    DestroyImGuiFontTexture();
    DestroyImGuiProgram();
    DestroyImGuiBuffer();

    // New: delete cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        //DestroyCursor(win32Cursors[i]);   // Nope: LoadCursor() loads SHARED cursors that should not be destroyed
    }
    //---------------------------------------------------------------

    // shutdown OpenGL
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( hRC );
    ReleaseDC( hWnd, hDC );

    // destroy the window explicitly
    DestroyWindow( hWnd );

    return msg.wParam;
}


#endif //#ifndef IMIMPL_BINDING_H

