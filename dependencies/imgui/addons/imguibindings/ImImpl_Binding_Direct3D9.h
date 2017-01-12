#ifndef IMIMPL_BINDING_H
#define IMIMPL_BINDING_H

#include <tchar.h>


#include "imguibindings.h"



static HWND window = NULL;
static ImVec2 mousePosScale(1.0f, 1.0f);
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp;
static LPDIRECT3DVERTEXBUFFER9  g_pVB = NULL;
static LPDIRECT3DINDEXBUFFER9   g_pIB = NULL;


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



inline static void ImImpl_InvalidateDeviceObjects()
{
    if (!g_pd3dDevice)  return;
    if (g_pVB)  {g_pVB->Release();g_pVB = NULL;}
    if (g_pIB){g_pIB->Release();g_pIB = NULL;}
    // Please note that we create all textures with D3DPOOL_MANAGED
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

            if (g_pd3dDevice)   {
                ImImpl_InvalidateDeviceObjects();
                g_d3dpp.BackBufferWidth  = LOWORD(lParam);
                g_d3dpp.BackBufferHeight = HIWORD(lParam);
                HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
                if (hr == D3DERR_INVALIDCALL)   IM_ASSERT(0);
            }
        ResizeGL(LOWORD (lParam),HIWORD (lParam));
        break;

    case WM_CREATE:
        return 0;

    case WM_CLOSE:
    case WM_DESTROY:
        //fprintf(stderr,"WM_CLOSE\n");
        PostQuitMessage( 0 );
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
    io.ImeWindowHandle = window;

    // 3 common init steps
    InitImGuiFontTexture(pOptionalInitParams);
    //InitImGuiProgram();
    //InitImGuiBuffer();
}




// Application code
int ImImpl_WinMain(const ImImpl_InitParams* pOptionalInitParams,HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int iCmdShow)
{
    WNDCLASS wc;
    HWND hWnd;
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
        (pOptionalInitParams && pOptionalInitParams->gWindowTitle[0]!='\0') ? (TCHAR*) &pOptionalInitParams->gWindowTitle[0] : _T("ImGui Direct3D9 OpenGL Example"),
        WS_CAPTION | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        0, 0,
        width,
        height,
        NULL, NULL, hInstance, NULL );

    // enable Direct3D for the window-------------------------------------------------
    LPDIRECT3D9 pD3D;
    if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
    {
        UnregisterClass(_T("ImGuiApp"), wc.hInstance);
        return 0;
    }
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Create the D3DDevice
    if (pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
    {
        pD3D->Release();
        UnregisterClass(_T("ImGuiApp"), wc.hInstance);
        return 0;
    }
    //----------------------------------------------------------------------------------

    // New: create cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        win32Cursors[i] = LoadCursor(NULL,(LPCTSTR) win32CursorIds[i]);
        if (i==0) SetCursor(win32Cursors[i]);
    }
    //---------------------------------------------------------------


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
            if ( msg.message == WM_QUIT)
            {
                quit = TRUE;
                break;
            }
            else
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }


        };


        const double current_time =  ::GetTickCount();
        static float deltaTime = 0;
        static const int numFramesDelay = 12;
        static int curFramesDelay = -1;

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

        if (g_pd3dDevice->BeginScene() >=0) {


            // Setup timestep
            float deltaTime = (float)(0.001*(double)(current_time - time));
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

            curFramesDelay = -1;
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

            // Rendering---------------------------------------------------------------------
            g_pd3dDevice->EndScene();

            if (gImGuiPreDrawGLSwapBuffersCallback) gImGuiPreDrawGLSwapBuffersCallback();
            g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
            if (gImGuiPostDrawGLSwapBuffersCallback) gImGuiPostDrawGLSwapBuffersCallback();
        }
        else g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        //--------------------------------------------------------------------------------

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
    //DestroyImGuiProgram();
    //DestroyImGuiBuffer();

    // New: delete cursors-------------------------------------------
    for (int i=0,isz=ImGuiMouseCursor_Count_+1;i<isz;i++) {
        //DestroyCursor(win32Cursors[i]);   // Nope: LoadCursor() loads SHARED cursors that should not be destroyed
    }
    //---------------------------------------------------------------

    // shutdown Direct3D
    ImImpl_InvalidateDeviceObjects();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (pD3D) pD3D->Release();
    UnregisterClass(_T("ImGuiApp"), wc.hInstance);

    return msg.wParam;
}


#endif //#ifndef IMIMPL_BINDING_H

