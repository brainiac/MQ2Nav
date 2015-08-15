// EQNavigation.cpp : Defines the entry point for the console application.
// Build Version(0.01a)

#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <tchar.h>

#include "Interface.h"

#define APPENDTEXT   "at"
bool _DEBUG_LOG = false;                      /* generate debug messages      */
char _DEBUG_LOG_FILE[260];            /* log file path        */
bool debug_log_first_start = TRUE;
char  log_file_name[256];                   /* file name for the log file   */
DWORD debug_start_time;                     /* starttime of debug           */
BOOL  debug_started = FALSE;                /* debug_log writes only if     */

int _tmain(int argc, _TCHAR* argv[])
{
	Interface window;
	window.ShowDialog();
	return 0;
}

void debug_log_proc(char *text, char *sourcefile, int sourceline)
	/* add a line to the log file                                               */
{
	FILE *fp;
	if(!_DEBUG_LOG)
		return;

	if(debug_started == FALSE) return;        /* exit if not initialized      */

	if((fp = fopen(log_file_name, APPENDTEXT)) != NULL) /* append to logfile  */

	{
		fprintf(fp, " %8u ms (line %u in %s):\n              - %s\n",
			(unsigned int)(GetTickCount() - debug_start_time),
			sourceline, sourcefile, text);
		fclose(fp);
	}
}
