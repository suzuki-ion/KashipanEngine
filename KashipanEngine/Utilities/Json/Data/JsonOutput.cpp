#include "JsonOutput.h"
#include <iomanip>
#include <sstream>

std::string JsonOutput::EscapeString(const std::string& str) {
	std::ostringstream oss;
	for (char c : str) {
		switch (c) {
		case '\"': oss << "\\\""; break;
		case '\\': oss << "\\\\"; break;
		case '\b': oss << "\\b"; break;
		case '\f': oss << "\\f"; break;
		case '\n': oss << "\\n"; break;
		case '\r': oss << "\\r"; break;
		case '\t': oss << "\\t"; break;
		default:
			if ('\x00' <= c && c <= '\x1f') {
				oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
			} else {
				oss << c;
			}
		}
	}
	return oss.str();
}

void JsonOutput::WriteValue(std::ostream& out, const ValueBase* value, bool isLast) {
	TypeID typeID = static_cast<TypeID>(value->GetTypeID());

	out << "  {\n";
	out << "    \"name\": \"" << EscapeString(value->name) << "\",\n";
	out << "    \"type\": " << static_cast<int>(typeID) << ",\n";
	out << "    \"value\": ";

	switch (typeID) {
	case TypeID::Int:
	{
		auto* val = static_cast<const Value<int>*>(value);
		out << val->value;
		break;
	}
	case TypeID::Float:
	{
		auto* val = static_cast<const Value<float>*>(value);
		out << std::fixed << std::setprecision(6) << val->value;
		break;
	}
	case TypeID::Bool:
	{
		auto* val = static_cast<const Value<bool>*>(value);
		out << (val->value ? "true" : "false");
		break;
	}
	case TypeID::String:
	{
		auto* val = static_cast<const Value<std::string>*>(value);
		out << "\"" << EscapeString(val->value) << "\"";
		break;
	}
	case TypeID::Double:
	{
		auto* val = static_cast<const Value<double>*>(value);
		out << std::fixed << std::setprecision(15) << val->value;
		break;
	}
	case TypeID::uint8_t:
	{
		auto* val = static_cast<const Value<uint8_t>*>(value);
		out << static_cast<int>(val->value);
		break;
	}
	case TypeID::uint32_t:
	{
		auto* val = static_cast<const Value<uint32_t>*>(value);
		out << val->value;
		break;
	}
	case TypeID::Vector2:
	{
		auto* val = static_cast<const Value<Vector2>*>(value);
		out << "[" << val->value.x << ", " << val->value.y << "]";
		break;
	}
	case TypeID::Vector3:
	{
		auto* val = static_cast<const Value<Vector3>*>(value);
		out << "[" << val->value.x << ", " << val->value.y << ", " << val->value.z << "]";
		break;
	}
	case TypeID::Vector4:
	{
		auto* val = static_cast<const Value<Vector4>*>(value);
		out << "[" << val->value.x << ", " << val->value.y << ", " << val->value.z << ", " << val->value.w << "]";
		break;
	}
	default:
		out << "null";
		break;
	}

	out << "\n  }";
	if (!isLast) {
		out << ",";
	}
	out << "\n";
}

void JsonOutput::WriteJson(std::ostream& out, const std::vector<std::shared_ptr<ValueBase>>& values) {
	out << "{\n";
	out << "  \"values\": [\n";

	for (size_t i = 0; i < values.size(); ++i) {
		WriteValue(out, values[i].get(), i == values.size() - 1);
	}

	out << "  ]\n";
	out << "}\n";
}
