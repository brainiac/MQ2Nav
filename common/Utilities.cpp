//
// Utilities.cpp
//

#include "Utilities.h"

#include <imgui/imgui.h>
#include <imgui/fonts/font_fontawesome_ttf.h>
#include <imgui/fonts/font_roboto_regular_ttf.h>
#include <imgui/fonts/font_materialicons_ttf.h>
#include <imgui/fonts/IconsFontAwesome.h>
#include <imgui/fonts/IconsMaterialDesign.h>

#include <SDL.h>
#include <SDL_OpenGL.h>

#include <stdio.h>

ImFont* ImGuiEx::DefaultFont = nullptr;
ImFont* ImGuiEx::ConsoleFont = nullptr;

//----------------------------------------------------------------------------

class GLCheckerTexture
{
	unsigned int m_texId;
public:
	GLCheckerTexture() : m_texId(0)
	{
	}

	~GLCheckerTexture()
	{
		if (m_texId != 0)
			glDeleteTextures(1, &m_texId);
	}
	void bind()
	{
		if (m_texId == 0)
		{
			// Create checker pattern.
			const unsigned int col0 = duRGBA(215, 215, 215, 255);
			const unsigned int col1 = duRGBA(255, 255, 255, 255);
			static const int TSIZE = 64;
			unsigned int data[TSIZE*TSIZE];

			glGenTextures(1, &m_texId);
			glBindTexture(GL_TEXTURE_2D, m_texId);

			int level = 0;
			int size = TSIZE;
			while (size > 0)
			{
				for (int y = 0; y < size; ++y)
					for (int x = 0; x < size; ++x)
						data[x + y*size] = (x == 0 || y == 0) ? col0 : col1;
				glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				size /= 2;
				level++;
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, m_texId);
		}
	}
};
GLCheckerTexture g_tex;


void DebugDrawGL::depthMask(bool state)
{
	glDepthMask(state ? GL_TRUE : GL_FALSE);
}

void DebugDrawGL::texture(bool state)
{
	if (state)
	{
		glEnable(GL_TEXTURE_2D);
		g_tex.bind();
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
}

void DebugDrawGL::begin(duDebugDrawPrimitives prim, float size)
{
	switch (prim)
	{
	case DU_DRAW_POINTS:
		glPointSize(size);
		glBegin(GL_POINTS);
		break;
	case DU_DRAW_LINES:
		glLineWidth(size);
		glBegin(GL_LINES);
		break;
	case DU_DRAW_TRIS:
		glBegin(GL_TRIANGLES);
		break;
	case DU_DRAW_QUADS:
		glBegin(GL_QUADS);
		break;
	};
}

void DebugDrawGL::vertex(const float* pos, unsigned int color)
{
	glColor4ubv((GLubyte*)&color);
	glVertex3fv(pos);
}

void DebugDrawGL::vertex(const float x, const float y, const float z, unsigned int color)
{
	glColor4ubv((GLubyte*)&color);
	glVertex3f(x, y, z);
}

void DebugDrawGL::vertex(const float* pos, unsigned int color, const float* uv)
{
	glColor4ubv((GLubyte*)&color);
	glTexCoord2fv(uv);
	glVertex3fv(pos);
}

void DebugDrawGL::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
{
	glColor4ubv((GLubyte*)&color);
	glTexCoord2f(u, v);
	glVertex3f(x, y, z);
}

void DebugDrawGL::end()
{
	glEnd();
	glLineWidth(1.0f);
	glPointSize(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FileIO::FileIO() :
	m_fp(0),
	m_mode(-1)
{
}

FileIO::~FileIO()
{
	if (m_fp) fclose(m_fp);
}

bool FileIO::openForWrite(const char* path)
{
	if (m_fp) return false;
	m_fp = fopen(path, "wb");
	if (!m_fp) return false;
	m_mode = 1;
	return true;
}

bool FileIO::openForRead(const char* path)
{
	if (m_fp) return false;
	m_fp = fopen(path, "rb");
	if (!m_fp) return false;
	m_mode = 2;
	return true;
}

bool FileIO::isWriting() const
{
	return m_mode == 1;
}

bool FileIO::isReading() const
{
	return m_mode == 2;
}

bool FileIO::write(const void* ptr, const size_t size)
{
	if (!m_fp || m_mode != 1) return false;
	fwrite(ptr, size, 1, m_fp);
	return true;
}

bool FileIO::read(void* ptr, const size_t size)
{
	if (!m_fp || m_mode != 2) return false;
	size_t readLen = fread(ptr, size, 1, m_fp);
	return readLen == 1;
}




void ImGuiEx::HelpMarker(const char* desc, float width, ImFont* tooltipFont)
{
	ImGui::TextDisabled(ICON_FA_QUESTION_CIRCLE_O);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(width);

		if (tooltipFont)
		{
			ImGui::PushFont(tooltipFont);
		}

		ImGui::TextUnformatted(desc);

		if (tooltipFont)
		{
			ImGui::PopFont();
		}

		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void ImGuiEx::ConfigureFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	// font: Roboto Regular @ 16px
	DefaultFont = io.Fonts->AddFontFromMemoryCompressedTTF(GetRobotoRegularCompressedData(),
		GetRobotoRegularCompressedSize(), 16.0);

	// font: FontAwesome
	ImFontConfig faConfig;
	faConfig.MergeMode = true;
	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromMemoryCompressedTTF(GetFontAwesomeCompressedData(),
		GetFontAwesomeCompressedSize(), 14.0f, &faConfig, icon_ranges);

	// font: Material Design Icons
	ImFontConfig mdConfig;
	mdConfig.MergeMode = true;
	static const ImWchar md_icon_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
	io.Fonts->AddFontFromMemoryCompressedTTF(GetMaterialIconsCompressedData(),
		GetMaterialIconsCompressedSize(), 13.0f, &mdConfig, md_icon_ranges);

	// add default proggy clean font as a secondary font
	ConsoleFont = io.Fonts->AddFontDefault();
}
