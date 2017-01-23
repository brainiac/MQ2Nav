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

#pragma once

#include "Sample.h"

#include <cstdint>

// Tool to create off-mesh connection for InputGeom

class OffMeshConnectionTool : public SampleTool
{
public:
	OffMeshConnectionTool();
	~OffMeshConnectionTool();
	
	virtual int type() override { return TOOL_OFFMESH_CONNECTION; }
	virtual void init(Sample* sample) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) override;
	virtual void handleToggle() override;
	virtual void handleStep() override;
	virtual void handleUpdate(float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;

private:
	Sample* m_sample = nullptr;
	glm::vec3 m_hitPos;
	bool m_hitPosSet = false;
	bool m_bidir = true;
	int8_t m_oldFlags = 0;
};
