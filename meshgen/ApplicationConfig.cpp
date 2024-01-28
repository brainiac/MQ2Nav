//
// EQConfig.cpp
//

#include "meshgen/ApplicationConfig.h"
#include <mq/base/Config.h>

#include <windows.h>
#include <ShObjIdl.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <istream>
#include <string>
#include <sstream>

#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

ApplicationConfig g_config;

ApplicationConfig::ApplicationConfig()
{
	// Init COM library
	::CoInitializeEx(nullptr, ::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE);
}

ApplicationConfig::~ApplicationConfig()
{
	::CoUninitialize();
}

static bool ValidateEverQuestPath(const fs::path& everquestPath)
{
	fs::path eqgame_file = everquestPath / "eqgame.exe";
	std::error_code ec;

	return fs::is_regular_file(eqgame_file, ec);
}

static bool ValidateMacroQuestPath(const fs::path& macroquestPath)
{
	fs::path macroquest_file = macroquestPath / "MacroQuest.exe";
	std::error_code ec;

	return fs::is_regular_file(macroquest_file, ec);
}

void ApplicationConfig::SetEverquestPath(const std::string& everquestPath)
{
	SPDLOG_DEBUG("Setting EverQuest Path: {}", everquestPath);

	m_everquestPath = everquestPath;
	mq::WritePrivateProfileString("General", "EverQuest Path", m_everquestPath.c_str(), m_settingsFile);
}

static bool InitDirectory(std::string& strPathToInit, const std::string& strIniKey, const std::string& iniToRead,
	const fs::path& appendPathIfRelative = ".")
{
	fs::path pathToInit = mq::GetPrivateProfileString("MacroQuest", strIniKey, strPathToInit, iniToRead);

	if (pathToInit.is_relative())
	{
		pathToInit = fs::absolute(appendPathIfRelative / pathToInit);
	}

	strPathToInit = pathToInit.string();
	std::error_code ec;

	if (fs::exists(pathToInit, ec) || fs::create_directories(pathToInit, ec))
	{
		return true;
	}

	return false;
}

void ApplicationConfig::SetMacroQuestPath(const std::string& macroquestPath)
{
	if (m_mqPath == macroquestPath)
		return;

	SPDLOG_DEBUG("Setting MacroQuest Path: {}", macroquestPath);

	m_mqPath = macroquestPath;
	m_mqPathValid = ValidateMacroQuestPath(m_mqPath);

	std::string strConfig = "Config";
	std::string strMQini = strConfig + "\\MacroQuest.ini";

	strMQini = mq::GetCreateMacroQuestIni(m_mqPath, strConfig, strMQini);

	if (InitDirectory(strConfig, "ConfigPath", strMQini, m_mqPath))
	{
		m_configPath = strConfig;
	}
	else
	{
		m_configPath = (fs::path(m_mqPath) / "Config").lexically_normal().string();
	}

	if (std::string strResources = "Resources"; InitDirectory(strResources, "ResourcePath", strMQini, m_mqPath))
	{
		m_resourcesPath = strResources;
	}
	else
	{
		m_resourcesPath = (fs::path(m_mqPath) / "Resources").lexically_normal().string();
	}

	if (std::string strLogsPath = "Logs"; InitDirectory(strLogsPath, "LogPath", strMQini, m_mqPath))
	{
		m_logsPath = strLogsPath;
	}
	else
	{
		m_logsPath = (fs::path(m_mqPath) / "Logs").lexically_normal().string();
	}

	m_outputPath = (fs::path(m_resourcesPath) / "MQ2Nav").lexically_normal().string();

	mq::WritePrivateProfileString("General", "MacroQuestPath", m_mqPath.c_str(), m_settingsFile);
}

static void CharToWChar(const char* inStr, wchar_t** outStr)
{
	int inStrByteCount = static_cast<int>(strlen(inStr));
	int charsNeeded = MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, nullptr, 0);

	charsNeeded += 1; // terminator
	*outStr = new wchar_t[charsNeeded];

	MultiByteToWideChar(CP_UTF8, 0, inStr, inStrByteCount, *outStr, charsNeeded);
	(*outStr)[charsNeeded - 1] = '\0';
}

static void WCharToChar(const wchar_t* inStr, char** outStr)
{
	int inStrCharacterCount = static_cast<int>(wcslen(inStr));
	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0,
		inStr, inStrCharacterCount, nullptr, 0, nullptr, nullptr);

	bytesNeeded += 1;
	*outStr = new char[bytesNeeded];

	WideCharToMultiByte(CP_UTF8, 0, inStr, -1, *outStr, bytesNeeded, nullptr, nullptr);
}

static void SetDefaultPath(IFileDialog* dialog, const char* defaultPath)
{
	wchar_t* defaultPathW = nullptr;
	CharToWChar(defaultPath, &defaultPathW);

	IShellItem* folder = nullptr;
	HRESULT result = SHCreateItemFromParsingName(defaultPathW, nullptr, IID_PPV_ARGS(&folder));

	// Valid non results.
	if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE))
	{
		delete[] defaultPathW;
		return;
	}

	if (FAILED(result))
	{
		delete[] defaultPathW;
		return;
	}

	dialog->SetFolder(folder);
	delete[] defaultPathW;
	folder->Release();
}

static std::string OpenFileDialog(const char* defaultPath, const char* message, HRESULT& result)
{
	::IFileOpenDialog* fileOpenDialog = nullptr;
	std::string resultString;

	// Create dialog
	result = ::CoCreateInstance(::CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, ::IID_IFileOpenDialog,
		reinterpret_cast<void**>(&fileOpenDialog));
	if (FAILED(result))
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

bool ApplicationConfig::SelectEverquestPath()
{
	do
	{
		HRESULT hr = S_OK;
		SPDLOG_DEBUG("Asking for EverQuest Path");

		std::string result = OpenFileDialog("c:\\Program Files (x86)\\Sony\\EverQuest", "Find Everquest Directory", hr);

		if (FAILED(hr))
			break;

		fs::path eq_dir = result;
		std::string selectedPath = eq_dir.lexically_normal().string();


		if (ValidateEverQuestPath(eq_dir))
		{
			SetEverquestPath(selectedPath);
			return true;
		}

		int mbResult = ::MessageBoxA(nullptr,
			"The selected directory does not appear to be a valid EverQuest installation directory, because it does not contain eqgame.exe.\n\n"
			"Are you sure you want to use this directory?",
			"Bad Everquest Directory",
			MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON2);

		if (mbResult == IDYES)
		{
			SetEverquestPath(selectedPath);
			return true;
		}

		if (mbResult == IDCANCEL)
		{
			break;
		}
	} while (true);

	return false;
}

bool ApplicationConfig::SelectMacroQuestPath()
{
	// Create a path to the current executable
	char executablePath[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
	PathRemoveFileSpecA(executablePath);

	do
	{
		SPDLOG_DEBUG("Asking for MacroQuest Path");

		HRESULT hr = S_OK;
		std::string result = OpenFileDialog(executablePath, "Find MacroQuest Directory", hr);

		if (FAILED(hr))
			break;

		fs::path mq_dir = result;
		std::string selectedPath = mq_dir.lexically_normal().string();

		if (ValidateMacroQuestPath(mq_dir))
		{
			SetMacroQuestPath(selectedPath);
			return true;
		}

		int mbResult = ::MessageBoxA(nullptr,
			"The selected directory does not appear to be a valid MacroQuest directory, because it does not contain MacroQuest.exe.\n\n"
			"Are you sure you want to use this directory?",
			"Bad MacroQuest Directory",
			MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON2);

		if (mbResult == IDYES)
		{
			SetMacroQuestPath(selectedPath);
			return true;
		}

		if (mbResult == IDCANCEL)
		{
			break;
		}
	} while (true);

	return false;
	
}

std::string ApplicationConfig::GetLongNameForShortName(const std::string& shortName)
{
	auto iter = m_mapNames.find(shortName);
	if (iter != m_mapNames.end())
		return iter->second;

	return std::string();
}

void ApplicationConfig::Initialize()
{
	// Construct the path to the ini file
	char fullPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(nullptr, fullPath, MAX_PATH);
	PathRemoveFileSpecA(fullPath);

	// Set working directory to be the location of the executable
	SetCurrentDirectoryA(fullPath);

	m_exePath = fullPath;
	m_settingsFile = (fs::path(m_exePath) / "config" / "MeshGenerator.ini").lexically_normal().string();

	SPDLOG_INFO("Settings File: {}", m_settingsFile);

	m_everquestPath = mq::GetPrivateProfileString("General", "EverQuestPath", "", m_settingsFile);
	if (m_everquestPath.empty())
	{
		// Fall back to old settings key
		m_everquestPath = mq::GetPrivateProfileString("General", "EverQuest Path", "", m_settingsFile);

		// Rewrite the key
		if (!m_everquestPath.empty())
		{
			mq::WritePrivateProfileString("General", "EverQuestPath", m_everquestPath, m_settingsFile);
			mq::DeletePrivateProfileKey("General", "EverQuest Path", m_settingsFile);
		}
	}
	SPDLOG_DEBUG("Saved EQ Path: {}", m_everquestPath);

	if (m_everquestPath.empty())
	{
		SelectEverquestPath();
	}

	std::string mqPath = mq::GetPrivateProfileString("General", "MacroQuestPath", "", m_settingsFile);
	if (mqPath.empty())
	{
		// Fallback to old settings key
		mqPath = mq::GetPrivateProfileString("General", "Output Path", "", m_settingsFile);

		// Rewrite the key
		if (!mqPath.empty())
		{
			mq::WritePrivateProfileString("General", "MacroQuestPath", mqPath, m_settingsFile);
			mq::DeletePrivateProfileKey("General", "Output Path", m_settingsFile);
		}
	}
	SPDLOG_DEBUG("Saved MQ Path: {}", mqPath);

	if (!mqPath.empty() && ValidateMacroQuestPath(mqPath))
	{
		SetMacroQuestPath(mqPath);
	}

	// Utilize the current executable path as a default if its valid.
	if (m_mqPath.empty() && ValidateMacroQuestPath(m_exePath))
	{
		SetMacroQuestPath(m_exePath);
	}

	// No existing value or no default, prompt the user.
	if (m_mqPath.empty() || !m_mqPathValid)
	{
		SelectMacroQuestPath();
	}

	// Still failed. Use our current location
	if (m_mqPath.empty() || !m_mqPathValid)
	{
		SetMacroQuestPath(m_exePath);
	}

	if (!m_mqPathValid)
	{
		SPDLOG_WARN("Could not find a valid MacroQuest installation directory!");
	}
	
	m_useMaxExtents = mq::GetPrivateProfileBool("General", "ZoneMaxExtents", m_useMaxExtents, fullPath);

	int x = mq::GetPrivateProfileInt("Window", "PosX", 0, m_settingsFile);
	int y = mq::GetPrivateProfileInt("Window", "PosY", 0, m_settingsFile);
	int z = mq::GetPrivateProfileInt("Window", "Width", 0, m_settingsFile);
	int w = mq::GetPrivateProfileInt("Window", "Height", 0, m_settingsFile);
	m_savedDimensions = glm::ivec4(x, y, z, w);

	m_mapsLoaded = LoadZones();
}

bool ApplicationConfig::LoadZones()
{
	// Construct the path to the ini file
	std::string zonesFile = (fs::path(m_resourcesPath) / "Zones.ini").string();

	if (!fs::is_regular_file(zonesFile))
	{
		return false;
	}

	m_loadedMaps.clear();
	m_mapNames.clear();

	std::ifstream ifs(zonesFile.c_str(), std::ifstream::in);
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

	return true;
}

bool ApplicationConfig::GetSavedWindowDimensions(glm::ivec4& rect) const
{
	rect = m_savedDimensions;
	return !(rect.z <= 0 || rect.w <= 0);
}

void ApplicationConfig::SetSavedWindowDimensions(const glm::ivec4& rect)
{
	mq::WritePrivateProfileInt("Window", "PosX", rect.x, m_settingsFile);
	mq::WritePrivateProfileInt("Window", "PosY", rect.y, m_settingsFile);
	mq::WritePrivateProfileInt("Window", "Width", rect.z, m_settingsFile);
	mq::WritePrivateProfileInt("Window", "Height", rect.w, m_settingsFile);
}

void ApplicationConfig::SetUseMaxExtents(bool use)
{
	m_useMaxExtents = use;

	mq::WritePrivateProfileBool("General", "ZoneMaxExtents", m_useMaxExtents, m_settingsFile);
}
