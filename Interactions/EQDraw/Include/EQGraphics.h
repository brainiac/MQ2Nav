/*****************************************************************************
EQGraphics.h

Copyright (C) 2009 ieatacid

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
******************************************************************************/

#pragma once

#include "d3dx9math.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")


class CRender
{
public:
    int DrawColoredLine(D3DXVECTOR3 &point1, D3DXVECTOR3 &point2, DWORD color);
    int DrawColoredText(DWORD size, char *text, RECT &rect1, RECT &rect2, DWORD color, unsigned short unk1 = 1, int unk2 = 0);
    int DrawColoredRect(struct VRECT &rect, DWORD color);

    /*0x000*/ BYTE Unknown0x0[0xf08];
    /*0xf08*/ IDirect3DDevice9 *pDevice; // device pointer
};

// used for drawing quadrilaterals:  coordinates of the four corners.
// changing z doesn't seem to affect anything.
struct VRECT
{
    // top left point
    float tl_x;
    float tl_y;
    float tl_z;

    // top right point
    float tr_x;
    float tr_y;
    float tr_z;

    // bottom right point
    float br_x;
    float br_y;
    float br_z;

    // bottom left point
    float bl_x;
    float bl_y;
    float bl_z;
};