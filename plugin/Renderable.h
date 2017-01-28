//
// Renderable.h
//
// Defines a base class for something that can be rendered

#pragma once

//----------------------------------------------------------------------------

class Renderable
{
public:
	virtual ~Renderable() {}

	enum RenderPhase {
		Render_Geometry   = 0x1, // render will be called during 3d geometry pass
		Render_UI         = 0x2, // render will be called during 2d ui pass
	};

	virtual void Render(RenderPhase phase) = 0;
	virtual bool CreateDeviceObjects() = 0;
	virtual void InvalidateDeviceObjects() = 0;
};
