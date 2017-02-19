//
// JsonProto.h
//

#pragma once

#include <rapidjson/document.h>

#include <memory>
#include <string>

namespace google {
	namespace protobuf {
		class Message;
	}
}

// Convert protobuf to json object
void ProtoToJsonString(const google::protobuf::Message* msg, std::string& str,
	bool pretty = false);

// Convert json object to protobuf
bool JsonToProto(const rapidjson::Value* json, google::protobuf::Message* msg);
