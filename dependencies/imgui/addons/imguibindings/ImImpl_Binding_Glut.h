#ifndef IMIMPL_BINDING_H
#define IMIMPL_BINDING_H

#include "imguibindings.h"

/*
#ifndef IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
#   ifdef _WIN32
#       define IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
#   endif //_WIN32
#endif //IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
*/

static ImVec2 mousePosScale(1.0f, 1.0f);
static const int specialCharMapAddend = 128;    // to prevent some special chars from clashing into ImGui normal chars

// NB: ImGui already provide OS clipboard support for Windows so this isn't needed if you are using Windows only.
#ifndef _WIN32
// If somebody implements these, in InitImGui(...) these callbacks MUST be set (they're currently detached).
// The default fallback on non-Windows OS is a "private" clipboard.
static const char* ImImpl_GetClipboardTextFn(void*)
{
    //fprintf(stderr,"ImImpl_GetClipboardTextFn()\n");
    static const char* txt = "copy and paste not implemented in the glut backend!";
    //return SDL_GetClipboardText();
    return txt;
}
static void ImImpl_SetClipboardTextFn(void*,const char* /*text*/)
{
    //fprintf(stderr,"ImImpl_SetClipboardTextFn()\n");
    //SDL_SetClipboardText(text);
}
#endif //_WIN32

// TODO: once we can extract the HWND from glut...
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

static void GlutReshapeFunc(int w,int h)    {
    int fb_w, fb_h;
    fb_w = w;
    fb_h = h;
    //glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = 1.f;//(float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = 1.f;//(float)fb_h / h;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);  // Display size, in pixels. For clamping windows positions.

    ResizeGL(w,h);
}
static void GlutEntryFunc(int a)   {
    //fprintf(stderr,"GlutEntryFunc %d\n",a);
    if (a==0){
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos.x = io.MousePos.y = -1;
    }
}
static bool gImGuiAppIconized = false;  // stays always false (I'm not able to detect when the user minimizes a window)
/*
// This method gets never called on my system:
static void GlutVisibilityFunc(int a)   {
    fprintf(stderr,"GlutVisibilityFunc %d\n",a);
    gImGuiAppIconized = a == GL_TRUE;
}
// This gets called, but does not provide the info I'm looking for
static void GlutWindowStatusFunc(int a)   {
    fprintf(stderr,"GlutWindowStatusFunc %d\n",a);
    //gImGuiAppIconized = a == GL_TRUE;
}*/
static inline void GlutSpecialUpDown(int key,int x,int y,bool down)   {

    ImGuiIO& io = ImGui::GetIO();

    const int mods = glutGetModifiers();
    io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;

    if (key>=GLUT_KEY_F1 && key<=GLUT_KEY_F12) {
        const int i = key-GLUT_KEY_F1;
        const bool prevState = gImGuiFunctionKeyDown[i];
        gImGuiFunctionKeyDown[i] = down;
        if (down!=prevState)    {
            if (down) gImGuiFunctionKeyPressed[i] = true;
            else gImGuiFunctionKeyReleased[i] = true;
        }
        //fprintf(stderr,"%d) D:%d P:%d R:%d\n",i,(int)gImGuiFunctionKeyDown[i],(int)gImGuiFunctionKeyPressed[i],(int)gImGuiFunctionKeyReleased[i]);
    }
    else if (key>=0 && key<512-specialCharMapAddend) io.KeysDown[key+specialCharMapAddend] = down;
}
static void GlutSpecial(int key,int x,int y)   {
    GlutSpecialUpDown(key,x,y,true);
}
static void GlutSpecialUp(int key,int x,int y)   {
    GlutSpecialUpDown(key,x,y,false);
}
static inline void GlutKeyboardUpDown(unsigned char key,int x,int y,bool down)   {

    ImGuiIO& io = ImGui::GetIO();

    const int mods = glutGetModifiers();
    io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;

    if ((int)key<512-specialCharMapAddend)   {
        io.KeysDown[key] = down;
        if (down) io.AddInputCharacter(key);
    }
}
static void GlutKeyboard(unsigned char key,int x,int y)   {
    GlutKeyboardUpDown(key,x,y,true);
}
static void GlutKeyboardUp(unsigned char key,int x,int y)   {
    GlutKeyboardUpDown(key,x,y,false);
}
static void GlutMouse(int b,int s,int x,int y)  {
    //fprintf(stderr,"GlutMouse(%d,%d,%d,%d);\n",b,s,x,y);
    ImGuiIO& io = ImGui::GetIO();
    const int mods = glutGetModifiers();
    io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;
    if (b>=0 && b<5)    {
        const int d = (b==1 ? 2 : b==2 ? 1 : b);
        io.MouseDown[d] = (s==0);
#       ifndef IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
        if (s==0)   io.MouseWheel = d==3 ? 1 : d==4 ? -1 : 0;
#       endif //IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
        // Manual double click handling:
        static double dblClickTimes[6]={-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX};  // seconds
        if (s == 0)   {
            double time = glutGet(GLUT_ELAPSED_TIME);
            double& oldTime = dblClickTimes[d];
            bool& mouseDoubleClicked = gImGuiBindingMouseDblClicked[b];
            if (time - oldTime < io.MouseDoubleClickTime*1000.f) {
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
}
#ifdef IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
static void GlutMouseWheel(int b,int s,int x,int y)  {
    //fprintf(stderr,"GlutMouseWheel(%d,%d,%d,%d);\n",b,s,x,y);
    ImGuiIO& io = ImGui::GetIO();
    const int mods = glutGetModifiers();
    io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;
    // NEVER TESTED !!!!!!!!
    if (s==0)   io.MouseWheel = b==0 ? 1 : b==1 ? -1 : 0;
}
#endif //IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK

static void GlutMotion(int x,int y)  {
    ImGuiIO& io = ImGui::GetIO();
    //const int mods = glutGetModifiers();
    //io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    //io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    //io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;
}
static void GlutPassiveMotion(int x,int y)  {
    ImGuiIO& io = ImGui::GetIO();
    //const int mods = glutGetModifiers();
    //io.KeyCtrl = (mods&GLUT_ACTIVE_CTRL) != 0;
    //io.KeyShift = (mods&GLUT_ACTIVE_SHIFT) != 0;
    //io.KeyAlt = (mods&GLUT_ACTIVE_ALT) != 0;
    io.MousePos.x = x;io.MousePos.y = y;
}
static void GlutDrawGL()    {
    ImGuiIO& io = ImGui::GetIO();    
    if (gImGuiAppIconized) WaitFor(1000);

    // Setup timestep
    static double time = 0.0f;
    const double current_time =  (double) glutGet(GLUT_ELAPSED_TIME)*0.001;
    static float deltaTime = (float)(current_time -time);
    deltaTime = (float) (current_time - time);
    time = current_time;
    if (deltaTime<=0) deltaTime = (1.0f/60.0f);

    // Start the frame
    io.DeltaTime = deltaTime;    
    if (!gImGuiPaused)	{
        static ImGuiMouseCursor oldCursor = ImGuiMouseCursor_Arrow;
        static bool oldMustHideCursor = io.MouseDrawCursor;
        if (oldMustHideCursor!=io.MouseDrawCursor) {
            glutSetCursor(GLUT_CURSOR_NONE);
            //glfwSetInputMode(window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
            oldMustHideCursor = io.MouseDrawCursor;
            oldCursor = ImGuiMouseCursor_Count_;
        }
        if (!io.MouseDrawCursor) {
            if (oldCursor!=ImGui::GetMouseCursor()) {
                oldCursor=ImGui::GetMouseCursor();
                static const int glutCursors[] = {
                    GLUT_CURSOR_INHERIT,
                    GLUT_CURSOR_TEXT,
                    GLUT_CURSOR_CROSSHAIR,      //ImGuiMouseCursor_Move,                  // Unused by ImGui
                    GLUT_CURSOR_UP_DOWN,    //ImGuiMouseCursor_ResizeNS,              // Unused by ImGui
                    GLUT_CURSOR_LEFT_RIGHT, //ImGuiMouseCursor_ResizeEW,              // Unused by ImGui
                    GLUT_CURSOR_TOP_RIGHT_CORNER,//ImGuiMouseCursor_ResizeNESW,
                    GLUT_CURSOR_BOTTOM_RIGHT_CORNER, //ImGuiMouseCursor_ResizeNWSE,          // Unused by ImGui
                    GLUT_CURSOR_INHERIT        //,ImGuiMouseCursor_Arrow
                };
                IM_ASSERT(sizeof(glutCursors)/sizeof(glutCursors[0])==ImGuiMouseCursor_Count_+1);
                glutSetCursor(glutCursors[oldCursor]);
            }
        }

        ImGui::NewFrame();
    }
    else {
        ImImpl_NewFramePaused();    // Enables some ImGui queries regardless ImGui::NewFrame() not being called.
        gImGuiCapturesInput = false;
    }
    for (size_t i = 0; i < 5; i++) io.MouseDoubleClicked[i]=gImGuiBindingMouseDblClicked[i];   // We manually set it (otherwise it won't work with low frame rates)

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
    glutSwapBuffers();
    if (gImGuiPostDrawGLSwapBuffersCallback) gImGuiPostDrawGLSwapBuffersCallback();

    if (!gImGuiPaused)	for (size_t i = 0; i < 5; i++) gImGuiBindingMouseDblClicked[i] = false;   // We manually set it (otherwise it won't work with low frame rates)

    // Reset additional special keys composed states (mandatory):
    for (int i=0;i<12;i++) {gImGuiFunctionKeyPressed[i] = gImGuiFunctionKeyReleased[i]= false;}

    // Handle clamped FPS:
    if (curFramesDelay>=0 && ++curFramesDelay>numFramesDelay) WaitFor(200);     // 200 = 5 FPS - frame rate when ImGui is inactive
    else {
        const float& inverseFPSClamp = gImGuiWereOutsideImGui ? gImGuiInverseFPSClampOutsideImGui : gImGuiInverseFPSClampInsideImGui;
        if (inverseFPSClamp==0.f) WaitFor(500);
        // If needed we must wait (gImGuiInverseFPSClamp-deltaTime) seconds (=> honestly I shouldn't add the * 2.0f factor at the end, but ImGui tells me the wrong FPS otherwise... why? <=)
        else if (inverseFPSClamp>0 && deltaTime < inverseFPSClamp)  WaitFor((unsigned int) ((inverseFPSClamp-deltaTime)*1000.f * 2.0f) );
    }

}
static void GlutIdle(void)  {
   glutPostRedisplay(); // TODO: Well, we could sleep a bit here if we detect that the window is minimized...
}
static void GlutFakeDrawGL() {
   glutDisplayFunc(GlutDrawGL);
}


static void InitImGui(const ImImpl_InitParams* pOptionalInitParams=NULL)    {
    //fprintf(stderr,"InitImGui(%d,%d);\n",w,h);
    //int w, h;
    int fb_w, fb_h;
    fb_w = glutGet(GLUT_WINDOW_WIDTH);
    fb_h = glutGet(GLUT_WINDOW_HEIGHT);
    //glfwGetFramebufferSize(window, &fb_w, &fb_h);
    mousePosScale.x = 1.f;//(float)fb_w / w;                  // Some screens e.g. Retina display have framebuffer size != from window size, and mouse inputs are given in window/screen coordinates.
    mousePosScale.y = 1.f;//(float)fb_h / h;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)fb_w, (float)fb_h);  // Display size, in pixels. For clamping windows positions.
    io.DeltaTime = 1.0f/60.0f;                          // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    //io.PixelCenterOffset = 0.0f;                        // Align OpenGL texels

    // Set up ImGui
    io.KeyMap[ImGuiKey_Tab] = 9;    // tab (ascii)

    // These are returned from glutSpecial:----
    io.KeyMap[ImGuiKey_LeftArrow] =     specialCharMapAddend + GLUT_KEY_LEFT;    // Left
    io.KeyMap[ImGuiKey_RightArrow] =    specialCharMapAddend + GLUT_KEY_RIGHT;   // Right
    io.KeyMap[ImGuiKey_UpArrow] =       specialCharMapAddend + GLUT_KEY_UP;      // Up
    io.KeyMap[ImGuiKey_DownArrow] =     specialCharMapAddend + GLUT_KEY_DOWN;    // Down
    io.KeyMap[ImGuiKey_PageUp] =        specialCharMapAddend + GLUT_KEY_PAGE_UP;    // Prior
    io.KeyMap[ImGuiKey_PageDown] =      specialCharMapAddend + GLUT_KEY_PAGE_DOWN;  // Next
    io.KeyMap[ImGuiKey_Home] =          specialCharMapAddend + GLUT_KEY_HOME;    // Home
    io.KeyMap[ImGuiKey_End] =           specialCharMapAddend + GLUT_KEY_END;     // End
    // -----------------------------------------

    io.KeyMap[ImGuiKey_Delete] =    127;      // Delete  (ascii) (0x006F)
    io.KeyMap[ImGuiKey_Backspace] = 8;        // Backspace  (ascii)
    io.KeyMap[ImGuiKey_Enter] = 13;           // Enter  (ascii)
    io.KeyMap[ImGuiKey_Escape] = 27;          // Escape  (ascii)
    io.KeyMap[ImGuiKey_A] = 1;
    io.KeyMap[ImGuiKey_C] = 3;
    io.KeyMap[ImGuiKey_V] = 22;
    io.KeyMap[ImGuiKey_X] = 24;
    io.KeyMap[ImGuiKey_Y] = 25;
    io.KeyMap[ImGuiKey_Z] = 26;

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
#ifndef _WIN32
    //io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
    //io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;
#endif
#ifdef _WIN32
    //io.ImeSetInputScreenPosFn = ImImpl_ImeSetInputScreenPosFn;
#endif

    // 3 common init steps
    InitImGuiFontTexture(pOptionalInitParams);
    InitImGuiProgram();
    InitImGuiBuffer();
}

static bool InitBinding(const ImImpl_InitParams* pOptionalInitParams=NULL,int argc=0, char** argv=NULL)	{

//-ENABLE-OPENGLES COMPATIBILITY PROFILES----------------------------------
/* // Don't know how to make this work... for sure,
 * // "custom" freeglut compiled with -legl -lglesv2 can do it,
 * // but otherwie it seems that the required profile definition is missing ATM.
 * // Moreover, I'm not sure that the glutInitContextMethods (defined in freeglut_ext.h)
 * // can be called at this point...
*/
/*
#ifndef IMIMPL_SHADER_NONE
#ifdef IMIMPL_SHADER_GLES
#   ifndef IMIMPL_SHADER_GL3
    glutInitContextVersion( 2, 0);
#   else //IMIMPL_SHADER_GL3
    glutInitContextVersion( 3, 0);
#   endif //MIMPL_SHADER_GL3
    //glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);                          // GLFW3 can do it!
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);   // SDL2 can do it too!

    // What about freeglut?
    glutInitContextProfile( GLUT_ES_PROFILE ); //GLUT_CORE_PROFILE GLUT_COMPATIBILITY_PROFILE
    //glutInitContextVersion( int majorVersion, int minorVersion );
    //glutInitContextFlags( int flags );  //GLUT_DEBUG  GLUT_FORWARD_COMPATIBLE
    //glutInitContextProfile( int profile ); //GLUT_CORE_PROFILE GLUT_COMPATIBILITY_PROFILE
#endif // IMIMPL_SHADER_GLES
#endif //IMIMPL_SHADER_NONE
*/
//--------------------------------------------------------------------------


    glutInitDisplayMode(GLUT_RGB | GLUT_ALPHA | GLUT_DEPTH | GLUT_DOUBLE);// | GLUT_MULTISAMPLE | GLUT_SRGB); // The latter is defined in freeglut_ext.h
    glutInitWindowSize(pOptionalInitParams ? pOptionalInitParams->gWindowSize.x : 1270, pOptionalInitParams ? pOptionalInitParams->gWindowSize.y : 720);
    //int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
    //glutInitWindowPosition(5 * screenWidth/ 12, 0);
    glutInit(&argc, argv);
    if (!glutCreateWindow((pOptionalInitParams && pOptionalInitParams->gWindowTitle[0]!='\0') ? (const char*) &pOptionalInitParams->gWindowTitle[0] : "ImGui Glut OpenGL example"))
    {
        fprintf(stderr, "Could not call glutCreateWindow(...) successfully.\n");
        return false;
    }

#ifdef IMGUI_USE_GLAD
   if(!gladLoadGL()) {
        fprintf(stderr,"Error initializing GLAD!\n");
        return false;
    }
    // gladLoadGLLoader(&glutGetProcAddress);
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


    // Set up glut callbacks
    glutIdleFunc(GlutIdle);
    glutReshapeFunc(GlutReshapeFunc);
    //glutDisplayFunc(GlutDrawGL);

    //glutInitContextFunc(InitGL);   // freeglut3 only: call order:  ResizeGL() - InitGL() - DrawGL()
    //InitGL();                      // call order:                  InitGL() - DrawGL() - ResizeGL()

    //But I prefer call order: InitGL() - ResizeGL() - DrawGL():
    // this can be achieved by skipping the first DrawGL call this way:
    glutDisplayFunc(GlutFakeDrawGL);

    glutKeyboardFunc(GlutKeyboard);
    glutKeyboardUpFunc(GlutKeyboardUp);
    glutSpecialFunc(GlutSpecial);
    glutSpecialUpFunc(GlutSpecialUp);
    glutMouseFunc(GlutMouse);               // This handles mouse wheel as well on non-Windows systems
    glutMotionFunc(GlutMotion);
    glutPassiveMotionFunc(GlutPassiveMotion);

#ifndef __EMSCRIPTEN__
    glutEntryFunc(GlutEntryFunc);
#endif //__EMSCRIPTEN__

    // Apparently, there's no way to detect when the user minimizes a window using glut (at least on my system these callbacks don't provide this info)
    //glutVisibilityFunc(GlutVisibilityFunc);       // never called
    //glutWindowStatusFunc(GlutWindowStatusFunc);   // called on resizing too

#ifdef IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK
    glutMouseWheelFunc(GlutMouseWheel);
#endif //IMIMPL_GLUT_HAS_MOUSE_WHEEL_CALLBACK

	return true;
}

/*
#	ifdef __EMSCRIPTEN__
static void ImImplMainLoopFrame()	{
    ImGuiIO& io = ImGui::GetIO();
    // TODO:
}
#   endif //__EMSCRIPTEN__
*/

// Application code
int ImImpl_Main(const ImImpl_InitParams* pOptionalInitParams,int argc, char** argv)
{
    if (!InitBinding(pOptionalInitParams,argc,argv)) return -1;	
    InitImGui(pOptionalInitParams);
    ImGuiIO& io = ImGui::GetIO();        

    gImGuiInverseFPSClampInsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampInsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampInsideImGui) : 1.0f) : -1.0f;
    gImGuiInverseFPSClampOutsideImGui = pOptionalInitParams ? ((pOptionalInitParams->gFpsClampOutsideImGui!=0) ? (1.0f/pOptionalInitParams->gFpsClampOutsideImGui) : 1.0f) : -1.0f;
    gImGuiDynamicFPSInsideImGui = pOptionalInitParams ? pOptionalInitParams->gFpsDynamicInsideImGui : false;

    InitGL();
    if (gImGuiPostInitGLCallback) gImGuiPostInitGLCallback();
    ResizeGL((int)io.DisplaySize.x,(int)io.DisplaySize.y);
	
#   ifdef __FREEGLUT_EXT_H__
    glutSetOption ( GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION ) ;
#   endif //__FREEGLUT_STD_H__

//#	ifdef __EMSCRIPTEN__
//    emscripten_set_main_loop(glutMainLoop, 0, 1);
//#	else
    glutMainLoop();     // GLUT has its own main loop, which calls display();
//#   endif //__EMSCRIPTEN__



    ImGui::Shutdown();
    DestroyGL();
    DestroyImGuiFontTexture();
    DestroyImGuiProgram();
    DestroyImGuiBuffer();

    return 0;
}

#endif //#ifndef IMIMPL_BINDING_H

