
#include "pch.h"

#include "imgui.h"
#include "imguiRenderGL.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "InputGeom.h"
#include "TestCase.h"

#include "Sample_TileMesh.h"
#include "Sample_Debug.h"

#undef main

class Interface
{
public:
	Interface();
	~Interface();

	int ShowDialog(const char* defaultZone = NULL);
	void Halt();
	void StartBuild();

	void LoadGeom(BuildContext* ctx, char* zoneShortName);

private:
	void DefineDirectories(bool bypassSettings = false);
};


#define snprintf _snprintf
#define putenv _putenv

BuildContext ctx;

static Sample_TileMesh* sample;
static HANDLE handle;
static char* output_path;
static char* message;
static char* everquest_path;
static bool resetCamera;
static bool lockRendering = false;
static InputGeom* geom;
static float progress = 0;
static char* buffer = new char[240];

struct FileList
{
	static const int MAX_FILES = 2048;
	inline FileList() : size(0) {}
	inline ~FileList()
	{
		clear();
	}

	void clear()
	{
		for (int i = 0; i < size; ++i)
			delete[] files[i];
		size = 0;
	}

	void add(const char* path)
	{
		if (size >= MAX_FILES)
			return;
		int n = strlen(path);
		files[size] = new char[n + 1];
		strcpy(files[size], path);
		size++;
	}

	static int cmp(const void* a, const void* b)
	{
		return strcmp(*(const char**)a, *(const char**)b);
	}

	void sort()
	{
		if (size > 1)
			qsort(files, size, sizeof(char*), cmp);
	}

	char* files[MAX_FILES];
	int size;
};

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	// If the BFFM_INITIALIZED message is received
	// set the path to the start path.
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
	{
		if (NULL != lpData)
		{
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		}
	}
	}

	return 0; // The function should always return 0.
}

// HWND is the parent window.
// szCurrent is an optional start folder. Can be NULL.
// szPath receives the selected path on success. Must be MAX_PATH characters in length.
BOOL BrowseForFolder(HWND hwnd, LPCTSTR szCurrent, LPTSTR szPath, LPCSTR szCaption)
{
	printf("%s\n", szCaption);
	BROWSEINFO   bi = { 0 };
	LPITEMIDLIST pidl;
	TCHAR        szDisplay[MAX_PATH];
	BOOL         retval;

	CoInitialize(NULL);

	bi.hwndOwner = hwnd;
	bi.pszDisplayName = szDisplay;
	bi.lpszTitle = szCaption;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM)szCurrent;

	pidl = SHBrowseForFolder(&bi);

	if (NULL != pidl)
	{
		retval = SHGetPathFromIDList(pidl, szPath);
		CoTaskMemFree(pidl);
	}
	else
	{
		retval = FALSE;
	}

	if (!retval)
	{
		szPath[0] = TEXT('\0');
	}

	CoUninitialize();
	return retval;
}

int LastIndexOf(TCHAR *text, TCHAR key) {
	int index = -1;
	for (unsigned int x = 0; x < _tcslen(text); x++)
		if (text[x] == key) index = x;
	return index + 1;
}

int contains(char* source, char key) {
	for (unsigned int i = 0; i < strlen(source); i++)
		if (source[i] == key) return i;
	return 0;
}

char* wtoc(char* Dest, TCHAR* Source, int SourceSize)
{
	for (int i = 0; i < SourceSize; ++i)
		Dest[i] = (char)Source[i];
	return Dest;
}

char* findEQPath(bool bypassSettings)
{
	LPTSTR path = new TCHAR[MAX_PATH];
	TCHAR *buffer = new TCHAR[MAX_PATH];
	char *cPath = new char[MAX_PATH];
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("EQNavigation.ini"));

	if (!GetPrivateProfileString("General", "EverQuest Path", "", path, MAX_PATH, FullPath) || bypassSettings)
	{
		if (BrowseForFolder(NULL, "C:\\Program Files\\Sony\\EverQuest", path, "Select EverQuest Folder"))
		{
			WIN32_FIND_DATA FindFileData;
			HANDLE hFind;
			TCHAR *pathstr = new TCHAR[MAX_PATH];
			wsprintf(pathstr, TEXT("%s\\eqgame.exe"), path);
			hFind = FindFirstFile(pathstr, &FindFileData);
			if (hFind != INVALID_HANDLE_VALUE)
				FindClose(hFind);
			else
			{
				TCHAR *message = new TCHAR[MAX_PATH];
				wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), path);
				if (MessageBox(NULL, message,
					"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
					return findEQPath(bypassSettings);
			}
			/*
			memcpy(buffer,path+ LastIndexOf(path,'\\'),MAX_PATH);
			if(_tcscmp(buffer,"EverQuest")) {
			TCHAR *message = new TCHAR[MAX_PATH];
			wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), buffer);
			if (MessageBox(NULL,message,
			"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
			return findEQPath(bypassSettings);
			}
			*/
			WritePrivateProfileString("General", "EverQuest Path", path, FullPath);
		}
		else
			return NULL;
	}
	return wtoc(cPath, path, MAX_PATH);
}

char* findMQPath(bool bypassSettings)
{
	LPTSTR path = new TCHAR[MAX_PATH];
	TCHAR *buffer = new TCHAR[MAX_PATH];
	char *cPath = new char[MAX_PATH];
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("EQNavigation.ini"));
	if (!GetPrivateProfileString("General", "Output Path", "", path, MAX_PATH, FullPath) || bypassSettings) {
		if (BrowseForFolder(NULL, "C:\\", path, "Select Mesh Folder"))
		{
			/*
			memcpy(buffer,path+ LastIndexOf(path,'\\'),MAX_PATH);
			if(_tcscmp(buffer,"Release")) {
			TCHAR *message = new TCHAR[MAX_PATH];
			wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), buffer);
			if (MessageBox(NULL,message,
			"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
			return findMQPath(bypassSettings);
			}
			*/
			WritePrivateProfileString("General", "Output Path", path, FullPath);
		}
		else
			return NULL;
	}
	return wtoc(cPath, path, MAX_PATH);
}

static char* parseRow(char* buf, char* bufEnd, char* row, int len)
{
	bool cont = false;
	bool start = true;
	bool done = false;
	int n = 0;
	while (!done && buf < bufEnd)
	{
		char c = *buf;
		buf++;
		// multirow
		switch (c)
		{
		case '\\':
			cont = true; // multirow
			break;
		case '\n':
			if (start) break;
			done = true;
			break;
		case '\r':
			break;
		case '\t':
		default:
			start = false;
			cont = false;
			row[n++] = c;
			if (n >= len - 1)
				done = true;
			break;
		}
	}
	row[n] = '\0';
	return buf;
}
char* Mid(char * dest, const char * src, int start, int length) {
	strcpy(dest, src + start);
	dest[length] = 0;
	return dest;
}

void loadZones(FileList& list)
{
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("Zones.ini"));
	list.clear();
	if (FILE* fp = fopen(FullPath, "rb")) {
		fseek(fp, 0, SEEK_END);
		int bufSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char* buf = new char[bufSize];
		if (!buf)
		{
			fclose(fp);
			return;
		}
		fread(buf, bufSize, 1, fp);
		fclose(fp);
		int index;
		char* buffer = new char[256];
		char* src = buf;
		char* srcEnd = buf + bufSize;
		char row[512];
		char* charValue1 = new char[256];
		while (src < srcEnd)
		{
			// Parse one row
			row[0] = '\0';
			src = parseRow(src, srcEnd, row, sizeof(row) / sizeof(char));
			// Skip comments
			if (row[0] == ';') continue;
			if (row[0] == '[' && row[strlen(row) - 1] == ']' && row[1] != 't')
			{
				// section name
			}
			else if (index = contains(row, '=')) {
				list.add(Mid(charValue1, row, 0, index));
			}
		}
		delete[] buf;
		list.sort();
	}
	else
	{
		printf("Zones.ini not found");
	}
}
void Interface::DefineDirectories(bool bypassSettings)
{
	everquest_path = new char[MAX_PATH];
	//if (char* buffer = findEQPath(bypassSettings))
	//	strcpy(everquest_path, buffer);
	strcpy(everquest_path, "C:\\projects\\eq-scripts\\equpdate\\build_folder");

	output_path = new char[MAX_PATH];
	//if (char* buffer = findMQPath(bypassSettings))
	//	strcpy(output_path, buffer);
	strcpy(output_path, "c:\\projects\\mq2-mmobugs\\Debug");
	sample->setOutputPath(output_path);

}

Interface::Interface()
{
	resetCamera = true;
	message = new char[256];
	geom = 0;
	sample = new Sample_TileMesh();
	sample->setContext(&ctx);
	DefineDirectories();
}

Interface::~Interface()
{
}

DWORD WINAPI BuildThread(LPVOID lpParam)
{
	if (sample)
		sample->handleBuild();

	return NULL;
}

void Interface::StartBuild()
{
	Halt();
	handle = CreateThread(NULL, 0, &BuildThread, 0, 0, NULL);
}

struct LoadThreadParams
{
	BuildContext* ctx;
	char* zoneShortName;
};

DWORD WINAPI LoadThread(LPVOID lpParam)
{
	lockRendering = true;

	LoadThreadParams* params = (LoadThreadParams*)lpParam;

	geom = new InputGeom();
	geom->loadMesh(params->ctx, params->zoneShortName, everquest_path);

	if (sample && geom)
	{
		sample->handleMeshChanged(geom);
		resetCamera = true;
	}

	delete params;
	lockRendering = false;

	return NULL;
}

void Interface::LoadGeom(BuildContext* ctx, char* zoneShortName)
{
	Halt();

	LoadThreadParams* params = new LoadThreadParams;
	params->ctx = ctx;
	params->zoneShortName = zoneShortName;

	handle = CreateThread(NULL, 0, LoadThread, params, 0, NULL);
}

void Interface::Halt()
{
	message = NULL;
	if (handle)
	{
		DWORD exitCode = 0;
		GetExitCodeThread(handle, &exitCode);
		TerminateThread(handle, exitCode);
	}

	if (lockRendering)
	{
		delete sample;
		delete geom;
		sample = new Sample_TileMesh();
		sample->setContext(&ctx);
		geom = new InputGeom();
		lockRendering = false;
	}

	handle = NULL;
}

int Interface::ShowDialog(const char* defaultZone)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialise SDL\n");
		return -1;
	}

	// Center window
	char env[] = "SDL_VIDEO_CENTERED=1";
	putenv(env);

	// Init OpenGL
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	//#ifndef WIN32
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	//#endif

	const SDL_VideoInfo* vi = SDL_GetVideoInfo();

	int width = vi->current_w - 200;
	int height = vi->current_h - 200;
	SDL_Surface* screen = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | SDL_RESIZABLE);

	if (!screen)
	{
		printf("Could not initialise SDL opengl\n");
		return -1;
	}

	SDL_SysWMinfo i;
	SDL_VERSION(&i.version);
	if (SDL_GetWMInfo(&i))
	{
		HWND hwnd = i.window;
		SetWindowPos(hwnd, HWND_TOP, 0, 0, width, height, NULL);
		
	}
	SDL_WM_SetCaption("EQ Navigation", 0);

	if (!imguiRenderGLInit("DroidSans.ttf"))
	{
		printf("Could not init GUI renderer. Likely missing the 'DroidSans.tff' file.\n");
		SDL_Quit();
		return -1;
	}

	float t = 0.0f;
	Uint32 lastTime = SDL_GetTicks();
	int mx = 0, my = 0;
	float rx = 45;
	float ry = -45;
	float moveW = 0, moveS = 0, moveA = 0, moveD = 0;
	float camx = 0, camy = 0, camz = 0, camr = 10;
	float origrx = 0, origry = 0;
	int origx = 0, origy = 0;
	bool rotate = false;
	float rays[3], raye[3];
	bool mouseOverMenu = false;
	bool showMenu = true;
	bool showLog = false;
	bool showDebugMode = false;
	bool showTools = false;
	bool showLevels = defaultZone != 0;
	bool showSample = true;
	bool showTestCases = false;
	bool showPreview = true;

	int propScroll = 0;
	int logScroll = 0;
	int toolsScroll = 0;
	int debugScroll = 0;

	FileList files;
	const char msg[] = "Hotkey List -> W/S/A/D: Move  RMB: Rotate   LMB: Place Start   LMB+SHIFT: Place End   TAB: Redefine EQ & MQ2 Directories";
	char meshName[128] = "Choose Zone...";
	char* meshShortName = new char[MAX_PATH];

	float mpos[3] = { 0, 0, 0 };
	bool mposSet = false;

	glEnable(GL_CULL_FACE);

	float fogCol[4] = { 0.32f, 0.25f, 0.25f, 1 };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0);
	glFogf(GL_FOG_END, 10);
	glFogfv(GL_FOG_COLOR, fogCol);

	glDepthFunc(GL_LEQUAL);

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);

	bool done = false;
	while (!done)
	{
		// Handle input events.
		int mscroll = 0;
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_VIDEORESIZE:
				width = event.resize.w;
				height = event.resize.h;
				break;
			case SDL_KEYDOWN:
				// Handle any key presses here.
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					//HaltBuild();
					done = true;
				}
				else if (event.key.keysym.sym == SDLK_TAB)
					DefineDirectories(true);
				break;

			case SDL_MOUSEBUTTONDOWN:
				// Handle mouse clicks here.
				if (!mouseOverMenu)
				{
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						// Rotate view
						rotate = true;
						origx = mx;
						origy = my;
						origrx = rx;
						origry = ry;
					}
					else if (event.button.button == SDL_BUTTON_LEFT)
					{
						// Hit test mesh.
						if (geom && sample)
						{
							// Hit test mesh.
							float t;
							if (geom->raycastMesh(rays, raye, t))
							{
								if (SDL_GetModState() & KMOD_CTRL)
								{
									mposSet = true;
									mpos[0] = rays[0] + (raye[0] - rays[0])*t;
									mpos[1] = rays[1] + (raye[1] - rays[1])*t;
									mpos[2] = rays[2] + (raye[2] - rays[2])*t;
								}
								else
								{
									float pos[3];
									pos[0] = rays[0] + (raye[0] - rays[0])*t;
									pos[1] = rays[1] + (raye[1] - rays[1])*t;
									pos[2] = rays[2] + (raye[2] - rays[2])*t;
									bool shift = (SDL_GetModState() & KMOD_SHIFT) ? true : false;
									if (!message) sample->handleClick(rays, pos, shift);
								}
							}
							else
							{
								if (SDL_GetModState() & KMOD_CTRL)
								{
									mposSet = false;
								}
							}
						}
					}
				}
				if (event.button.button == SDL_BUTTON_WHEELUP)
					mscroll--;
				if (event.button.button == SDL_BUTTON_WHEELDOWN)
					mscroll++;
				break;

			case SDL_MOUSEBUTTONUP:
				// Handle mouse clicks here.
				if (event.button.button == SDL_BUTTON_RIGHT)
				{
					rotate = false;
				}
				break;

			case SDL_MOUSEMOTION:
				mx = event.motion.x;
				my = height - 1 - event.motion.y;
				if (rotate)
				{
					int dx = mx - origx;
					int dy = my - origy;
					rx = origrx - dy*0.25f;
					ry = origry + dx*0.25f;
				}
				break;

			case SDL_QUIT:
				Halt();
				done = true;
				break;

			default:
				break;
			}
		}

		unsigned char mbut = 0;
		if (SDL_GetMouseState(0, 0) & SDL_BUTTON_LMASK)
			mbut |= IMGUI_MBUT_LEFT;
		if (SDL_GetMouseState(0, 0) & SDL_BUTTON_RMASK)
			mbut |= IMGUI_MBUT_RIGHT;

		Uint32	time = SDL_GetTicks();
		float	dt = (time - lastTime) / 1000.0f;
		lastTime = time;

		t += dt;

		// Update and render
		glViewport(0, 0, width, height);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);

		// Render 3d
		glEnable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(50.0f, (float)width / (float)height, 1.0f, camr);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(rx, 1, 0, 0);
		glRotatef(ry, 0, 1, 0);
		glTranslatef(-camx, -camy, -camz);

		// Get hit ray position and direction.
		GLdouble proj[16];
		GLdouble model[16];
		GLint view[4];
		glGetDoublev(GL_PROJECTION_MATRIX, proj);
		glGetDoublev(GL_MODELVIEW_MATRIX, model);
		glGetIntegerv(GL_VIEWPORT, view);
		GLdouble x, y, z;
		if (showPreview && sample && !lockRendering) {
			gluUnProject(mx, my, 0.0f, model, proj, view, &x, &y, &z);
			rays[0] = (float)x; rays[1] = (float)y; rays[2] = (float)z;
			gluUnProject(mx, my, 1.0f, model, proj, view, &x, &y, &z);
			raye[0] = (float)x; raye[1] = (float)y; raye[2] = (float)z;

			// Handle keyboard movement.
			Uint8* keystate = SDL_GetKeyState(NULL);
			moveW = rcClamp(moveW + dt * 4 * (keystate[SDLK_w] ? 1 : -1), 0.0f, 1.0f);
			moveS = rcClamp(moveS + dt * 4 * (keystate[SDLK_s] ? 1 : -1), 0.0f, 1.0f);
			moveA = rcClamp(moveA + dt * 4 * (keystate[SDLK_a] ? 1 : -1), 0.0f, 1.0f);
			moveD = rcClamp(moveD + dt * 4 * (keystate[SDLK_d] ? 1 : -1), 0.0f, 1.0f);

			float keybSpeed = 30.0f;
			if (SDL_GetModState() & KMOD_SHIFT)
				keybSpeed *= 10.0f;
			if (SDL_GetModState() & KMOD_ALT)
				keybSpeed *= 10.0f;

			float movex = (moveD - moveA) * keybSpeed * dt;
			float movey = (moveS - moveW) * keybSpeed * dt;

			camx += movex * (float)model[0];
			camy += movex * (float)model[4];
			camz += movex * (float)model[8];

			camx += movey * (float)model[2];
			camy += movey * (float)model[6];
			camz += movey * (float)model[10];

			glEnable(GL_FOG);
			sample->handleRender();
			glDisable(GL_FOG);
		}

		// Render GUI
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0, width, 0, height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		mouseOverMenu = false;

		imguiBeginFrame(mx, my, mbut, mscroll);
		if (sample)
		{
			sample->handleRenderOverlay((double*)proj, (double*)model, (int*)view);
		}

		// Help text.
		if (showMenu)
		{
			imguiDrawText(width / 2, height - 20, IMGUI_ALIGN_CENTER, msg, imguiRGBA(255, 255, 255, 128));
		}
		if (message)
		{
			sprintf(buffer, message, progress);
			imguiDrawText(width / 2, height / 2, IMGUI_ALIGN_CENTER, buffer, imguiRGBA(255, 255, 255, 128));
		}
		if (showMenu)
		{
			int propDiv = showDebugMode ? (int)(height*0.6f) : height;

			if (imguiBeginScrollArea("Properties",
				width - 250 - 10, 10 + height - propDiv, 250, propDiv - 20, &propScroll))
				mouseOverMenu = true;

			if (imguiCheck("Show Log", showLog))
				showLog = !showLog;
			if (imguiCheck("Show Tools", showTools))
				showTools = !showTools;
			if (imguiCheck("Show Preview", showPreview))
				showPreview = !showPreview;
			/*if (imguiCheck("Show Debug Mode", showDebugMode))
			showDebugMode = !showDebugMode;*/

			imguiSeparator();

			if (sample)
			{
				imguiSeparator();
				imguiLabel("Zone Mesh");
				if (imguiButton(meshName))
				{
					if (showLevels)
					{
						showLevels = false;
					}
					else
					{
						showSample = false;
						showTestCases = false;
						showLevels = true;
						loadZones(files);
					}
				}
				/*if (geom)
				{
				char text[64];
				snprintf(text, 64, "Verts: %.1fk  Tris: %.1fk",
				geom->getMesh()->getVertCount()/1000.0f,
				geom->getMesh()->getTriCount()/1000.0f);
				imguiValue(text);
				}
				imguiSeparator();*/
			}


			if (geom && sample)
			{
				if (message) {
					imguiSeparator();
					if (imguiButton("Halt Action"))
						Halt();
				}
				else {
					sample->handleSettings();
					if (imguiButton("Build"))
						StartBuild();
					imguiSeparator();
				}
			}


			imguiEndScrollArea();

			if (showDebugMode)
			{
				if (imguiBeginScrollArea("Debug Mode",
					width - 250 - 10, 10,
					250, height - propDiv - 10, &debugScroll))
					mouseOverMenu = true;

				if (sample)
					sample->handleDebugMode();

				imguiEndScrollArea();
			}
		}

		// Level selection dialog.
		if (showLevels && !lockRendering)
		{
			static int levelScroll = 0;
			if (imguiBeginScrollArea("Choose Zone", width - 10 - 300 - 10 - 200, height - 10 - 800, 250, 800, &levelScroll))
				mouseOverMenu = true;

			int levelToLoad = -1;
			for (int i = 0; i < files.size; ++i)
			{
				if (imguiItem(files.files[i]))
					levelToLoad = i;
			}
			if (levelToLoad != -1 || defaultZone) {
				delete geom;
				geom = 0;
				showLevels = false;
				if (levelToLoad != -1)
				{
					strncpy(meshName, files.files[levelToLoad], sizeof(meshName));
					meshName[sizeof(meshName) - 1] = '\0';

					TCHAR FullPath[MAX_PATH] = { 0 };
					GetModuleFileName(NULL, FullPath, MAX_PATH);
					PathRemoveFileSpec(FullPath);
					PathAppend(FullPath, _T("Zones.ini"));
					if (FILE* fp = fopen(FullPath, "rb")) {
						fseek(fp, 0, SEEK_END);
						int bufSize = ftell(fp);
						fseek(fp, 0, SEEK_SET);
						char* buf = new char[bufSize];
						if (buf) {
							fread(buf, bufSize, 1, fp);
							fclose(fp);
							int index;
							char* buffer = new char[256];
							char* src = buf;
							char* srcEnd = buf + bufSize;
							char row[512];
							char* charValue1 = new char[256];
							while (src < srcEnd)
							{
								// Parse one row
								row[0] = '\0';
								src = parseRow(src, srcEnd, row, sizeof(row) / sizeof(char));
								// Skip comments
								if (row[0] == ';') continue;
								if (index = contains(row, '=')) {
									if (!strcmp(Mid(charValue1, row, 0, index), meshName)){
										strcpy(meshShortName, Mid(charValue1, row, index + 1, strlen(row)));
									}
								}
							}
							delete[] buf;
						}
						else
							fclose(fp);
					}
					else {
						printf("Zones.ini not found");
					}
				}
				else {
					strcpy(meshShortName, defaultZone);
					defaultZone = 0;
				}
				LoadGeom(&ctx, meshShortName);
			}
			imguiEndScrollArea();
		}

		// Camera Reset
		if (geom && resetCamera)
		{
			const float* bmin = geom->getMeshBoundsMin();
			const float* bmax = geom->getMeshBoundsMax();
			// Reset camera and fog to match the mesh bounds.
			camr = sqrtf(rcSqr(bmax[0] - bmin[0]) +
				rcSqr(bmax[1] - bmin[1]) +
				rcSqr(bmax[2] - bmin[2])) / 2;
			camx = (bmax[0] + bmin[0]) / 2 + camr;
			camy = (bmax[1] + bmin[1]) / 2 + camr;
			camz = (bmax[2] + bmin[2]) / 2 + camr;
			camr *= 3;
			rx = 45;
			ry = -45;
			glFogf(GL_FOG_START, camr*0.2f);
			glFogf(GL_FOG_END, camr*1.25f);
			resetCamera = false;
		}

		// Log
		if (showLog && showMenu)
		{
			if (imguiBeginScrollArea("Log", 10, 10, width - 300, 200, &logScroll))
				mouseOverMenu = true;
			for (int i = 0; i < ctx.getLogCount(); ++i)
				imguiLabel(ctx.getLogText(i));
			imguiEndScrollArea();
		}

		// Tools
		if (!showTestCases && showTools && showMenu && geom && sample)
		{
			if (imguiBeginScrollArea("Tools", 10, height - 10 - 350, 200, 350, &toolsScroll))
				mouseOverMenu = true;

			sample->handleTools();

			imguiEndScrollArea();
		}

		imguiEndFrame();
		imguiRenderGLDraw();

		glEnable(GL_DEPTH_TEST);
		SDL_GL_SwapBuffers();
	}

	imguiRenderGLDestroy();
	Halt();
	SDL_Quit();

	delete sample;
	delete geom;
	return true;
}

int main(int argc, char* argv[])
{
	Interface window;
	return window.ShowDialog();
}

//----------------------------------------------------------------------------
bool _DEBUG_LOG = false;                    /* generate debug messages      */
char _DEBUG_LOG_FILE[260];                  /* log file path        */
bool debug_log_first_start = TRUE;
char  log_file_name[256];                   /* file name for the log file   */
DWORD debug_start_time;                     /* starttime of debug           */
BOOL  debug_started = FALSE;                /* debug_log writes only if     */

/* add a line to the log file                                               */
void debug_log_proc(char* text, char* sourcefile, int sourceline)
{
	FILE* fp;
	if (!_DEBUG_LOG)
		return;

	if (debug_started == FALSE)
		return;        /* exit if not initialized      */

	if ((fp = fopen(log_file_name, "at")) != NULL) /* append to logfile  */
	{
		fprintf(fp, " %8u ms (line %u in %s):\n              - %s\n",
			(unsigned int)(GetTickCount() - debug_start_time),
			sourceline, sourcefile, text);
		fclose(fp);
	}
}
