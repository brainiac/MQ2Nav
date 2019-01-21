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

	virtual void Render() {};

	virtual bool CreateDeviceObjects() = 0;
	virtual void InvalidateDeviceObjects() = 0;
};
