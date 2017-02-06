//
// DebugDraw.h
//

#pragma once

#include <DebugDraw.h>
#include <Recast.h>
#include <RecastDump.h>

// OpenGL debug draw implementation.
class DebugDrawGL : public duDebugDraw
{
public:
	virtual void depthMask(bool state) override;
	virtual void texture(bool state) override;
	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override;
	virtual void vertex(const float* pos, unsigned int color) override;
	virtual void vertex(const float x, const float y, const float z, unsigned int color) override;
	virtual void vertex(const float* pos, unsigned int color, const float* uv) override;
	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override;
	virtual void end() override;
};
