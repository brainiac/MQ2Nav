/*****************************************************************************
EQDraw.h

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
#include <map>
#include <string>
#include "EQGraphics.h"
#include "EQDraw.h"

class EQVector3
{
public:
    EQVector3(float Y, float X, float Z)
    {
        y = Y;
        x = X;
        z = Z;
    };
    
    EQVector3()
    {
        Clear();
    };
    
    void Clear()
    {
        y = 0;
        x = 0;
        z = 0;
    };

    float y;
    float x;
    float z;
};

struct DoorAdjust
{
    float out;
    float z;
    float orientation;
    int   special;
};

struct DoorAdjustTable
{
    DWORD      numDoors;
    DoorAdjust *pDoor[0xFF];
};

class CAdjustmentParser
{
public:
    CAdjustmentParser(char *line) : m_szLine(line) {};

    bool FindDoor(DWORD zoneID, DWORD doorID)
    {
        DWORD zID = 0;
        DWORD dID = 0;

        for(unsigned int n = 0; n < strlen(m_szLine); n++)
        {
            if(m_szLine[n] == '(')
            {
                sscanf(&m_szLine[n], "(%u,%u)", &zID, &dID);
                if(zID == zoneID && dID == doorID)
                {
                    m_szLine = strchr(&m_szLine[n], ')') + 1;
                    return true;
                }
            }
        }

        return false;
    }

    float ReadFloat()
    {
        char szFlt[32] = {0};

        unsigned int n = 0;
        unsigned int len = strlen(m_szLine);

        for(; n < len && *m_szLine != '^' && *m_szLine != ' '; n++, m_szLine++)
        {
            szFlt[n] = *m_szLine;
        }

        if(n)
        {
            m_szLine++;
            return (float)atof(szFlt);
        }

        return 0;
    }

    int ReadInt()
    {
        char szInt[32] = {0};

        unsigned int len = strlen(m_szLine);
        unsigned int n = 0;

        for(; n < len && *m_szLine != '^' && *m_szLine != ' '; n++, m_szLine++)
        {
            szInt[n] = *m_szLine;
        }

        if(n)
        {
            m_szLine++;
            return atoi(szInt);
        }
        
        return 0;
    }

private:
    char *m_szLine;
};


// Screen coordinate struct (same as D3DVECTOR)
struct ScreenVector3
{
    float x; // left to right screen coordinate
    float y; // top to bottom screen coordinate
    float z;
};


// Drawing interface class
class CEQDraw
{
private:
    // Returns screen coordinates in v3ScreenCoord for world y/x/z coordinates.
    // If out of view, the function returns false.
    bool WorldToScreen(float worldY, float worldX, float worldZ, ScreenVector3 *v3ScreenCoord);

	void Click(EQVector3 v3world);
	void ClearAdjustmentTable();
	void AdjustItemCoords(PGROUNDITEM pItem, EQVector3 &v3);
	void AdjustDoorCoords(PDOOR pDoor, EQVector3 &v3);

	map<string, string> DoorAdjustFile;
	DoorAdjustTable *pDoorAdjustTable;
	CRender *pRender;
	IDirect3DDevice9 *pD3Ddevice;
	D3DVIEWPORT9 g_viewPort;
	D3DXMATRIX g_projection, g_view, g_world;
	D3DXVECTOR3 g_vWorldLocation;
	bool bDoorsLoaded;

	bool bInitialized;
public:
	CEQDraw();
	~CEQDraw();

	void Click(PDOOR pDoor);
	void Click(PGROUNDITEM pItem);
    void Initialize();
	bool LoadDoorAdjustments();
};