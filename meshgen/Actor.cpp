//
// Actor.cpp
//

#include "Application.h"

#include "routing/Routing.h"
#include "common/proto/NavMeshClient.pb.h"

using namespace mq;

mq::ProtoPipeClient* s_pipeClient = nullptr; // { mq::MQ2_PIPE_SERVER_PATH };

class PipeEventsHandler : public NamedPipeEvents
{
public:
	PipeEventsHandler() {}

	virtual void OnIncomingMessage(PipeMessagePtr&& message) override
	{
		switch (message->GetMessageId())
		{
		case MQMessageId::MSG_ROUTE:
		{
			auto envelope = ProtoMessage::Parse<proto::routing::Envelope>(message);
			if (envelope.has_payload())
			{
				nav::client::ClientMessage client_message;
				client_message.ParseFromString(envelope.payload());

				switch (client_message.message_case())
				{
				case nav::client::ClientMessage::kTransform:
					ClientInformation::UpdateTransform(client_message.transform());
					break;
				case nav::client::ClientMessage::kDrop:
					ClientInformation::DropCharacter(client_message.drop().character_info());
					break;
				default:
					break;
				}
			}
			break;
		}

		case MQMessageId::MSG_IDENTIFICATION:
		{
			// the launcher wants identification
			proto::routing::Identification id;
			id.set_name("meshgen");
			id.set_pid(GetCurrentProcessId());
			s_pipeClient->SendProtoMessage(MQMessageId::MSG_IDENTIFICATION, id);
			break;
		}

		default: break;
		}
	}

	virtual void OnClientConnected() override
	{
		// Send an ID message to the launcher
		proto::routing::Identification id;
		id.set_name("meshgen");
		id.set_pid(GetCurrentProcessId());
		s_pipeClient->SendProtoMessage(MQMessageId::MSG_IDENTIFICATION, id);
	}
};

void StopPipeClient()
{
	s_pipeClient->Stop();

	delete s_pipeClient;
	s_pipeClient = nullptr;
}

void StartPipeClient()
{
	s_pipeClient = new mq::ProtoPipeClient(mq::MQ2_PIPE_SERVER_PATH);
	s_pipeClient->SetHandler(std::make_shared<PipeEventsHandler>());
	s_pipeClient->Start();
	::atexit(StopPipeClient);
}

void ProcessPipeClient()
{
	if (s_pipeClient != nullptr)
		s_pipeClient->Process();
}
