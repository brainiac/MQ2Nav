
#include "pch.h"

#include "imgui.h"
#include "imguiRenderGL.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "InputGeom.h"
#include "Sample_TileMesh.h"
#include "Sample_Debug.h"

#include "Interface.h"

#include "zone-utilities/log/log_macros.h"
#include "zone-utilities/log/log_stdout.h"
#include "zone-utilities/log/log_file.h"

#include <time.h>
#include <memory>

#undef main

#if 0
// logger to log to the build context
class LogContext : public EQEmu::Log::LogBase
{
public:
	LogContext(BuildContext& ctx) : m_ctx(ctx) {}
	virtual ~LogContext() {}

	virtual void OnRegister(int enabled_logs) {}
	virtual void OnUnregister() {}
	virtual void OnMessage(EQEmu::Log::LogType log_type, const std::string &message)
	{
		char time_buffer[512];
		time_t current_time;
		struct tm *time_info;

		time(&current_time);
		time_info = localtime(&current_time);

		strftime(time_buffer, 512, "[%m/%d/%y %H:%M:%S] ", time_info);

		switch (log_type)
		{
		case EQEmu::Log::LogTrace:
			m_ctx.log(RC_LOG_PROGRESS, "[Trace]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogDebug:
			m_ctx.log(RC_LOG_PROGRESS, "[Debug]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogInfo:
			m_ctx.log(RC_LOG_PROGRESS, "[Info]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogWarn:
			m_ctx.log(RC_LOG_WARNING, "[Warn]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogError:
			m_ctx.log(RC_LOG_ERROR, "[Error]%s%s\n", time_buffer, message.c_str());
			break;
		case EQEmu::Log::LogFatal:
			m_ctx.log(RC_LOG_ERROR, "[Fatal]%s%s\n", time_buffer, message.c_str());
			break;
		default:
			m_ctx.log(RC_LOG_PROGRESS, "[All]%s%s\n", time_buffer, message.c_str());
			break;
		}
	}

private:
	BuildContext& m_ctx;
};

struct FileList
{
	static const int MAX_FILES = 2048;
	inline FileList() : size(0) {}
	inline ~FileList()
	{
		clear();
	}

	void clear()
	{
		for (int i = 0; i < size; ++i)
			delete[] files[i];
		size = 0;
	}

	void add(const char* path)
	{
		if (size >= MAX_FILES)
			return;
		int n = strlen(path);
		files[size] = new char[n + 1];
		strcpy(files[size], path);
		size++;
	}

	static int cmp(const void* a, const void* b)
	{
		return strcmp(*(const char**)a, *(const char**)b);
	}

	void sort()
	{
		if (size > 1)
			qsort(files, size, sizeof(char*), cmp);
	}

	char* files[MAX_FILES];
	int size;
};

int LastIndexOf(TCHAR *text, TCHAR key) {
	int index = -1;
	for (unsigned int x = 0; x < _tcslen(text); x++)
		if (text[x] == key) index = x;
	return index + 1;
}

int contains(char* source, char key) {
	for (unsigned int i = 0; i < strlen(source); i++)
		if (source[i] == key) return i;
	return 0;
}

char* wtoc(char* Dest, TCHAR* Source, int SourceSize)
{
	for (int i = 0; i < SourceSize; ++i)
		Dest[i] = (char)Source[i];
	return Dest;
}

char* findEQPath(bool bypassSettings)
{
	LPTSTR path = new TCHAR[MAX_PATH];
	TCHAR *buffer = new TCHAR[MAX_PATH];
	char *cPath = new char[MAX_PATH];
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("EQNavigation.ini"));

	if (!GetPrivateProfileString("General", "EverQuest Path", "", path, MAX_PATH, FullPath) || bypassSettings)
	{
		if (BrowseForFolder(NULL, "C:\\Program Files\\Sony\\EverQuest", path, "Select EverQuest Folder"))
		{
			WIN32_FIND_DATA FindFileData;
			HANDLE hFind;
			TCHAR *pathstr = new TCHAR[MAX_PATH];
			wsprintf(pathstr, TEXT("%s\\eqgame.exe"), path);
			hFind = FindFirstFile(pathstr, &FindFileData);
			if (hFind != INVALID_HANDLE_VALUE)
				FindClose(hFind);
			else
			{
				TCHAR *message = new TCHAR[MAX_PATH];
				wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), path);
				if (MessageBox(NULL, message,
					"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
					return findEQPath(bypassSettings);
			}
			/*
			memcpy(buffer,path+ LastIndexOf(path,'\\'),MAX_PATH);
			if(_tcscmp(buffer,"EverQuest")) {
			TCHAR *message = new TCHAR[MAX_PATH];
			wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), buffer);
			if (MessageBox(NULL,message,
			"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
			return findEQPath(bypassSettings);
			}
			*/
			WritePrivateProfileString("General", "EverQuest Path", path, FullPath);
		}
		else
			return NULL;
	}
	return wtoc(cPath, path, MAX_PATH);
}

char* findMQPath(bool bypassSettings)
{
	LPTSTR path = new TCHAR[MAX_PATH];
	TCHAR *buffer = new TCHAR[MAX_PATH];
	char *cPath = new char[MAX_PATH];
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("EQNavigation.ini"));
	if (!GetPrivateProfileString("General", "Output Path", "", path, MAX_PATH, FullPath) || bypassSettings) {
		if (BrowseForFolder(NULL, "C:\\", path, "Select Mesh Folder"))
		{
			/*
			memcpy(buffer,path+ LastIndexOf(path,'\\'),MAX_PATH);
			if(_tcscmp(buffer,"Release")) {
			TCHAR *message = new TCHAR[MAX_PATH];
			wsprintf(message, TEXT("Do you intend to parse zone files from %s?"), buffer);
			if (MessageBox(NULL,message,
			"Abnormal Directory Name", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO)
			return findMQPath(bypassSettings);
			}
			*/
			WritePrivateProfileString("General", "Output Path", path, FullPath);
		}
		else
			return NULL;
	}
	return wtoc(cPath, path, MAX_PATH);
}

static char* parseRow(char* buf, char* bufEnd, char* row, int len)
{
	bool cont = false;
	bool start = true;
	bool done = false;
	int n = 0;
	while (!done && buf < bufEnd)
	{
		char c = *buf;
		buf++;
		// multirow
		switch (c)
		{
		case '\\':
			cont = true; // multirow
			break;
		case '\n':
			if (start) break;
			done = true;
			break;
		case '\r':
			break;
		case '\t':
		default:
			start = false;
			cont = false;
			row[n++] = c;
			if (n >= len - 1)
				done = true;
			break;
		}
	}
	row[n] = '\0';
	return buf;
}
char* Mid(char * dest, const char * src, int start, int length) {
	strcpy(dest, src + start);
	dest[length] = 0;
	return dest;
}

void loadZones(FileList& list)
{
	TCHAR FullPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, FullPath, MAX_PATH);
	PathRemoveFileSpec(FullPath);
	PathAppend(FullPath, _T("Zones.ini"));
	list.clear();
	if (FILE* fp = fopen(FullPath, "rb")) {
		fseek(fp, 0, SEEK_END);
		int bufSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char* buf = new char[bufSize];
		if (!buf)
		{
			fclose(fp);
			return;
		}
		fread(buf, bufSize, 1, fp);
		fclose(fp);
		int index;
		char* buffer = new char[256];
		char* src = buf;
		char* srcEnd = buf + bufSize;
		char row[512];
		char* charValue1 = new char[256];
		while (src < srcEnd)
		{
			// Parse one row
			row[0] = '\0';
			src = parseRow(src, srcEnd, row, sizeof(row) / sizeof(char));
			// Skip comments
			if (row[0] == ';') continue;
			if (row[0] == '[' && row[strlen(row) - 1] == ']' && row[1] != 't')
			{
				// section name
			}
			else if (index = contains(row, '=')) {
				list.add(Mid(charValue1, row, 0, index));
			}
		}
		delete[] buf;
		list.sort();
	}
	else
	{
		printf("Zones.ini not found");
	}
}
#endif

int main(int argc, char* argv[])
{
	eqLogInit(-1);
	eqLogRegister(std::make_shared<EQEmu::Log::LogFile>("eqnav.log"));
	eqLogRegister(std::make_shared<EQEmu::Log::LogStdOut>());
	//eqLogRegister(std::make_shared<LogContext>(ctx));

	Interface window("draniksscar");
	return window.RunMainLoop();
}

//----------------------------------------------------------------------------
bool _DEBUG_LOG = false;                    /* generate debug messages      */
char _DEBUG_LOG_FILE[260];                  /* log file path        */
bool debug_log_first_start = TRUE;
char  log_file_name[256];                   /* file name for the log file   */
DWORD debug_start_time;                     /* starttime of debug           */
BOOL  debug_started = FALSE;                /* debug_log writes only if     */

/* add a line to the log file                                               */
void debug_log_proc(char* text, char* sourcefile, int sourceline)
{
	FILE* fp;
	if (!_DEBUG_LOG)
		return;

	if (debug_started == FALSE)
		return;        /* exit if not initialized      */

	if ((fp = fopen(log_file_name, "at")) != NULL) /* append to logfile  */
	{
		fprintf(fp, " %8u ms (line %u in %s):\n              - %s\n",
			(unsigned int)(GetTickCount() - debug_start_time),
			sourceline, sourcefile, text);
		fclose(fp);
	}
}
