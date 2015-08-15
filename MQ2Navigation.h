// MQ2Navigation.h : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#ifndef __MQ2NAVIGATION_H
#define __MQ2NAVIGATION_H


#include "../MQ2Plugin.h"


class dtNavMesh;

namespace mq2nav {


	enum {
		
		MAX_POLYS = 4028,

		WAYPOINT_PROGRESSION_DISTANCE = 5, // minimum distance to a waypoint for moving to the next waypoint
		ENDPOINT_STOP_DISTANCE = 15, // stoping distance at the final way point

		PATHFINDING_DELAY = 2000 // how often to update the path (milliseconds)
	};


	extern bool initialized_;

   extern PDOOR m_pEndingDoor;
   extern PGROUNDITEM m_pEndingItem;

   extern bool m_bSpamClick;
   extern bool m_bDoMove;

   extern float m_destination[3];
   extern float m_currentPath[MAX_POLYS*3];
   extern float m_candidatePath[MAX_POLYS*3];

   extern int m_currentPathCursor;
   extern int m_currentPathSize;

	extern dtNavMesh* m_navMesh;




	void AttemptClick();
	bool ClickNearestClosedDoor(FLOAT cDistance = 30);

	void StuckCheck();
	void LookAt(FLOAT X,FLOAT Y,FLOAT Z);
	void AttemptMovement();


	bool IsInt(char* buffer);
	class dtNavMesh* LoadMesh();
	bool FindPath(double X, double Y, double Z);
	VOID NavigateCommand(PSPAWNINFO pChar, PCHAR szLine);	
	bool ValidEnd();
	void Stop();


} // namespace mq2nav {


#endif