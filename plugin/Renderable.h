//
// Renderable.h
//
// Defines a base class for something that can be rendered

#pragma once

#include <string>

//----------------------------------------------------------------------------

class Renderable
{
public:
	Renderable(const std::string& name);
	virtual ~Renderable() {}

	enum RenderPhase {
		Render_Geometry   = 0x1, // render will be called during 3d geometry pass
		Render_UI         = 0x2, // render will be called during 2d ui pass
	};

	virtual void Render(RenderPhase phase) = 0;
	virtual bool CreateDeviceObjects() = 0;
	virtual void InvalidateDeviceObjects() = 0;

	const std::string& GetName() const {
		return m_name;
	};

private:
	std::string m_name;
};
