// Implementation of main GUI interface for EQNavigation

#include "pch.h"
#include "Interface.h"


#include "RecastDebugDraw.h"
#include "InputGeom.h"
#include "Sample_TileMesh.h"
#include "Sample_Debug.h"

#include <imgui/imgui.h>
#include "imgui_impl_sdl.h"

#include <gl/GLU.h>

#include <stdarg.h>
#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <sstream>

#pragma warning(push)
#pragma warning(disable: 4244)

//============================================================================

static const int32_t MAX_LOG_MESSAGES = 1000;

static bool IsKeyboardBlocked() {
	return ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput;
}

//============================================================================

Interface::Interface(const std::string& defaultZone)
	: m_context(new BuildContext())
	, m_mesh(new Sample_TileMesh)
	, m_resetCamera(true)
	, m_width(1600), m_height(900)
	, m_progress(0.0)
	, m_showPreview(true)
	, m_showLog(false)
	, m_nextZoneToLoad(defaultZone)
{
	m_mesh->setContext(m_context.get());
	m_mesh->setOutputPath(m_eqConfig.GetOutputPath().c_str());

	// Construct the path to the ini file
	CHAR fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);
	PathAppendA(fullPath, "EQNavigation_UI.ini");
	m_iniFile = fullPath;

	InitializeWindow();
	SetTheme(m_currentTheme, true);
}

Interface::~Interface()
{
	DestroyWindow();
}

unsigned int GetDroidSansCompressedSize();
const unsigned int* GetDroidSansCompressedData();

bool Interface::InitializeWindow()
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialise SDL\n");
		return false;
	}

	// Init OpenGL
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

//#ifndef WIN32
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
//#endif
	
	SDL_Rect vr;
	SDL_GetDisplayBounds(0, &vr);

	m_width = vr.w - 200;
	m_height = vr.h - 200;

	m_window = SDL_CreateWindow("EQ Navigation", 100, 100, m_width, m_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	m_glContext = SDL_GL_CreateContext(m_window);

	ImGui_ImplSdl_Init(m_window);

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = m_iniFile.c_str();
	io.Fonts->AddFontFromMemoryCompressedTTF(GetDroidSansCompressedData(),
		GetDroidSansCompressedSize(), 16);

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
	ImGui_ImplSdl_Shutdown();

	SDL_GL_DeleteContext(m_glContext);
	SDL_DestroyWindow(m_window);

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

		ImGui_ImplSdl_NewFrame(m_window);

		Uint32 time = SDL_GetTicks();
		float dt = (time - m_lastTime) / 1000.0f;
		m_lastTime = time;

		m_time += dt;
		m_timeDelta = dt;

		RenderInterface();

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

		if (m_showPreview)
		{
			gluUnProject(m_mx, m_my, 0.0f, m_model, m_proj, m_view, &x, &y, &z);
			m_rays[0] = (float)x;
			m_rays[1] = (float)y;
			m_rays[2] = (float)z;

			gluUnProject(m_mx, m_my, 1.0f, m_model, m_proj, m_view, &x, &y, &z);
			m_raye[0] = (float)x;
			m_raye[1] = (float)y;
			m_raye[2] = (float)z;

			// Handle keyboard movement.
			if (!IsKeyboardBlocked())
			{
				const Uint8* keystate = SDL_GetKeyboardState(NULL);
				m_moveW = rcClamp(m_moveW + dt * 4 * (keystate[SDL_SCANCODE_W] ? 1 : -1), 0.0f, 1.0f);
				m_moveS = rcClamp(m_moveS + dt * 4 * (keystate[SDL_SCANCODE_S] ? 1 : -1), 0.0f, 1.0f);
				m_moveA = rcClamp(m_moveA + dt * 4 * (keystate[SDL_SCANCODE_A] ? 1 : -1), 0.0f, 1.0f);
				m_moveD = rcClamp(m_moveD + dt * 4 * (keystate[SDL_SCANCODE_D] ? 1 : -1), 0.0f, 1.0f);
				m_moveUp = rcClamp(m_moveUp + dt * 4 * (keystate[SDL_SCANCODE_SPACE] ? 1 : -1), 0.0f, 1.0f);
				m_moveDown = rcClamp(m_moveDown + dt * 4 * (keystate[SDL_SCANCODE_C] ? 1 : -1), 0.0f, 1.0f);

				float keybSpeed = 250.0f;
				if (SDL_GetModState() & KMOD_SHIFT)
					keybSpeed *= 10.0f;

				float movex = (m_moveD - m_moveA) * keybSpeed * dt;
				float movey = (m_moveS - m_moveW) * keybSpeed * dt;
				float movez = (m_moveUp - m_moveDown) * keybSpeed * dt;

				//  0  1  2  3
				//  4  5  6  7
				//  8  9 10 11
				// 12 13 14 15

				m_camx += movex * (float)m_model[0];
				m_camy += movex * (float)m_model[4];
				m_camz += movex * (float)m_model[8];

				m_camx += movey * (float)m_model[2];
				m_camy += movey * (float)m_model[6];
				m_camz += movey * (float)m_model[10];

				m_camy += movez;
			}

			std::lock_guard<std::mutex> lock(m_renderMutex);

			glEnable(GL_FOG);
			m_mesh->handleRender();
			glDisable(GL_FOG);
		}

		// Perform the camera reset if requested
		if (m_resetCamera)
		{
			ResetCamera();
			m_resetCamera = false;
		}

		glEnable(GL_DEPTH_TEST);

		ImGui::Render();
		SDL_GL_SwapWindow(m_window);

		// Do additional work here after rendering

		// Load a zone if a zone load was requested
		if (!m_nextZoneToLoad.empty())
		{
			LoadGeometry(m_nextZoneToLoad);
			m_nextZoneToLoad.clear();
		}
	}

	Halt();

	m_geom.reset();
	return 0;
}

void Interface::HandleEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSdl_ProcessEvent(&event);

		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				m_width = event.window.data1;
				m_height = event.window.data2;
			}
			break;

		case SDL_KEYDOWN:
			if (IsKeyboardBlocked())
				break;

			// Handle any key presses here.
			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				Halt();
			}
			else if (event.key.keysym.mod & KMOD_CTRL)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_o:
					m_showZonePickerDialog = true;
					break;
				case SDLK_l:
					OpenMesh();
					break;
				case SDLK_s:
					SaveMesh();
					break;
				case SDLK_t:
					m_showTools = !m_showTools;
					break;
				case SDLK_p:
					m_showSettingsDialog = true;
					break;
				default:
					//printf("key: %d", event.key.keysym.sym);
					break;
				}
			}
			else if (event.key.keysym.sym == SDLK_F4
				&& (event.key.keysym.mod & KMOD_ALT))
			{
				m_done = true;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (ImGui::GetIO().WantCaptureMouse)
				break;

			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				//SDL_SetRelativeMouseMode(SDL_TRUE);

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
				if (m_geom)
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
			break;

		case SDL_MOUSEBUTTONUP:
			if (ImGui::GetIO().WantCaptureMouse)
				break;

			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				//SDL_SetRelativeMouseMode(SDL_FALSE);
				m_rotate = false;
			}
			break;

		case SDL_MOUSEMOTION:
			if (ImGui::GetIO().WantCaptureMouse)
				break;

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
}

void Interface::RenderInterface()
{
	m_mesh->handleRenderOverlay((double*)m_proj, (double*)m_model, (int*)m_view);

	bool showMenu = true;

	char buffer[240] = { 0, };

	// Help text.
	if (showMenu)
	{
		static const char msg[] = "Hotkey List -> W/S/A/D: Move  "
			"RMB: Rotate   "
			"LMB: Place Start   "
			"LMB+SHIFT: Place End   ";

		ImGui::RenderTextCentered(ImVec2(m_width / 2, 20),
			ImColor(255, 255, 255, 128), "%s", msg);
	}

	if (!m_activityMessage.empty())
	{
		ImGui::RenderTextCentered(ImVec2(m_width / 2, m_height / 2),
			ImColor(255, 255, 255, 128), m_activityMessage.c_str(), m_progress);
	}

	if (showMenu)
	{
		m_showFailedToOpenDialog = false;

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Zone", "Ctrl+O", nullptr,
					!m_mesh->isBuildingTiles()))
				{
					m_showZonePickerDialog = true;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Load Mesh", "Ctrl+L", nullptr,
					!m_mesh->isBuildingTiles()))
				{
					OpenMesh();
				}
				if (ImGui::MenuItem("Save Mesh", "Ctrl+S", nullptr,
					!m_mesh->isBuildingTiles() && m_mesh->getNavMesh() != nullptr))
				{
					SaveMesh();
				}
				if (ImGui::MenuItem("Reset Mesh", "", nullptr,
					!m_mesh->isBuildingTiles() && m_mesh->getNavMesh() != nullptr))
				{
					m_mesh->ResetMesh();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Settings", "Ctrl+P", nullptr))
				{
					m_showSettingsDialog = true;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit", "Alt+F4"))
					m_done = true;
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Show Log", 0, m_showLog))
					m_showLog = !m_showLog;
				if (ImGui::MenuItem("Show Preview", 0, m_showPreview))
					m_showPreview = !m_showPreview;
				if (ImGui::MenuItem("Show Tools", "Ctrl+T", m_showTools))
					m_showTools = !m_showTools;

				ImGui::Separator();

				if (ImGui::BeginMenu("Theme"))
				{
					if (ImGui::MenuItem("Default", 0, m_currentTheme == DefaultTheme))
						SetTheme(DefaultTheme);
					if (ImGui::MenuItem("Light", 0, m_currentTheme == LightTheme))
						SetTheme(LightTheme);
					if (ImGui::MenuItem("DarkTheme1", 0, m_currentTheme == DarkTheme1))
						SetTheme(DarkTheme1);
					if (ImGui::MenuItem("DarkTheme2", 0, m_currentTheme == DarkTheme2))
						SetTheme(DarkTheme2);
					if (ImGui::MenuItem("DarkTheme3", 0, m_currentTheme == DarkTheme3))
						SetTheme(DarkTheme3);
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		if (m_showFailedToOpenDialog)
			ImGui::OpenPopup("Failed To Open");
		if (ImGui::BeginPopupModal("Failed To Open"))
		{
			m_showFailedToOpenDialog = false;
			ImGui::Text("Failed to open mesh.");
			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		if (m_showFailedToLoadZone)
			ImGui::OpenPopup("Failed To Open Zone");
		if (ImGui::BeginPopupModal("Failed To Open Zone"))
		{
			m_showFailedToLoadZone = false;

			ImGui::Text("%s", m_failedZoneMsg.c_str());
			if (ImGui::Button("Close"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ShowSettingsDialog();
		ShowZonePickerDialog();
	}

	// start the properties dialog
	ImGui::SetNextWindowPos(ImVec2(10, 40));
	ImGui::SetNextWindowSize(ImVec2(300, m_height - 20));
	if (ImGui::Begin("Properties", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::LabelText("Zone", m_zoneDisplayName.c_str());

		float camPos[3] = { m_camz, m_camx, m_camy };
		if (ImGui::InputFloat3("Position", camPos, -2))
		{
			m_camx = camPos[1];
			m_camy = camPos[2];
			m_camz = camPos[0];
		}

		ImGui::Separator();

		if (m_geom)
		{
			ImGui::LabelText("Verts", "%.1fk", m_geom->getMeshLoader()->getVertCount() / 1000.0f);
			ImGui::LabelText("Tris", "%.1fk", m_geom->getMeshLoader()->getTriCount() / 1000.0f);

			int tw, th, tm;
			m_mesh->getTileStatistics(tw, th, tm);
			int tt = tw*th;

			float percent = (float)m_mesh->getTilesBuilt() / (float)tt * 100;

			if (m_mesh->isBuildingTiles())
			{
				ImGui::LabelText("Progress", "%d of %d (%.2f%%)",
					m_mesh->getTilesBuilt(), tt, percent);

				if (ImGui::Button("Halt Build"))
					m_mesh->cancelBuildAllTiles();
			}
			else
			{
				ImVec4 col = ImColor(255, 255, 255);
				if (tt > tm) {
					col = ImColor(255, 0, 0);
				}
				ImGui::TextColored(col, "Tile Limit: %d", tm);

				if (ImGui::Button("Build Mesh"))
					m_mesh->handleBuild();

				ImGui::SameLine();

				col = ImColor(0, 255, 0);
				if (tt > tm) {
					col = ImColor(255, 0, 0);
				}
				ImGui::TextColored(col, "%d Tiles (%d x %d)", tt, tw, th);

				float totalBuildTime = m_mesh->getTotalBuildTimeMS();
				if (totalBuildTime > 0)
					ImGui::Text("Build Time: %.1fms", totalBuildTime);
			}

			ImGui::Separator();

			if (!m_mesh->isBuildingTiles())
			{
				m_mesh->handleSettings();

				if (ImGui::CollapsingHeader("Debug"))
				{
					m_mesh->handleDebugMode();
				}
			}
		}

		ImGui::End();
	}

	if (m_geom && m_showTools)
	{
		ImGui::SetNextWindowPos(ImVec2(m_width - 300 - 10, 10));
		ImGui::SetNextWindowSize(ImVec2(300, 600));

		ImGui::Begin("Tools", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		m_mesh->handleTools();
		ImGui::End();
	}

	// Log
	if (m_showLog && showMenu)
	{
		int logXPos = 10 + 10 + 300;

		ImGui::SetNextWindowPos(ImVec2(logXPos, m_height - 200 - 10), ImGuiSetCond_Once);
		ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiSetCond_Once);
	
		ImGui::Begin("Log");
		ImGui::BeginChild("log contents");

		ImGuiListClipper clipper(m_context->getLogCount(), ImGui::GetTextLineHeightWithSpacing());
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) // display only visible items
			ImGui::Text(m_context->getLogText(i));
		clipper.End();

		// auto scroll if at bottom of window
		float scrollY = ImGui::GetScrollY();
		float scrollYMax = ImGui::GetScrollMaxY();

		if (m_lastScrollPosition == m_lastScrollPositionMax
			&& m_lastScrollPosition != scrollYMax
			/*|| m_lastScrollPosition == 0*/) // find a way to clamp when starting empty
		{
			m_lastScrollPositionMax = scrollYMax;
			ImGui::SetScrollY(scrollYMax);
		}
		else if (m_lastScrollPositionMax != scrollYMax)
		{
			m_lastScrollPositionMax = scrollYMax;
		}

		m_lastScrollPosition = scrollY;

		ImGui::End();
		ImGui::End();
	}
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void Interface::OpenMesh()
{
	if (m_mesh->isBuildingTiles())
		return;

	std::string meshFilename = GetMeshFilename();
	if (!m_mesh->LoadMesh(meshFilename))
	{
		m_showFailedToOpenDialog = true;
	}
}

void Interface::SaveMesh()
{
	if (m_mesh->isBuildingTiles() || m_mesh->getNavMesh() == nullptr)
		return;

	std::string meshFilename = GetMeshFilename();

	m_mesh->SaveMesh(meshFilename);
}

void Interface::ShowSettingsDialog()
{
	if (m_showSettingsDialog)
		ImGui::OpenPopup("Settings");
	m_showSettingsDialog = false;

	if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("EQ Path:"); ImGui::SameLine();
		ImGui::TextColored(ImColor(244, 250, 125), "%s", m_eqConfig.GetEverquestPath().c_str());
		if (ImGui::Button("Change", ImVec2(120, 0)))
			m_eqConfig.SelectEverquestPath();

		ImGui::Text("Navmesh Path:"); ImGui::SameLine();
		ImGui::TextColored(ImColor(244, 250, 125), "%s", m_eqConfig.GetOutputPath().c_str());
		if (ImGui::Button("Change", ImVec2(120, 0)))
			m_eqConfig.SelectOutputPath();

		ImGui::Separator();

		if (ImGui::Button("Close", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void Interface::ShowZonePickerDialog()
{
	if (m_mesh->isBuildingTiles()) {
		m_showZonePickerDialog = false;
		return;
	}

	bool setfocus = false;
	if (m_showZonePickerDialog) {
		ImGui::OpenPopup("Open Zone");
		setfocus = true;
	}
	m_showZonePickerDialog = false;

	ImGui::SetNextWindowSize(ImVec2(500, 575), ImGuiSetCond_Once);
	if (ImGui::BeginPopupModal("Open Zone"))
	{
		ImGui::Text("Select a zone or type to filter by name");

		bool selectSingle = false;
		static char filterText[64] = "";
		if (setfocus) ImGui::SetKeyboardFocusHere(0);
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth());
		if (ImGui::InputText("", filterText, 64, ImGuiInputTextFlags_EnterReturnsTrue))
			selectSingle = true;
		ImGui::PopItemWidth();

		std::string text(filterText);
		bool closePopup = false;

		ImGui::BeginChild("##ZoneList", ImVec2(ImGui::GetWindowContentRegionWidth(),
			ImGui::GetWindowHeight() - 115), false);

		if (text.empty())
		{
			// if there is no filter we will display the tree of zones
			const auto& mapList = m_eqConfig.GetMapList();
			for (const auto& mapIter : mapList)
			{
				const std::string& expansionName = mapIter.first;

				if (ImGui::TreeNode(expansionName.c_str()))
				{
					ImGui::Columns(2);

					for (const auto& zonePair : mapIter.second)
					{
						const std::string& longName = zonePair.first;
						const std::string& shortName = zonePair.second;

						bool selected = false;
						ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns);
						ImGui::NextColumn();
						ImGui::SetColumnOffset(-1, 300);
						ImGui::Text(shortName.c_str());
						ImGui::NextColumn();

						if (selected) {
							LoadGeometry(zonePair.second.c_str());
							closePopup = true;
							break;
						}
					}
					ImGui::Columns(1);

					ImGui::TreePop();
				}
			}
		}
		else
		{
			const auto& mapList = m_eqConfig.GetAllMaps();
			bool count = 0;
			std::string lastZone;

			ImGui::Columns(2);

			for (const auto& mapIter : mapList)
			{
				const std::string& shortName = mapIter.first;
				const std::string& longName = mapIter.second;

				if (boost::ifind_first(shortName, text) || boost::ifind_first(longName, text))
				{
					std::string displayName = longName + " (" + shortName + ")";
					lastZone = shortName;
					bool selected = false;
					ImGui::Selectable(longName.c_str(), &selected, ImGuiSelectableFlags_SpanAllColumns);
					ImGui::NextColumn();
					ImGui::SetColumnOffset(-1, 300);
					ImGui::Text(shortName.c_str());
					ImGui::NextColumn();
					if (selected) {
						m_nextZoneToLoad = shortName.c_str();
						closePopup = true;
						break;
					}
				}
				count++;
			}

			ImGui::Columns(1);


			if (count == 1 && selectSingle) {
				m_nextZoneToLoad = lastZone;
				closePopup = true;
			}
		}

		ImGui::EndChild();

		ImGui::PushItemWidth(250);
		if (ImGui::Button("Cancel") || closePopup) {
			filterText[0] = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopItemWidth();

		ImGui::EndPopup();
	}
}

void Interface::ResetCamera()
{
	// Camera Reset
	if (m_geom)
	{
		const glm::vec3& bmin = m_geom->getMeshBoundsMin();
		const glm::vec3& bmax = m_geom->getMeshBoundsMax();

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
	}
}

std::string Interface::GetMeshFilename()
{
	std::stringstream ss;
	ss << m_eqConfig.GetOutputPath() << "\\MQ2Navigation\\" << m_zoneShortname << ".bin";

	return ss.str();
}

void Interface::Halt()
{
	m_mesh->cancelBuildAllTiles();
}

void Interface::LoadGeometry(const std::string& zoneShortName)
{
	std::unique_lock<std::mutex> lock(m_renderMutex);
	m_geom.reset();

	Halt();

	auto ptr = std::make_unique<InputGeom>(zoneShortName, m_eqConfig.GetEverquestPath());
	if (!ptr->loadMesh(m_context.get()))
	{
		m_showFailedToLoadZone = true;

		std::stringstream ss;
		ss << "Failed to load zone: " << m_eqConfig.GetLongNameForShortName(zoneShortName);

		m_failedZoneMsg = ss.str();
		return;
	}

	m_geom = std::move(ptr);

	m_zoneShortname = zoneShortName;
	m_zoneLongname = m_eqConfig.GetLongNameForShortName(m_zoneShortname);

	std::stringstream ss;
	ss << m_zoneLongname << " (" << m_zoneShortname << ")";
	m_zoneDisplayName = ss.str();
	m_expansionExpanded.clear();

	if (m_geom)
	{
		// Update the window title
		std::stringstream ss;
		ss << "EQ Navigation (" << m_zoneLongname << ")";
		std::string windowTitle = ss.str();
		SDL_SetWindowTitle(m_window, windowTitle.c_str());

		m_mesh->handleMeshChanged(m_geom.get());
		m_resetCamera = true;
	}
}

void Interface::SetTheme(Theme theme, bool force)
{
	if (m_currentTheme == theme && !force)
		return;
	m_currentTheme = theme;

	#pragma region Themes
	auto& style = ImGui::GetStyle();
	if (theme == DefaultTheme)
	{
		style = ImGuiStyle();
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.00f, 0.00f, 0.00f, 0.45f);
	}
	else if (theme == LightTheme)
	{
		style.Colors[ImGuiCol_Text] = ImVec4(0.31f, 0.25f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.68f, 0.68f, 0.68f, 0.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.19f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.62f, 0.70f, 0.72f, 0.56f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.33f, 0.14f, 0.47f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.97f, 0.31f, 0.13f, 0.81f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.42f, 0.75f, 1.00f, 0.53f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.65f, 0.80f, 0.20f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.40f, 0.62f, 0.80f, 0.15f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.39f, 0.64f, 0.80f, 0.30f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.67f, 0.80f, 0.59f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 0.48f, 0.53f, 0.67f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.89f, 0.98f, 1.00f, 0.99f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.37f, 0.37f, 0.91f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.31f, 0.47f, 0.99f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(1.00f, 0.79f, 0.18f, 0.78f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.82f, 1.00f, 0.81f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.86f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.73f, 0.80f, 0.86f, 0.45f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.88f, 0.94f, 0.80f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.46f, 0.84f, 0.90f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.41f, 0.75f, 0.98f, 0.50f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(1.00f, 0.47f, 0.41f, 0.60f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(1.00f, 0.16f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.99f, 0.54f, 0.43f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.82f, 0.92f, 1.00f, 0.90f);
		style.Alpha = 1.0f;
		style.WindowFillAlphaDefault = 1.0f;
		style.FrameRounding = 4;
	}
	else if (theme == DarkTheme1)
	{
		style.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.93f, 0.89f, 0.61f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.58f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.21f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.47f, 0.77f, 0.83f, 0.32f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.86f, 0.93f, 0.89f, 0.16f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.86f, 0.93f, 0.89f, 0.39f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.86f, 0.93f, 0.89f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.47f, 0.77f, 0.83f, 0.72f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);
		style.WindowMinSize = ImVec2(160, 20);
		style.FramePadding = ImVec2(4, 2);
		style.ItemSpacing = ImVec2(6, 2);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.Alpha = 0.95f;
		style.WindowFillAlphaDefault = 1.0f;
		style.WindowRounding = 4.0f;
		style.FrameRounding = 2.0f;
		style.IndentSpacing = 6.0f;
		style.ItemInnerSpacing = ImVec2(2, 4);
		style.ColumnsMinSpacing = 50.0f;
		style.GrabMinSize = 14.0f;
		style.GrabRounding = 16.0f;
		style.ScrollbarRounding = 16.0f;
	}
	else if (theme == DarkTheme2)
	{
		style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 0.86f, 0.50f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 0.86f, 0.22f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.97f, 0.48f, 0.32f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.22f, 0.22f, 0.50f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 0.62f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.68f, 0.68f, 0.68f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.71f, 0.71f, 0.71f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.72f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.65f);
		style.WindowPadding = ImVec2(20, 10);
		style.WindowMinSize = ImVec2(160, 20);
		style.FramePadding = ImVec2(4, 4);
		style.ItemSpacing = ImVec2(8, 4);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.Alpha = 0.95f;
		style.WindowFillAlphaDefault = 0.95f;
		style.WindowRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.IndentSpacing = 6;
		style.ColumnsMinSpacing = 50;
	}
	else if (theme == DarkTheme3)
	{
		style.Colors[ImGuiCol_Text] = ImVec4(0.72f, 0.79f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.96f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.09f);
		style.Colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.47f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.24f, 0.26f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.73f, 0.71f, 0.71f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.15f, 0.15f, 0.30f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.80f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.62f, 0.91f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.54f, 0.53f, 0.61f, 0.91f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.99f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.41f, 0.90f, 0.78f, 0.76f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.91f, 1.00f, 0.32f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 0.61f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.24f, 0.29f, 0.64f, 0.96f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.18f, 0.67f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.21f, 0.57f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.25f, 0.31f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.37f, 0.37f, 0.87f, 0.80f);
		style.Colors[ImGuiCol_Column] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.60f, 0.40f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.43f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.50f, 0.50f, 0.90f, 0.50f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.70f, 0.70f, 0.90f, 0.60f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.55f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.90f);
	}
	#pragma endregion
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
	std::unique_lock<std::mutex> lock(m_mtx);

	m_logs.clear();
}

void BuildContext::doLog(const rcLogCategory category,
	const char* message, const int length)
{
#if 0
	if (!length)
		return;

	std::unique_lock<std::mutex> lock(m_mtx);

	// if the message buffer is full, drop an old message
	if (m_logs.size() > MAX_LOG_MESSAGES)
		m_logs.pop_front();

	m_logs.emplace_back(std::string(message, static_cast<std::size_t>(length)));
#endif
	OutputDebugStringA(message);
}

void BuildContext::dumpLog(const char* format, ...)
{
	return;


	// Print header.
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	printf("\n");

	std::unique_lock<std::mutex> lock(m_mtx);

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
	std::unique_lock<std::mutex> lock(m_mtx);

	if (index >= 0 && index < (int32_t)m_logs.size())
		return m_logs[index].c_str();

	return nullptr;
}

int BuildContext::getLogCount() const
{
	std::unique_lock<std::mutex> lock(m_mtx);

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

#pragma warning(pop)