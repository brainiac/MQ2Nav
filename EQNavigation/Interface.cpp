// Implementation of main GUI interface for EQNavigation

#include "pch.h"
#include "Interface.h"

#include "imgui.h"
#include "imguiRenderGL.h"

#include "RecastDebugDraw.h"
#include "InputGeom.h"
#include "Sample_TileMesh.h"
#include "Sample_Debug.h"

#include <stdarg.h>
#include <stdio.h>

#include <sstream>

//============================================================================

static const int32_t MAX_LOG_MESSAGES = 1000;

//============================================================================

Interface::Interface(const std::string& defaultZone)
	: m_context(new BuildContext())
	, m_mesh(new Sample_TileMesh)
	, m_screen(nullptr)
	, m_resetCamera(true)
	, m_defaultZone(defaultZone)
	, m_width(1600), m_height(900)
	, m_progress(0.0)
	, m_mouseOverMenu(false)
	, m_showDebugMode(false)
	, m_showPreview(true)
	, m_showLog(false)
	, m_showTools(false)
	, m_showLevels(!defaultZone.empty())
	, m_showSample(true)
	, m_showTestCases(false)
{

	m_mesh->setContext(m_context.get());
	m_mesh->setOutputPath(m_eqConfig.GetOutputPath().c_str());

	InitializeWindow();
}

Interface::~Interface()
{
	DestroyWindow();
}

bool Interface::InitializeWindow()
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialise SDL\n");
		return false;
	}

	// Center window
	char env[] = "SDL_VIDEO_CENTERED=1";
	_putenv(env);

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

	m_width = vi->current_w - 200;
	m_height = vi->current_h - 200;
	m_screen = SDL_SetVideoMode(m_width, m_height, 0, SDL_OPENGL | SDL_RESIZABLE);

	if (!m_screen)
	{
		printf("Could not initialise SDL opengl\n");
		return false;
	}

	SDL_SysWMinfo i;
	SDL_VERSION(&i.version);
	if (SDL_GetWMInfo(&i))
	{
		HWND hwnd = i.window;
		SetWindowPos(hwnd, HWND_TOP, 0, 0, m_width, m_height, NULL);
	}

	SDL_WM_SetCaption("EQ Navigation", 0);

	if (!imguiRenderGLInit("DroidSans.ttf"))
	{
		printf("Could not init GUI renderer. Likely missing the 'DroidSans.tff' file.\n");
		return false;
	}

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

	return true;
}

void Interface::DestroyWindow()
{
	imguiRenderGLDestroy();

	if (m_screen)
	{
		SDL_FreeSurface(m_screen);
		m_screen = nullptr;
	}

	SDL_Quit();
}

int Interface::RunMainLoop()
{
	m_time = 0.0f;
	m_lastTime = SDL_GetTicks();

	while (!m_done)
	{
		// Handle input events.
		HandleEvents();

		Uint32 time = SDL_GetTicks();
		float dt = (time - m_lastTime) / 1000.0f;
		m_lastTime = time;

		m_time += dt;
		m_timeDelta = dt;

		// Update and render
		glViewport(0, 0, m_width, m_height);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);

		// Render 3d
		glEnable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(50.0f, (float)m_width / (float)m_height, 1.0f, m_camr);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(m_rx, 1, 0, 0);
		glRotatef(m_ry, 0, 1, 0);
		glTranslatef(-m_camx, -m_camy, -m_camz);

		// Get hit ray position and direction.
		glGetDoublev(GL_PROJECTION_MATRIX, m_proj);
		glGetDoublev(GL_MODELVIEW_MATRIX, m_model);
		glGetIntegerv(GL_VIEWPORT, m_view);
		GLdouble x, y, z;

		if (m_showPreview && m_mesh)
		{
			std::lock_guard<std::mutex> lock(m_renderMutex);

			gluUnProject(m_mx, m_my, 0.0f, m_model, m_proj, m_view, &x, &y, &z);
			m_rays[0] = (float)x;
			m_rays[1] = (float)y;
			m_rays[2] = (float)z;

			gluUnProject(m_mx, m_my, 1.0f, m_model, m_proj, m_view, &x, &y, &z);
			m_raye[0] = (float)x;
			m_raye[1] = (float)y;
			m_raye[2] = (float)z;

			// Handle keyboard movement.
			Uint8* keystate = SDL_GetKeyState(NULL);
			m_moveW = rcClamp(m_moveW + dt * 4 * (keystate[SDLK_w] ? 1 : -1), 0.0f, 1.0f);
			m_moveS = rcClamp(m_moveS + dt * 4 * (keystate[SDLK_s] ? 1 : -1), 0.0f, 1.0f);
			m_moveA = rcClamp(m_moveA + dt * 4 * (keystate[SDLK_a] ? 1 : -1), 0.0f, 1.0f);
			m_moveD = rcClamp(m_moveD + dt * 4 * (keystate[SDLK_d] ? 1 : -1), 0.0f, 1.0f);

			float keybSpeed = 30.0f;
			if (SDL_GetModState() & KMOD_SHIFT)
				keybSpeed *= 10.0f;
			if (SDL_GetModState() & KMOD_ALT)
				keybSpeed *= 10.0f;

			float movex = (m_moveD - m_moveA) * keybSpeed * dt;
			float movey = (m_moveS - m_moveW) * keybSpeed * dt;

			m_camx += movex * (float)m_model[0];
			m_camy += movex * (float)m_model[4];
			m_camz += movex * (float)m_model[8];

			m_camx += movey * (float)m_model[2];
			m_camy += movey * (float)m_model[6];
			m_camz += movey * (float)m_model[10];

			glEnable(GL_FOG);
			m_mesh->handleRender();
			glDisable(GL_FOG);
		}

		// Render GUI
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0, m_width, 0, m_height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		m_mouseOverMenu = false;

		imguiBeginFrame(m_mx, m_my, m_mbuttons, m_mscroll);

		RenderInterface();

		imguiRenderGLDraw();

		// Camera Reset
		if (m_geom && m_resetCamera)
		{
			const float* bmin = m_geom->getMeshBoundsMin();
			const float* bmax = m_geom->getMeshBoundsMax();
			// Reset camera and fog to match the mesh bounds.
			m_camr = sqrtf(rcSqr(bmax[0] - bmin[0]) +
				rcSqr(bmax[1] - bmin[1]) +
				rcSqr(bmax[2] - bmin[2])) / 2;
			m_camx = (bmax[0] + bmin[0]) / 2 + m_camr;
			m_camy = (bmax[1] + bmin[1]) / 2 + m_camr;
			m_camz = (bmax[2] + bmin[2]) / 2 + m_camr;
			m_camr *= 3;
			m_rx = 45;
			m_ry = -45;

			glFogf(GL_FOG_START, m_camr * 0.2f);
			glFogf(GL_FOG_END, m_camr * 1.25f);

			m_resetCamera = false;
		}

		glEnable(GL_DEPTH_TEST);
		SDL_GL_SwapBuffers();
	}

	Halt();

	m_geom.reset();
	return 0;
}

void Interface::HandleEvents()
{
	m_mscroll = 0;
	m_mbuttons = 0;

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_VIDEORESIZE:
			m_width = event.resize.w;
			m_height = event.resize.h;
			break;

		case SDL_KEYDOWN:
			// Handle any key presses here.
			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				//HaltBuild();
				m_done = true;
			}
			else if (event.key.keysym.sym == SDLK_TAB)
			{
				// TODO: Reacquire paths
				//DefineDirectories(true);
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			// Handle mouse clicks here.
			if (!m_mouseOverMenu)
			{
				if (event.button.button == SDL_BUTTON_RIGHT)
				{
					// Rotate view
					m_rotate = true;
					m_origx = m_mx;
					m_origy = m_my;
					m_origrx = m_rx;
					m_origry = m_ry;
				}
				else if (event.button.button == SDL_BUTTON_LEFT)
				{
					// Hit test mesh.
					if (m_geom && m_mesh)
					{
						// Hit test mesh.
						float t;
						if (m_geom->raycastMesh(m_rays, m_raye, t))
						{
							if (SDL_GetModState() & KMOD_CTRL)
							{
								m_mposSet = true;
								m_mpos[0] = m_rays[0] + (m_raye[0] - m_rays[0])*t;
								m_mpos[1] = m_rays[1] + (m_raye[1] - m_rays[1])*t;
								m_mpos[2] = m_rays[2] + (m_raye[2] - m_rays[2])*t;
							}
							else
							{
								float pos[3];
								pos[0] = m_rays[0] + (m_raye[0] - m_rays[0])*t;
								pos[1] = m_rays[1] + (m_raye[1] - m_rays[1])*t;
								pos[2] = m_rays[2] + (m_raye[2] - m_rays[2])*t;
								bool shift = (SDL_GetModState() & KMOD_SHIFT) ? true : false;

								//if (!message)
								m_mesh->handleClick(m_rays, pos, shift);
							}
						}
						else
						{
							if (SDL_GetModState() & KMOD_CTRL)
							{
								m_mposSet = false;
							}
						}
					}
				}
			}
			if (event.button.button == SDL_BUTTON_WHEELUP)
				m_mscroll--;
			if (event.button.button == SDL_BUTTON_WHEELDOWN)
				m_mscroll++;
			break;

		case SDL_MOUSEBUTTONUP:
			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				m_rotate = false;
			}
			break;

		case SDL_MOUSEMOTION:
			m_mx = event.motion.x;
			m_my = m_height - 1 - event.motion.y;
			if (m_rotate)
			{
				int dx = m_mx - m_origx;
				int dy = m_my - m_origy;
				m_rx = m_origrx - dy*0.25f;
				m_ry = m_origry + dx*0.25f;
			}
			break;

		case SDL_QUIT:
			Halt();
			m_done = true;
			break;

		default:
			break;
		}
	}

	if (SDL_GetMouseState(0, 0) & SDL_BUTTON_LMASK)
		m_mbuttons |= IMGUI_MBUT_LEFT;
	if (SDL_GetMouseState(0, 0) & SDL_BUTTON_RMASK)
		m_mbuttons |= IMGUI_MBUT_RIGHT;
}

void Interface::RenderInterface()
{
	if (m_mesh)
	{
		m_mesh->handleRenderOverlay((double*)m_proj, (double*)m_model, (int*)m_view);
	}

	bool showMenu = true;
	bool showDebugMode = false;

	char buffer[240] = { 0, };

	// Help text.
	if (showMenu)
	{
		static const char msg[] = "Hotkey List -> W/S/A/D: Move  "
			"RMB: Rotate   "
			"LMB: Place Start   "
			"LMB+SHIFT: Place End   "
			"TAB: Redefine EQ & MQ2 Directories";

		imguiDrawText(m_width / 2, m_height - 20, IMGUI_ALIGN_CENTER, msg, imguiRGBA(255, 255, 255, 128));
	}

	if (!m_activityMessage.empty())
	{
		sprintf(buffer, m_activityMessage.c_str(), m_progress);

		imguiDrawText(m_width / 2, m_height / 2, IMGUI_ALIGN_CENTER, buffer, imguiRGBA(255, 255, 255, 128));
	}

	if (showMenu)
	{
		int propDiv = m_showDebugMode ? (int)(m_height*0.6f) : m_height;
		int propScroll = 0;

		if (imguiBeginScrollArea("Properties",
			m_width - 250 - 10, 10 + m_height - propDiv, 250, propDiv - 20, &propScroll))
		{
			m_mouseOverMenu = true;
		}

		if (imguiCheck("Show Log", m_showLog))
			m_showLog = !m_showLog;
		if (imguiCheck("Show Tools", m_showTools))
			m_showTools = !m_showTools;
		if (imguiCheck("Show Preview", m_showPreview))
			m_showPreview = !m_showPreview;
#if 0
		if (imguiCheck("Show Debug Mode", m_showDebugMode))
			m_showDebugMode = !m_showDebugMode;
#endif

		imguiSeparator();

		if (m_mesh)
		{
			imguiSeparator();
			imguiLabel("Zone Mesh");

			if (imguiButton(m_zoneDisplayName.c_str()))
			{
				if (m_showLevels)
				{
					m_showLevels = false;
				}
				else
				{
					m_showSample = false;
					m_showTestCases = false;
					m_showLevels = true;
				}
			}

			//if (m_geom)
			//{
			//	char text[64];
			//	snprintf(text, 64, "Verts: %.1fk  Tris: %.1fk",
			//	geom->getMesh()->getVertCount()/1000.0f,
			//	geom->getMesh()->getTriCount()/1000.0f);
			//	imguiValue(text);
			//}
			//imguiSeparator();
		}


		if (m_geom && m_mesh)
		{
			if (/*message*/ false)
			{
				imguiSeparator();

				if (imguiButton("Halt Action"))
					Halt();
			}
			else
			{
				m_mesh->handleSettings();

				if (imguiButton("Build"))
					StartBuild();

				imguiSeparator();
			}
		}

		imguiEndScrollArea();

		if (showDebugMode)
		{
			int debugScroll = 0;

			if (imguiBeginScrollArea("Debug Mode",
				m_width - 250 - 10, 10,
				250, m_height - propDiv - 10, &debugScroll))
			{
				m_mouseOverMenu = true;
			}

			if (m_mesh)
				m_mesh->handleDebugMode();

			imguiEndScrollArea();
		}
	}

	// Level selection dialog.
	if (m_showLevels)
	{
		std::unique_lock<std::mutex> lock(m_renderMutex);

		static int levelScroll = 0;
		if (imguiBeginScrollArea("Choose Expansion",
			m_width - 10 - 300 - 10 - 350,
			m_height - 10 - 800, 400, 800, &levelScroll))
		{
			m_mouseOverMenu = true;
		}

		std::string levelToLoad;

		const auto& mapList = m_eqConfig.GetMapList();
		for (const auto& mapIter : mapList)
		{
			const std::string& expansionName = mapIter.first;
			bool expanded = m_expansionExpanded[expansionName];

			if (imguiCollapse(expansionName.c_str(), 0, expanded))
				m_expansionExpanded[expansionName] = !expanded;

			if (m_expansionExpanded[expansionName])
			{
				imguiIndent();

				for (const auto& zonePair : mapIter.second)
				{
					std::stringstream ss;
					ss << zonePair.first << " (" << zonePair.second << ")";

					if (imguiItem(ss.str().c_str()))
						levelToLoad = zonePair.second;

				}

				imguiUnindent();
			}
		}


		if (!levelToLoad.empty() || !m_defaultZone.empty())
		{
			m_geom.reset();
			m_showLevels = false;

			if (!levelToLoad.empty())
			{
				m_zoneShortname = levelToLoad;
			}
			else
			{
				m_zoneShortname = m_defaultZone;
				m_defaultZone.clear();
			}

			m_zoneLongname = m_eqConfig.GetLongNameForShortName(m_zoneShortname);

			std::stringstream ss;
			ss << m_zoneLongname << " (" << m_zoneShortname << ")";
			m_zoneDisplayName = ss.str();
			m_expansionExpanded.clear();

			LoadGeometry();
		}
		imguiEndScrollArea();
	}

	// Log
	if (m_showLog && showMenu)
	{
		int logScroll = 0;

		if (imguiBeginScrollArea("Log", 10, 10, m_width - 300, 200, &logScroll))
			m_mouseOverMenu = true;

		for (int i = 0; i < m_context->getLogCount(); ++i)
		{
			imguiLabel(m_context->getLogText(i));
		}

		imguiEndScrollArea();
	}

	// Tools
	if (!m_showTestCases && m_showTools && showMenu && m_geom && m_mesh)
	{
		int toolsScroll = 0;

		if (imguiBeginScrollArea("Tools", 10, m_height - 10 - 350, 200, 350, &toolsScroll))
			m_mouseOverMenu = true;

		m_mesh->handleTools();

		imguiEndScrollArea();
	}

	imguiEndFrame();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Interface::LoadZoneList()
{
}

// the shitty zone. Fix this up
HANDLE handle = 0;

DWORD WINAPI Interface::BuildThread(LPVOID lpParam)
{
	Interface* iface = static_cast<Interface*>(lpParam);
	iface->BuildThreadImpl();
	return NULL;
}

void Interface::BuildThreadImpl()
{
	if (m_mesh)
	{
		m_mesh->handleBuild();
	}
}

void Interface::StartBuild()
{
	Halt();

	handle = CreateThread(NULL, 0, BuildThread, this, 0, NULL);
}

void Interface::LoadGeometry()
{
	Halt();

	m_geom.reset(new InputGeom());
	m_geom->loadMesh(m_context.get(), m_zoneShortname.c_str(),
		m_eqConfig.GetEverquestPath().c_str());

	if (m_mesh && m_geom)
	{
		m_mesh->handleMeshChanged(m_geom.get());
		m_resetCamera = true;
	}
}

void Interface::Halt()
{
	// message = NULL;
	if (handle)
	{
		DWORD exitCode = 0;
		GetExitCodeThread(handle, &exitCode);
		TerminateThread(handle, exitCode);

		handle = 0;
	}

	{
		//std::unique_lock<std::mutex> lock(m_renderMutex);

		m_mesh.reset(new Sample_TileMesh());
		m_mesh->setContext(m_context.get());
		m_mesh->setOutputPath(m_eqConfig.GetOutputPath().c_str());

		m_geom.reset(new InputGeom());
	}

}

//============================================================================

BuildContext::BuildContext()
{
	resetTimers();
}

BuildContext::~BuildContext()
{
}

//----------------------------------------------------------------------------

void BuildContext::doResetLog()
{
	m_logs.clear();
}

void BuildContext::doLog(const rcLogCategory category,
	const char* message, const int length)
{
	if (!length)
		return;

	// if the message buffer is full, drop an old message
	if (m_logs.size() > MAX_LOG_MESSAGES)
		m_logs.pop_front();

	m_logs.emplace_back(std::string(message, static_cast<std::size_t>(length)));
}

void BuildContext::dumpLog(const char* format, ...)
{
	// Print header.
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	printf("\n");

	// Print messages
	const int TAB_STOPS[4] = { 28, 36, 44, 52 };
	for (auto& message : m_logs)
	{
		const char* msg = message.c_str();
		int n = 0;
		while (*msg)
		{
			if (*msg == '\t')
			{
				int count = 1;
				for (int j = 0; j < 4; ++j)
				{
					if (n < TAB_STOPS[j])
					{
						count = TAB_STOPS[j] - n;
						break;
					}
				}
				while (--count)
				{
					putchar(' ');
					n++;
				}
			}
			else
			{
				putchar(*msg);
				n++;
			}
			msg++;
		}
		putchar('\n');
	}
}

const char* BuildContext::getLogText(int32_t index) const
{
	if (index >= 0 && index < (int32_t)m_logs.size())
		return m_logs[index].c_str();

	return nullptr;
}

int BuildContext::getLogCount() const
{
	return m_logs.size();
}

//----------------------------------------------------------------------------

void BuildContext::doResetTimers()
{
	for (int i = 0; i < RC_MAX_TIMERS; ++i)
		m_accTime[i] = -1;
}

void BuildContext::doStartTimer(const rcTimerLabel label)
{
	m_startTime[label] = getPerfTime();
}

void BuildContext::doStopTimer(const rcTimerLabel label)
{
	const TimeVal endTime = getPerfTime();
	const int deltaTime = static_cast<int>(endTime - m_startTime[label]);
	if (m_accTime[label] == -1)
		m_accTime[label] = deltaTime;
	else
		m_accTime[label] += deltaTime;
}

int BuildContext::doGetAccumulatedTime(const rcTimerLabel label) const
{
	return m_accTime[label];
}
