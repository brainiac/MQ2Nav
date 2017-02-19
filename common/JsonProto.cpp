//
// JsonProto.cpp
//

#include "JsonProto.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <stdexcept>

inline std::string hex2bin(const std::string &s)
{
	if (s.size() % 2)
		throw std::runtime_error("Odd hex data size");
	static const char lookup[] = ""
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x20
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x80\x80\x80\x80\x80\x80" // 0x30
		"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x40
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x50
		"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x60
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x70
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
		"";
	std::string r;
	r.reserve(s.size() / 2);
	for (size_t i = 0; i < s.size(); i += 2) {
		char hi = lookup[static_cast<unsigned char>(s[i])];
		char lo = lookup[static_cast<unsigned char>(s[i + 1])];
		if (0x80 & (hi | lo))
			throw std::runtime_error("Invalid hex data: " + s.substr(i, 6));
		r.push_back((hi << 4) | lo);
	}
	return r;
}

inline std::string bin2hex(const std::string &s)
{
	static const char lookup[] = "0123456789abcdef";
	std::string r;
	r.reserve(s.size() * 2);
	for (size_t i = 0; i < s.size(); i++) {
		unsigned char hi = s[i] >> 4;
		unsigned char lo = s[i] & 0xf;
		r.push_back(lookup[hi]);
		r.push_back(lookup[lo]);
	}
	return r;
}

inline std::string b64_encode(const std::string &s)
{
	typedef unsigned char u1;
	static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const u1 * data = (const u1 *)s.c_str();
	std::string r;
	r.reserve(s.size() * 4 / 3 + 3);
	for (size_t i = 0; i < s.size(); i += 3) {
		unsigned n = data[i] << 16;
		if (i + 1 < s.size()) n |= data[i + 1] << 8;
		if (i + 2 < s.size()) n |= data[i + 2];

		u1 n0 = (u1)(n >> 18) & 0x3f;
		u1 n1 = (u1)(n >> 12) & 0x3f;
		u1 n2 = (u1)(n >> 6) & 0x3f;
		u1 n3 = (u1)(n) & 0x3f;

		r.push_back(lookup[n0]);
		r.push_back(lookup[n1]);
		if (i + 1 < s.size()) r.push_back(lookup[n2]);
		if (i + 2 < s.size()) r.push_back(lookup[n3]);
	}
	for (size_t i = 0; i < (3 - s.size() % 3) % 3; i++)
		r.push_back('=');
	return r;
}

inline std::string b64_decode(const std::string &s)
{
	typedef unsigned char u1;
	static const char lookup[] = ""
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x3e\x80\x80\x80\x3f" // 0x20
		"\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x80\x80\x80\x00\x80\x80" // 0x30
		"\x80\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // 0x40
		"\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x80\x80\x80\x80\x80" // 0x50
		"\x80\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // 0x60
		"\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x80\x80\x80\x80\x80" // 0x70
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
		"";
	std::string r;
	if (!s.size()) return r;
	if (s.size() % 4)
		throw std::runtime_error("Invalid base64 data size");
	size_t pad = 0;
	if (s[s.size() - 1] == '=') pad++;
	if (s[s.size() - 2] == '=') pad++;

	r.reserve(s.size() * 3 / 4 + 3);
	for (size_t i = 0; i < s.size(); i += 4) {
		u1 n0 = lookup[(u1)s[i + 0]];
		u1 n1 = lookup[(u1)s[i + 1]];
		u1 n2 = lookup[(u1)s[i + 2]];
		u1 n3 = lookup[(u1)s[i + 3]];
		if (0x80 & (n0 | n1 | n2 | n3))
			throw std::runtime_error("Invalid hex data: " + s.substr(i, 4));
		unsigned n = (n0 << 18) | (n1 << 12) | (n2 << 6) | n3;
		r.push_back((n >> 16) & 0xff);
		if (s[i + 2] != '=') r.push_back((n >> 8) & 0xff);
		if (s[i + 3] != '=') r.push_back((n) & 0xff);
	}
	return r;
}

namespace
{
	enum struct ParseResult
	{
		Success = 0,
		InvalidArg = 1,
		InvalidProto = 2,
		UnknownField = 3,
		InvalidJson = 4
	};
}

static rapidjson::Value ParseMessage(const google::protobuf::Message* msg,
	rapidjson::Value::AllocatorType& allocator);

static rapidjson::Value FieldToJson(const google::protobuf::Message* msg,
	const google::protobuf::FieldDescriptor* field,
	rapidjson::Value::AllocatorType& allocator)
{
	const google::protobuf::Reflection* reflection = msg->GetReflection();

	if (field->is_repeated())
	{
		rapidjson::Value json{ rapidjson::kArrayType };

		for (int index = 0; index < reflection->FieldSize(*msg, field); ++index)
		{
			switch (field->cpp_type())
			{
			case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedDouble(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedFloat(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedInt64(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedUInt64(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedInt32(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedUInt32(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
				json.PushBack(rapidjson::Value{ reflection->GetRepeatedBool(*msg, field, index) }, allocator);
				break;
			case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
			{
				std::string value = reflection->GetRepeatedString(*msg, field, index);
				if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
				{
					value = b64_encode(value);
				}

				json.PushBack(rapidjson::Value{ value.c_str(), static_cast<rapidjson::SizeType>(value.size()), allocator }, allocator);
				break;
			}
			case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			{
				json.PushBack(ParseMessage(&reflection->GetRepeatedMessage(*msg, field, index), allocator), allocator);
				break;
			}
			case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
			{
				const google::protobuf::EnumValueDescriptor* value = reflection->GetRepeatedEnum(*msg, field, index);
				json.PushBack(rapidjson::Value{ value->number() }, allocator);
				break;
			}
			default:
				break;
			}
		}

		return std::move(json);
	}
	else
	{
		switch (field->cpp_type())
		{
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
			return rapidjson::Value(reflection->GetDouble(*msg, field));
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
			return rapidjson::Value(reflection->GetFloat(*msg, field));
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
			return rapidjson::Value(static_cast<int64_t>(reflection->GetInt64(*msg, field)));
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
			return rapidjson::Value(static_cast<uint64_t>(reflection->GetUInt64(*msg, field)));
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
			return rapidjson::Value(static_cast<int32_t>(reflection->GetInt32(*msg, field)));
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
			return rapidjson::Value(static_cast<uint32_t>(reflection->GetUInt32(*msg, field)));
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
			return rapidjson::Value(reflection->GetBool(*msg, field));
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			std::string value = reflection->GetString(*msg, field);
			if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
			{
				value = b64_encode(value);
			}

			return rapidjson::Value(value.c_str(), static_cast<rapidjson::SizeType>(value.size()), allocator);
		}
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			return ParseMessage(&reflection->GetMessage(*msg, field), allocator);
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
			return rapidjson::Value(reflection->GetEnum(*msg, field)->number());
		default:
			break;
		}
	}

	return rapidjson::Value{};
}

static rapidjson::Value ParseMessage(const google::protobuf::Message* msg,
	rapidjson::Value::AllocatorType& allocator)
{
	const google::protobuf::Descriptor* descriptor = msg->GetDescriptor();
	const google::protobuf::Reflection* reflection = msg->GetReflection();

	rapidjson::Value root(rapidjson::kObjectType);

	for (int index = 0; index < descriptor->field_count(); ++index)
	{
		const google::protobuf::FieldDescriptor* field = descriptor->field(index);

		if (!field->is_optional() || reflection->HasField(*msg, field))
		{
			rapidjson::Value jsonField = FieldToJson(msg, field, allocator);
			root.AddMember(rapidjson::Value{ field->name().c_str(),
				static_cast<rapidjson::SizeType>(field->name().size()) },
				jsonField, allocator);
		}
	}

	return std::move(root);
}

static ParseResult ParseJson(const rapidjson::Value* json, google::protobuf::Message* msg,
	std::string& errorMessage);

static ParseResult JsonToField(const rapidjson::Value* json, google::protobuf::Message* msg,
	const google::protobuf::FieldDescriptor* field, std::string& errorMessage)
{
	const google::protobuf::Reflection* reflection = msg->GetReflection();
	bool repeated = field->is_repeated();

	switch (field->cpp_type())
	{
	case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddInt32(msg, field, static_cast<int32_t>(json->GetInt()));
		else
			reflection->SetInt32(msg, field, static_cast<int32_t>(json->GetInt()));
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddUInt32(msg, field, static_cast<uint32_t>(json->GetUint()));
		else
			reflection->SetUInt32(msg, field, static_cast<uint32_t>(json->GetUint()));
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddInt64(msg, field, static_cast<int64_t>(json->GetInt64()));
		else
			reflection->SetInt64(msg, field, static_cast<int64_t>(json->GetInt64()));
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddUInt64(msg, field, static_cast<uint64_t>(json->GetUint64()));
		else
			reflection->SetUInt64(msg, field, static_cast<uint64_t>(json->GetUint64()));
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddDouble(msg, field, json->GetDouble());
		else
			reflection->SetDouble(msg, field, json->GetDouble());
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
	{
		if (json->GetType() != rapidjson::kNumberType)
		{
			errorMessage = "Not a number";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddFloat(msg, field, static_cast<float>(json->GetDouble()));
		else
			reflection->SetFloat(msg, field, static_cast<float>(json->GetDouble()));
		break;
	}

	case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
	{
		if (json->GetType() != rapidjson::kTrueType
			&& json->GetType() != rapidjson::kFalseType)
		{
			errorMessage = "Not a bool";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddBool(msg, field, json->GetBool());
		else
			reflection->SetBool(msg, field, json->GetBool());
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
	{
		if (json->GetType() != rapidjson::kStringType)
		{
			errorMessage = "Not a string";
			return ParseResult::InvalidJson;
		}

		std::string str_value{ json->GetString(), json->GetStringLength() };
		if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
			str_value = b64_decode(str_value);

		if (repeated)
			reflection->AddString(msg, field, str_value);
		else
			reflection->SetString(msg, field, str_value);
		break;
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
	{
		google::protobuf::Message* message = [&]() {
			if (repeated)
				return reflection->AddMessage(msg, field);
			return reflection->MutableMessage(msg, field);
		}();
		return ParseJson(json, message, errorMessage);
	}
	case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
	{
		const google::protobuf::EnumDescriptor* ed = field->enum_type();
		const google::protobuf::EnumValueDescriptor* ev = nullptr;

		if (json->GetType() == rapidjson::kNumberType)
		{
			ev = ed->FindValueByNumber(json->GetInt());
		}
		else if (json->GetType() == rapidjson::kStringType)
		{
			ev = ed->FindValueByName(json->GetString());
		}
		else
		{
			errorMessage = "Not an integer or string";
			return ParseResult::InvalidJson;
		}

		if (!ev)
		{
			errorMessage = "Enum value not found";
			return ParseResult::InvalidJson;
		}

		if (repeated)
			reflection->AddEnum(msg, field, ev);
		else
			reflection->SetEnum(msg, field, ev);
		break;
	}
	default:
		return ParseResult::InvalidProto;
	}

	return ParseResult::Success;
}

static ParseResult ParseJson(const rapidjson::Value* json, google::protobuf::Message* msg,
	std::string& errorMessage)
{
	if (!json || json->GetType() != rapidjson::kObjectType)
		return ParseResult::InvalidArg;

	const google::protobuf::Descriptor* descriptor = msg->GetDescriptor();
	const google::protobuf::Reflection* reflection = msg->GetReflection();

	for (auto iter = json->MemberBegin(); iter != json->MemberEnd(); ++iter)
	{
		const char* name = iter->name.GetString();
		const rapidjson::Value& value = iter->value;

		const google::protobuf::FieldDescriptor* field = descriptor->FindFieldByName(name);

		if (!field)
		{
			field = reflection->FindKnownExtensionByName(name);
		}

		if (!field)
		{
			// TODO: Write to unknown field
			continue;
		}
		else if (value.GetType() == rapidjson::kNullType)
		{
			reflection->ClearField(msg, field);
		}
		else if (field->is_repeated())
		{
			if (value.GetType() != rapidjson::kArrayType)
			{
				errorMessage = "Not an array";
				return ParseResult::InvalidJson;
			}

			for (auto arrIt = value.Begin(); arrIt != value.End(); ++arrIt)
			{
				ParseResult result = JsonToField(arrIt, msg, field, errorMessage);
				if (result != ParseResult::Success)
				{
					return result;
				}
			}
		}
		else
		{
			ParseResult result = JsonToField(&value, msg, field, errorMessage);
			if (result != ParseResult::Success)
			{
				return result;
			}
		}
	}

	return ParseResult::Success;
}

void JsonToString(const rapidjson::Value& json, std::string& str, bool pretty)
{
	rapidjson::StringBuffer buffer;

	if (pretty)
	{
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		writer.SetIndent(' ', 2);
		json.Accept(writer);
	}
	else
	{
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		json.Accept(writer);
	}

	str = std::string{ buffer.GetString(), buffer.GetSize() };
}

void ProtoToJsonString(const google::protobuf::Message* msg,
	std::string& str, bool pretty /* = false */)
{
	rapidjson::Value::AllocatorType allocator;

	rapidjson::Value value = ParseMessage(msg, allocator);
	JsonToString(value, str, pretty);
}

bool JsonToProto(const rapidjson::Value* json, google::protobuf::Message* msg)
{
	std::string errorMessage;

	return ParseJson(json, msg, errorMessage) == ParseResult::Success;
}

