//
// MeshgenHandler.cpp
//

#include "pch.h"
#include "MeshgenHandler.h"

#include "common/proto/NavMeshClient.pb.h"

#include <fmt/format.h>
#include <google/protobuf/util/message_differencer.h>

#include <chrono>

void MeshgenHandler::Initialize()
{
	if (pLocalPC != nullptr)
	{
		m_characterInfo = fmt::format("{}_{}", GetServerShortName(), pLocalPC->Name);
		to_lower(m_characterInfo);
	}
}

void MeshgenHandler::Shutdown()
{
	postoffice::Address address;
	address.Name = "meshgen";

	nav::client::DropClient drop;
	drop.set_character_info(m_characterInfo);

	nav::client::ClientMessage message;
	*message.mutable_drop() = drop;

	postoffice::SendToActor(address, message);
}

void MeshgenHandler::OnPulse()
{
	using clock = std::chrono::system_clock;
	using namespace std::chrono_literals;

	static clock::time_point last_update = clock::now();
	static constexpr clock::duration update_interval = 1s;

	if (!m_characterInfo.empty() && pZoneInfo != nullptr && pLocalPC != nullptr)
	{
		auto now = clock::now();
		if (now >= last_update + update_interval)
		{
			nav::client::UpdateTransform transform;
			transform.set_character_info(m_characterInfo);
			transform.set_shortzone(pZoneInfo->ShortName);

			nav::vector3& position = *transform.mutable_position();
			position.set_x(pLocalPlayer->X);
			position.set_y(pLocalPlayer->Y);
			position.set_z(pLocalPlayer->Z);

			transform.set_heading(pLocalPlayer->Heading);

			postoffice::Address address;
			address.Name = "meshgen";

			nav::client::ClientMessage message;
			*message.mutable_transform() = transform;

			postoffice::SendToActor(address, message);

			last_update = now;
		}
	}
}

void MeshgenHandler::SetGameState(int gameState)
{
	if (pLocalPC != nullptr)
	{
		m_characterInfo = fmt::format("{}_{}", GetServerShortName(), pLocalPC->Name);
		to_lower(m_characterInfo);
	}
	else
	{
		m_characterInfo.clear();
	}
}
