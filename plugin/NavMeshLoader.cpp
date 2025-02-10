//
// NavMeshLoader.cpp
//

#include "pch.h"
#include "NavMeshLoader.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/Utilities.h"
#include "mq/base/WString.h"

#include <DetourNavMesh.h>
#include <DetourCommon.h>
#include <spdlog/spdlog.h>
#include <wil/filesystem.h>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

//============================================================================

struct NavMeshLoader::FileWatcher
{
	FileWatcher(const std::string& navMeshDir, std::function<void(const std::string&)>&& callback)
		: m_callback(std::move(callback))
	{
		m_watcher = wil::make_folder_change_reader_nothrow(
			utf8_to_wstring(navMeshDir).c_str(), false, wil::FolderChangeEvents::All,
			[this](wil::FolderChangeEvent event, PCWSTR fileName)
			{
				if (ci_ends_with(fileName, L".navmesh"))
				{
					if (event == wil::FolderChangeEvent::Modified || event == wil::FolderChangeEvent::Added)
						m_callback(wstring_to_utf8(fileName));
				}
			});
	}

	std::function<void(const std::string&)> m_callback;
	wil::unique_folder_change_reader_nothrow m_watcher;
};

//============================================================================


NavMeshLoader::NavMeshLoader(NavMesh* mesh)
	: m_navMesh(mesh)
{
	UpdateAutoReload();
}

//----------------------------------------------------------------------------

void NavMeshLoader::UpdateAutoReload()
{
	if (m_autoReload)
	{
		if (!m_fileWatcher)
		{
			m_fileWatcher = std::make_unique<FileWatcher>(m_navMesh->GetNavMeshDirectory(),
				[this](const std::string& fileName)
				{
					if (ci_equals(fileName, m_navMesh->GetFileName()))
					{
						m_changed = true;
						m_lastUpdate = clock::now();
					}
				});
		}
	}
	else
	{
		m_fileWatcher.reset();
	}
}

void NavMeshLoader::SetZoneId(int zoneId)
{
	if (m_zoneId == zoneId)
		return;

	m_zoneId = zoneId;

	if (m_zoneId != -1)
	{
		// set or clear zone name
		const char* zoneName = GetShortZone(zoneId);
		m_zoneShortName = zoneName ? zoneName : std::string();

		if (m_zoneShortName == "UNKNOWN_ZONE")
		{
			// invalid / unsupported zone id
			SPDLOG_WARN("Unrecognized zone id: {}", zoneId);
			m_navMesh->ResetNavMesh();
		}
		else
		{
			SPDLOG_DEBUG("Zone changed to: {}", m_zoneShortName);
			m_navMesh->SetZoneName(m_zoneShortName);

			if (m_autoLoad)
			{
				m_navMesh->LoadNavMeshFile();
			}
		}
	}
	else
	{
		m_navMesh->ResetNavMesh();
	}
}

void NavMeshLoader::SetAutoLoad(bool autoLoad)
{
	m_autoLoad = autoLoad;
}

bool NavMeshLoader::LoadNavMesh()
{
	NavMesh::LoadResult result = m_navMesh->LoadNavMeshFile();
	std::string meshFile = m_navMesh->GetFullFilePath();
	bool success = false;

	switch (result)
	{
	default:
	case NavMesh::LoadResult::None:
		SPDLOG_ERROR("An error occurred trying to load the mesh");
		break;

	case NavMesh::LoadResult::Success:
		SPDLOG_INFO("\agSuccessfully loaded mesh for \am{}\ax", m_zoneShortName);
		success = true;
		break;

	case NavMesh::LoadResult::MissingFile:
		SPDLOG_WARN("No nav mesh available for \am{}\ax", m_zoneShortName);
		break;

	case NavMesh::LoadResult::Corrupt:
		SPDLOG_ERROR("Failed to load mesh file, the file is corrupt ({})", meshFile);
		break;

	case NavMesh::LoadResult::VersionMismatch:
		SPDLOG_ERROR("Couldn't load mesh file due to version mismatch ({})", meshFile);
		break;

	case NavMesh::LoadResult::ZoneMismatch:
		SPDLOG_ERROR("Couldn't load mesh file. It isn't for this zone.");
		break;

	case NavMesh::LoadResult::OutOfMemory:
		SPDLOG_ERROR("Couldn't load mesh file. Ran out of memory!");
		break;
	}

	return success;
}

void NavMeshLoader::OnPulse()
{
	if (m_autoReload)
	{
		clock::time_point now = clock::now();
		if (m_changed && now - m_lastUpdate > std::chrono::seconds(1))
		{
			m_changed = false;

			if (m_navMesh->IsNavMeshLoadedFromDisk())
			{
				SPDLOG_DEBUG("Change detected on navmesh, reloading");
				LoadNavMesh();
			}
		}
	}
}

void NavMeshLoader::SetGameState(int GameState)
{
	if (GameState != GAMESTATE_INGAME) {
		// don't unload the mesh until we finish zoning. We might zone
		// into the same zone (succor), so no use unload the mesh until
		// after loading completes.
		if (GameState != GAMESTATE_ZONING && GameState != GAMESTATE_LOGGINGIN) {
			m_navMesh->ResetNavMesh();
		}
	}
}

//----------------------------------------------------------------------------

void NavMeshLoader::SetAutoReload(bool autoReload)
{
	if (m_autoReload != autoReload)
	{
		m_autoReload = autoReload;
		UpdateAutoReload();
	}
}
