/*****************************************************************************
EQDraw.cpp

Copyright (C) 2009 ieatacid

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
******************************************************************************/
#undef CINTERFACE
#include "../../../../MQ2Main/MQ2Main.h"
#include "EQDraw.h"


void CEQDraw::AdjustItemCoords(PGROUNDITEM pItem, EQVector3 &v3)
{
// wassup/agripa maths -- thanks!
#define FixItemY(n) (n * (float)sin(2 * PI / 512 * (pItem->pSwitch->Heading + 128)))
#define FixItemX(n) (n * (float)-cos(2 * PI / 512 * (pItem->pSwitch->Heading + 128)))

    DWORD itemID = atoi(pItem->Name + 2);

    // set default item coords
    v3.y = pItem->pSwitch->Y;
    v3.x = pItem->pSwitch->X;
    v3.z = pItem->pSwitch->Z;

    float fAdjust = pItem->pSwitch->HeightAdjustment;

    switch(itemID)
    {
    case 10: // forgot to comment what this is
        break;
    case 74: // pottery wheel in PoK
        v3.y += FixItemY(2.5f);
        v3.x += FixItemX(2.5f);
        v3.z += fAdjust * 0.5f;
        break;
    case 10645: // forgot to comment what this is
        if(GetCharInfo()->zoneId == 202)
        {
            if(pItem->DropID == 57) // book in PoK library, 3rd floor
                break;
            else if(pItem->DropID == 17) // book in PoK library, 2nd floor
                v3.z += fAdjust * 0.5f;
            else
                v3.z += fAdjust;
        }
        break;
    case 10649: // forgot to comment what this is
    case 10686: // forgot to comment what this is
        break;
    case 10802: // forgot to comment what this is
        v3.y = pItem->Y;
        v3.x = pItem->X;
        v3.z = pItem->Z;
        break;
    default:
        v3.z += fAdjust * 0.5f;
        break;
    }   
}

void CEQDraw::AdjustDoorCoords(PDOOR pDoor, EQVector3 &v3)
{
    // set default door coords
    v3.y = pDoor->Y;
    v3.x = pDoor->X;
    v3.z = pDoor->Z;

    // it's not in the adjustment table? RETURN!
    if(!pDoorAdjustTable->pDoor[pDoor->ID]) return;

    /** THESE WERE A NIGHTMARE TO HANLDE **/
    // toggle switches, draw bridges, etc
    if(pDoorAdjustTable->pDoor[pDoor->ID]->special && pDoor->pSwitch)
    {
        float fOut = 1;
        if(pDoorAdjustTable->pDoor[pDoor->ID]->out != 0)
            fOut = pDoorAdjustTable->pDoor[pDoor->ID]->out;
        if(pDoorAdjustTable->pDoor[pDoor->ID]->special == 1)
        {
            if(pDoor->pSwitch->xAdjustment2 == 1)
            {
                if(pDoor->pSwitch->Heading < 128 || pDoorTarget->pSwitch->Heading > 384)
                {
                    v3.y -= pDoor->pSwitch->yAdjustment1 * fOut;
                    v3.z -= pDoor->pSwitch->zAdjustment1 * fOut;
                }
                else
                {
                    v3.y += pDoor->pSwitch->yAdjustment3 * fOut;
                    v3.z += pDoor->pSwitch->zAdjustment3 * fOut;
                }
            }
            else if(pDoor->pSwitch->xAdjustment2 == -1)
            {
                v3.y -= pDoor->pSwitch->yAdjustment1 * fOut;
                v3.z -= pDoor->pSwitch->zAdjustment1 * fOut;
            }
            else if(pDoor->pSwitch->yAdjustment2 == 1)
            {
                v3.x += pDoor->pSwitch->xAdjustment3 * fOut;
                v3.z += pDoor->pSwitch->zAdjustment3 * fOut;
            }
        }
        else if(pDoorAdjustTable->pDoor[pDoor->ID]->special == 2)
            if(pDoor->pSwitch->yAdjustment2 == 1)
            {
                v3.x -= pDoor->pSwitch->xAdjustment1 * fOut;
                v3.z -= pDoor->pSwitch->zAdjustment1 * fOut;
            }
            else if(pDoor->pSwitch->xAdjustment2 == -1)
            {
                v3.y -= pDoor->pSwitch->yAdjustment1 * fOut;
                v3.z -= pDoor->pSwitch->zAdjustment1 * fOut;
            }
        else if(pDoorAdjustTable->pDoor[pDoor->ID]->special == 3)
        {
            v3.y += fOut;
            v3.x += fOut;
            v3.z += pDoorAdjustTable->pDoor[pDoor->ID]->z;
        }
        return;
    }
    // normal (if you can call them that) doors
    else if(pDoorAdjustTable->pDoor[pDoor->ID])
    {
        float fOut = pDoorAdjustTable->pDoor[pDoor->ID]->out;
        float fOr = pDoorAdjustTable->pDoor[pDoor->ID]->orientation;
        float fZ = pDoorAdjustTable->pDoor[pDoor->ID]->z;
        if(fOr < 512)
        {
            // wassup/agripa maths -- thanks!
            v3.y += fOut * cosf((pDoor->Heading + fOr) / 256 * (float)PI);
            v3.x += fOut * sinf((pDoor->Heading + fOr) / 256 * (float)PI);
        }
        v3.z += fZ;
    }
}

// clear the adjustment table
void CEQDraw::ClearAdjustmentTable()
{
    if(pDoorAdjustTable && pDoorAdjustTable->numDoors)
		for(DWORD n = 0; n < pDoorAdjustTable->numDoors; n++)
			if(pDoorAdjustTable->pDoor[n])
			{
				DoorAdjust *pDoor = pDoorAdjustTable->pDoor[n];
				delete pDoor;
				pDoorAdjustTable->pDoor[n] = 0;
			}
}

// load DoorAdjustments.txt, match door with doors in the zone, apply to door adjust table
bool CEQDraw::LoadDoorAdjustments()
{
    char buffer[512];
    sprintf(buffer, "%s\\DoorAdjustments.txt", gszINIPath);    
    if(!pDoorAdjustTable)
    {
        pDoorAdjustTable = new DoorAdjustTable;
        pDoorAdjustTable->numDoors = 0;
	}
    DoorAdjustFile.clear();
    ClearAdjustmentTable();
    if(FILE *fFile = fopen(buffer, "rt"))
        while(fgets(buffer, 512, fFile))
        {
            if(buffer[0] == '0' || buffer[0] == '\n') continue;
            char szName[32] = {0};
            strncpy(szName, buffer, strchr(buffer, '^') - &buffer[0]);
            char *pRest = strchr(buffer, '^') + 1;
            DoorAdjustFile[szName] = pRest;
        }
    else
    {
        WriteChatf("[MQ2Navigation] Error opening DoorAdjustments.txt");
        return false;
    }
    PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
    pDoorAdjustTable->numDoors = pDoorTable->NumEntries;
    for(DWORD i = 0; i < pDoorTable->NumEntries; i++)
    {
        BYTE cID = pDoorTable->pDoor[i]->ID;
        if(DoorAdjustFile[pDoorTable->pDoor[i]->Name].length())
        {
            char szTemp[512];
            strcpy(szTemp, DoorAdjustFile[pDoorTable->pDoor[i]->Name].c_str());
            szTemp[strlen(szTemp) - 1] = 0;
            DoorAdjust *pDoorAdjust = new DoorAdjust;
			ZeroMemory(pDoorAdjust, sizeof(DoorAdjust));
			CAdjustmentParser p(szTemp);
            p.FindDoor(GetCharInfo()->zoneId, pDoorTable->pDoor[i]->ID);
			pDoorAdjust->orientation = p.ReadFloat();
			pDoorAdjust->out = p.ReadFloat();
			pDoorAdjust->z = p.ReadFloat();
			pDoorAdjust->special = p.ReadInt();
            pDoorAdjustTable->pDoor[cID] = pDoorAdjust;
        }
        else
            pDoorAdjustTable->pDoor[cID] = 0;
    }
    DoorAdjustFile.clear();
    return true;
}

void CEQDraw::Click(EQVector3 v3world) {
	ScreenVector3 v3screen;
	if(WorldToScreen(v3world.y, v3world.x, v3world.z, &v3screen))
    {
		static int CameraMode;
		#define CAMERA_MODE *(int*)(((char*)pinstViewActor)+0x14)
		if ( (int)CAMERA_MODE != 0 ) {
			CameraMode = (int)CAMERA_MODE;
			CAMERA_MODE=0;
		}
		*((DWORD*)__LMouseHeldTime)=((PCDISPLAY)pDisplay)->TimeStamp-0x45;
		pEverQuest->LMouseUp(v3screen.x,v3screen.y);
        gLClickedObject=true;
        EnviroTarget.Name[0]=0;
		if ( CameraMode != 0 ) {
			CAMERA_MODE = CameraMode;
			CameraMode = 0;
		} 
	}
}

void CEQDraw::Click(PGROUNDITEM pItem)
{
    if(pItem && bInitialized)
    {
		EQVector3 v3world;
        AdjustItemCoords(pItem, v3world);
		Click(v3world);
    }
}

void CEQDraw::Click(PDOOR pDoor)
{
    if(pDoor && bInitialized)
    {
		EQVector3 v3world;
        AdjustDoorCoords(pDoor, v3world);
		Click(v3world);
    }
}

void CEQDraw::Initialize()
{
	if (bInitialized) return;
	if(pRender = (CRender*)g_pDrawHandler) {
		pD3Ddevice = pRender->pDevice;
		if(pD3Ddevice)
			bInitialized = true;
	}
}

bool CEQDraw::WorldToScreen(float spawnY, float spawnX, float spawnZ, ScreenVector3 *v3ScreenCoord)
{
    g_vWorldLocation.x = spawnY;
    g_vWorldLocation.y = spawnX;
    g_vWorldLocation.z = spawnZ;
 
    pD3Ddevice->GetTransform(D3DTS_VIEW, &g_view);
    pD3Ddevice->GetTransform(D3DTS_PROJECTION, &g_projection);
    pD3Ddevice->GetTransform(D3DTS_WORLD, &g_world);
    pD3Ddevice->GetViewport(&g_viewPort);

    D3DXVec3Project((D3DXVECTOR3*)v3ScreenCoord, &g_vWorldLocation, &g_viewPort, &g_projection, &g_view, &g_world);

    if(v3ScreenCoord->z >= 1)
        return false;

    return true;
}

CEQDraw::CEQDraw() 
:	DoorAdjustFile(),
	pDoorAdjustTable(NULL),
	pRender(NULL),
	pD3Ddevice(NULL),
	g_viewPort(),
	g_projection(), 
	g_view(),
	g_world(),
	g_vWorldLocation(),
	bInitialized(false) {
	if(!LoadDoorAdjustments()) {
		WriteChatf("[MQ2Navigation] CEQDraw::LoadDoorAdjustments() failed.");
		WriteChatf("[MQ2Navigation] Ensure DoorAdjustments.txt is in your release folder then reload MQ2Navigation.");
	} 
}

CEQDraw::~CEQDraw() {
    if(pDoorAdjustTable)
    {
        ClearAdjustmentTable();
        delete pDoorAdjustTable;
        pDoorAdjustTable = 0;
    }
}