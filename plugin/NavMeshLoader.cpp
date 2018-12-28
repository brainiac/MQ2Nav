//
// NavMeshLoader.cpp
//

#include "NavMeshLoader.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/Utilities.h"

#include <DetourNavMesh.h>
#include <DetourCommon.h>
#include <ctime>

//============================================================================

NavMeshLoader::NavMeshLoader(Context* context, NavMesh* mesh)
	: m_context(context)
	, m_navMesh(mesh)
{
}

//----------------------------------------------------------------------------

void NavMeshLoader::SetZoneId(int zoneId)
{
	if (m_zoneId == zoneId)
		return;

	m_zoneId = zoneId;

	if (m_zoneId != -1)
	{
		// set or clear zone name
		PCHAR zoneName = GetShortZone(zoneId);
		m_zoneShortName = zoneName ? zoneName : std::string();

		if (m_zoneShortName == "UNKNOWN_ZONE")
		{
			// invalid / unsupported zone id
			m_context->Log(LogLevel::WARNING, "Unrecognized zone id: %d", zoneId);
			m_navMesh->ResetNavMesh();
		}
		else
		{
			m_context->Log(LogLevel::INFO, "Zone changed to: %s", m_zoneShortName.c_str());
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
	std::string meshFile = m_navMesh->GetDataFileName();
	bool success = false;

	switch (result)
	{
	default:
	case NavMesh::LoadResult::None:
		WriteChatf(PLUGIN_MSG "\arAn error occurred trying to load the mesh");
		break;

	case NavMesh::LoadResult::Success:
		WriteChatf(PLUGIN_MSG "\agSuccessfully loaded mesh for \am%s\ax", m_zoneShortName.c_str());
		{
			// Get filetime
			HANDLE hFile = CreateFile(meshFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, 0, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				GetFileTime(hFile, NULL, NULL, &m_fileTime);
			}
			CloseHandle(hFile);
		}

		success = true;
		break;

	case NavMesh::LoadResult::MissingFile:
		WriteChatf(PLUGIN_MSG "\ayNo nav mesh available for \am%s\ax", m_zoneShortName.c_str());
		break;

	case NavMesh::LoadResult::Corrupt:
		WriteChatf(PLUGIN_MSG "\arFailed to load mesh file, the file is corrupt (%s)", 
			meshFile.c_str());
		break;

	case NavMesh::LoadResult::VersionMismatch:
		WriteChatf(PLUGIN_MSG "\arCouldn't load mesh file due to version mismatch (%s)",
			meshFile.c_str());
		break;

	case NavMesh::LoadResult::ZoneMismatch:
		WriteChatf(PLUGIN_MSG "\arCouldn't load mesh file. It isn't for this zone.");
		break;
	}

	return success;
}

void NavMeshLoader::OnPulse()
{
	if (m_autoReload)
	{
		clock::time_point now = clock::now();
		if (now - m_lastUpdate > std::chrono::seconds(1))
		{
			m_lastUpdate = now;

			if (m_navMesh->IsNavMeshLoadedFromDisk())
			{
				std::string filename = m_navMesh->GetDataFileName();

				// Get the current filetime
				FILETIME currentFileTime;

				HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
					NULL, OPEN_EXISTING, 0, NULL);
				if (hFile != INVALID_HANDLE_VALUE)
				{
					GetFileTime(hFile, NULL, NULL, &currentFileTime);

					// Reload file *if* it is 5 seconds old AND newer than existing
					if (CompareFileTime(&currentFileTime, &m_fileTime))
					{
						m_context->Log(LogLevel::DEBUG,
							"Current file time is newer than old file time, refreshing");
						LoadNavMesh();
					}
				}

				CloseHandle(hFile);
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
	}
}
