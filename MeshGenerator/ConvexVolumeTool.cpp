//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "ConvexVolumeTool.h"
#include "InputGeom.h"
#include "Sample.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "DetourDebugDraw.h"

#include <imgui/imgui.h>
#include <imgui/imgui_custom/imgui_user.h>
#include <glm/gtc/type_ptr.hpp>


// Quick and dirty convex hull.

// Returns true if 'c' is left of line 'a'-'b'.
inline bool left(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	const float u1 = b.x - a.x;
	const float v1 = b.z - a.z;
	const float u2 = c.x - a.x;
	const float v2 = c.z - a.z;

	return u1 * v2 - v1 * u2 < 0;
}

// Returns true if 'a' is more lower-left than 'b'.
inline bool cmppt(const glm::vec3& a, const glm::vec3& b)
{
	if (a.x < b.x) return true;
	if (a.x > b.x) return false;
	if (a.z < b.z) return true;
	if (a.z > b.z) return false;
	return false;
}

inline float distSqr(const glm::vec3& v1, const glm::vec3& v2)
{
	glm::vec3 temp = v2 - v1;
	return glm::dot(temp, temp);
}

// Calculates convex hull on xz-plane of points on 'pts',
// stores the indices of the resulting hull in 'out' and
// returns number of points on hull.
static int convexhull(const glm::vec3* pts, int npts, int* out)
{
	// Find lower-leftmost point.
	int hull = 0;
	for (int i = 1; i < npts; ++i)
		if (cmppt(pts[i], pts[hull]))
			hull = i;
	// Gift wrap hull.
	int endpt = 0;
	int i = 0;
	do
	{
		out[i++] = hull;
		endpt = 0;
		for (int j = 1; j < npts; ++j)
			if (hull == endpt || left(pts[hull], pts[endpt], pts[j]))
				endpt = j;
		hull = endpt;
	} while (endpt != out[0]);

	return i;
}

static int pointInPoly(int nvert, const glm::vec3* verts, const glm::vec3& p)
{
	bool c = false;

	for (int i = 0, j = nvert - 1; i < nvert; j = i++)
	{
		const glm::vec3& vi = verts[i];
		const glm::vec3& vj = verts[j];

		if (((vi[2] > p[2]) != (vj[2] > p[2])) &&
			(p[0] < (vj[0] - vi[0]) * (p[2] - vi[2]) / (vj[2] - vi[2]) + vi[0]))
		{
			c = !c;
		}
	}
	return c;
}

//----------------------------------------------------------------------------

ConvexVolumeTool::ConvexVolumeTool()
{
}

ConvexVolumeTool::~ConvexVolumeTool()
{
}

void ConvexVolumeTool::init(Sample* sample)
{
	m_sample = sample;
}

void ConvexVolumeTool::reset()
{
	m_npts = 0;
	m_nhull = 0;
}

void ConvexVolumeTool::handleMenu()
{
	ImGui::SliderFloat("Shape Height", &m_boxHeight, 0.1f, 20.0f);
	ImGui::SliderFloat("Shape Descent", &m_boxDescent, 0.1f, 20.0f);
	ImGui::SliderFloat("Poly Offset", &m_polyOffset, 0.0f, 10.0f);

	ImGui::Separator();

	ImGui::Text("Area Type");

	ImGui::Indent();

	if (ImGui::RadioButton("Ground", m_areaType == PolyArea::Ground))
		m_areaType = PolyArea::Ground;
	if (ImGui::RadioButton("Unwalkable", m_areaType == PolyArea::Unwalkable))
		m_areaType = PolyArea::Unwalkable;
#if 0
	if (ImGui::RadioButton("Grass", m_areaType == PolyArea::Grass))
		m_areaType = PolyArea::Grass;
	if (ImGui::RadioButton("Road", m_areaType == PolyArea::Road))
		m_areaType = PolyArea::Road;
	if (ImGui::RadioButton("Water", m_areaType == PolyArea::Water))
		m_areaType = PolyArea::Water;
	if (ImGui::RadioButton("Door", m_areaType == PolyArea::Door))
		m_areaType = PolyArea::Door;
#endif
	ImGui::Unindent();

	ImGui::Separator();

	if (ImGui::Button("Clear Shape"))
	{
		m_npts = 0;
		m_nhull = 0;
	}
}

void ConvexVolumeTool::handleClick(const glm::vec3& /*s*/, const glm::vec3& p, bool shift)
{
	if (!m_sample) return;
	InputGeom* geom = m_sample->getInputGeom();
	if (!geom) return;

	if (shift)
	{
		// Delete
		int nearestIndex = -1;
		const ConvexVolume* vols = geom->getConvexVolumes();
		for (int i = 0; i < geom->getConvexVolumeCount(); ++i)
		{
			if (pointInPoly(vols[i].nverts, vols[i].verts, p)
				&& p.y >= vols[i].hmin && p.y <= vols[i].hmax)
			{
				nearestIndex = i;
			}
		}

		// If end point close enough, delete it.
		if (nearestIndex != -1)
		{
			geom->deleteConvexVolume(nearestIndex);
		}
	}
	else
	{
		// Create
		bool alt = (SDL_GetModState() & KMOD_ALT) != 0;

		// If clicked on that last pt, create the shape.
		if (m_npts && (alt || distSqr(p, m_pts[m_npts - 1]) < rcSqr(0.2f)))
		{
			if (m_nhull > 2)
			{
				// Create shape.
				glm::vec3 verts[MAX_PTS];
				for (int i = 0; i < m_nhull; ++i)
					verts[i] = m_pts[m_hull[i]];

				float minh = FLT_MAX, maxh = 0;
				for (int i = 0; i < m_nhull; ++i)
					minh = glm::min(minh, verts[i].y);

				minh -= m_boxDescent;
				maxh = minh + m_boxHeight;

				if (m_polyOffset > 0.01f)
				{
					glm::vec3 offset[MAX_PTS * 2];
					int noffset = rcOffsetPoly(glm::value_ptr(verts[0]), m_nhull,
						m_polyOffset, glm::value_ptr(offset[0]), MAX_PTS * 2);
					if (noffset > 0)
					{
						geom->addConvexVolume(glm::value_ptr(offset[0]), noffset,
							minh, maxh, static_cast<uint8_t>(m_areaType));
					}
				}
				else
				{
					geom->addConvexVolume(glm::value_ptr(verts[0]), m_nhull,
						minh, maxh, static_cast<uint8_t>(m_areaType));
				}
			}

			m_npts = 0;
			m_nhull = 0;
		}
		else
		{
			// Add new point
			if (m_npts < MAX_PTS)
			{
				m_pts[m_npts++] = p;

				// Update hull.
				if (m_npts > 1)
					m_nhull = convexhull(m_pts, m_npts, m_hull);
				else
					m_nhull = 0;
			}
		}
	}

}

void ConvexVolumeTool::handleToggle()
{
}

void ConvexVolumeTool::handleStep()
{
}

void ConvexVolumeTool::handleUpdate(float /*dt*/)
{
}

void ConvexVolumeTool::handleRender()
{
	DebugDrawGL dd;

	// Find height extents of the shape.
	float minh = FLT_MAX, maxh = 0;
	for (int i = 0; i < m_npts; ++i)
		minh = glm::min(minh, m_pts[i].y);
	minh -= m_boxDescent;
	maxh = minh + m_boxHeight;

	dd.begin(DU_DRAW_POINTS, 4.0f);
	for (int i = 0; i < m_npts; ++i)
	{
		unsigned int col = duRGBA(255, 255, 255, 255);
		if (i == m_npts - 1)
			col = duRGBA(240, 32, 16, 255);
		dd.vertex(m_pts[i].x, m_pts[i].y + 0.1f, m_pts[i].z, col);
	}
	dd.end();

	dd.begin(DU_DRAW_LINES, 2.0f);
	for (int i = 0, j = m_nhull - 1; i < m_nhull; j = i++)
	{
		const glm::vec3& vi = m_pts[m_hull[j]];
		const glm::vec3& vj = m_pts[m_hull[i]];
		dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
		dd.vertex(vi.x, minh, vi.z, duRGBA(255, 255, 255, 64));
		dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
		dd.vertex(vi.x, maxh, vi.z, duRGBA(255, 255, 255, 64));
		dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
		dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
	}
	dd.end();
}

void ConvexVolumeTool::handleRenderOverlay(const glm::mat4& /*proj*/,
	const glm::mat4& /*model*/, const glm::ivec4& view)
{
	// Tool help
	if (!m_npts)
	{
		ImGui::RenderTextRight(-330, -(view[3] - 40), ImVec4(255, 255, 255, 192),
			"LMB: Create new shape.  SHIFT+LMB: Delete existing shape (click inside a shape).");
	}
	else
	{
		ImGui::RenderTextRight(-330, -(view[3] - 40), ImVec4(255, 255, 255, 192),
			"Click LMB to add new points. Alt+Click to finish the shape.");
		ImGui::RenderTextRight(-330, -(view[3] - 60), ImVec4(255, 255, 255, 192),
			"The shape will be convex hull of all added points.");
	}
}
