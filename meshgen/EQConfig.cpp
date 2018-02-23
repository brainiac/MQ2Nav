//
// EQConfig.cpp
//

#include "meshgen/EQConfig.h"

#include <atlbase.h>
#include <windows.h>
#include <ShObjIdl.h>

#include <algorithm>
#include <istream>
#include <string>
#include <sstream>

EQConfig::EQConfig()
{
	// Init COM library
	::CoInitializeEx(NULL,
		::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE);

	LoadConfigFromIni();
	LoadZones();
}

EQConfig::~EQConfig()
{
	SaveConfigToIni();

	::CoUninitialize();
}

static void CharToWChar(const char* inStr, wchar_t** outStr)
{
	int inStrByteCount = static_cast<int>(strlen(inStr));
	int charsNeeded = MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, NULL, 0);

	charsNeeded += 1; // terminator
	*outStr = new wchar_t[charsNeeded];

	MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, *outStr, charsNeeded);
	(*outStr)[charsNeeded - 1] = '\0';
}

static void WCharToChar(const wchar_t* inStr, char** outStr)
{
	int inStrCharacterCount = static_cast<int>(wcslen(inStr));
	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0,
		inStr, inStrCharacterCount, NULL, 0, NULL, NULL);

	bytesNeeded += 1;
	*outStr = new char[bytesNeeded];

	WideCharToMultiByte(CP_UTF8, 0, inStr, -1, *outStr, bytesNeeded, NULL, NULL);
}

static void SetDefaultPath(IFileDialog* dialog, const char* defaultPath)
{
	wchar_t* defaultPathW = nullptr;
	CharToWChar(defaultPath, &defaultPathW);

	IShellItem* folder = nullptr;
	HRESULT result = SHCreateItemFromParsingName(defaultPathW, NULL, IID_PPV_ARGS(&folder));

	// Valid non results.
	if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE))
	{
		delete[] defaultPathW;
		return;
	}

	if (!SUCCEEDED(result))
	{
		delete[] defaultPathW;
		return;
	}

	dialog->SetFolder(folder);
	delete[] defaultPathW;
	folder->Release();
}

static std::string OpenFileDialog(const char* defaultPath, const char* message)
{
	::IFileOpenDialog* fileOpenDialog = nullptr;
	std::string resultString;

	// Create dialog
	HRESULT result = ::CoCreateInstance(::CLSID_FileOpenDialog, NULL,
		CLSCTX_ALL, ::IID_IFileOpenDialog,
		reinterpret_cast<void**>(&fileOpenDialog));
	if (!SUCCEEDED(result))
	{
		return resultString;
	}

	SetDefaultPath(fileOpenDialog, defaultPath);

	wchar_t* title = nullptr;
	CharToWChar(message, &title);
	fileOpenDialog->SetTitle(title);
	delete[] title;

	fileOpenDialog->SetOptions(FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT);

	result = fileOpenDialog->Show(nullptr);
	if (SUCCEEDED(result))
	{
		::IShellItem* shellItem = nullptr;
		result = fileOpenDialog->GetResult(&shellItem);

		wchar_t* filePath = nullptr;
		shellItem->GetDisplayName(::SIGDN_FILESYSPATH, &filePath);

		char* outPath = nullptr;
		WCharToChar(filePath, &outPath);
		CoTaskMemFree(filePath);

		resultString = outPath;
		shellItem->Release();
	}

	return resultString;
}

std::string EQConfig::GetLongNameForShortName(const std::string& shortName)
{
	auto iter = m_mapNames.find(shortName);
	if (iter != m_mapNames.end())
		return iter->second;

	return std::string();
}

void EQConfig::LoadConfigFromIni()
{
	// Construct the path to the ini file
	CHAR fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);
	PathAppendA(fullPath, "MeshGenerator.ini");

	CHAR eqPath[MAX_PATH] = { 0 };
	bool changed = false;

	if (!GetPrivateProfileStringA("General", "EverQuest Path", "", eqPath, MAX_PATH, fullPath))
	{
		SelectEverquestPath();
	}
	else
	{
		m_everquestPath = eqPath;
	}

	CHAR outPath[MAX_PATH] = { 0 };

	if (!GetPrivateProfileStringA("General", "Output Path", "", outPath, MAX_PATH, fullPath))
	{
		SelectOutputPath();
	}
	else
	{
		m_outputPath = outPath;
	}

	char szTemp[10] = { 0 };
	GetPrivateProfileString("General", "ZoneMaxExtents", m_useMaxExtents ? "true" : "false",
		szTemp, 10, fullPath);
	m_useMaxExtents = !_stricmp(szTemp, "true");
}

void EQConfig::SaveConfigToIni()
{
	// Construct the path to the ini file
	CHAR fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);
	PathAppendA(fullPath, "MeshGenerator.ini");

	WritePrivateProfileString("General", "EverQuest Path", m_everquestPath.c_str(), fullPath);
	WritePrivateProfileString("General", "Output Path", m_outputPath.c_str(), fullPath);
	WritePrivateProfileString("General", "ZoneMaxExtents", m_useMaxExtents ? "true" : "false", fullPath);
}

void EQConfig::LoadZones()
{
	// Construct the path to the ini file
	CHAR fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);
	PathAppendA(fullPath, "Zones.ini");

	m_loadedMaps.clear();
	m_mapNames.clear();

	std::ifstream ifs(fullPath, std::ifstream::in);
	std::string line;
	std::string currentSection;
	ZoneList currentList;

	while (std::getline(ifs, line))
	{
		if (line[0] == '[' && line[line.length() - 1] == ']')
		{
			// move current data into the vector, if it exists
			if (!currentSection.empty())
			{
				m_loadedMaps.emplace_back(
					std::make_pair(std::move(currentSection), std::move(currentList)));
			}

			currentSection = line.substr(1, line.length() - 2);
			currentList.clear();
		}
		else
		{
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				// first half is long name, second half is shortname
				std::string longName = line.substr(0, pos);
				std::string shortName = line.substr(pos + 1);

				m_mapNames[shortName] = longName;
				currentList.emplace(std::make_pair(std::move(longName), std::move(shortName)));
			}
		}
	}

	// move current data into the vector, if it exists
	if (!currentSection.empty())
	{
		m_loadedMaps.emplace_back(
			std::make_pair(std::move(currentSection), std::move(currentList)));
	}
}

void EQConfig::SelectEverquestPath()
{
	std::string result = OpenFileDialog("c:\\Program Files (x86)\\Sony\\EverQuest", "Find Everquest Directory");

	if (!result.empty())
	{
		m_everquestPath = result;
		SaveConfigToIni();
	}
}

void EQConfig::SelectOutputPath()
{
	std::string result = OpenFileDialog("c:\\", "Find Output (MQ2) Directory");

	if (!result.empty())
	{
		m_outputPath = result;
		SaveConfigToIni();
	}
}
