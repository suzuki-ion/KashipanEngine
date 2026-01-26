#include "JsonInput.h"
#include <sstream>
#include <cctype>

void JsonInput::SkipWhitespace(std::istream& in) {
	while (in && std::isspace(in.peek())) {
		in.get();
	}
}

void JsonInput::SkipUntil(std::istream& in, char target) {
	char c;
	while (in.get(c)) {
		if (c == target) {
			break;
		}
	}
}

bool JsonInput::ExpectChar(std::istream& in, char expected) {
	SkipWhitespace(in);
	char c;
	if (in.get(c) && c == expected) {
		return true;
	}
	return false;
}

std::string JsonInput::ReadString(std::istream& in) {
	SkipWhitespace(in);
	if (!ExpectChar(in, '\"')) {
		return "";
	}

	std::string result;
	char c;
	while (in.get(c)) {
		if (c == '\"') {
			break;
		}
		if (c == '\\') {
			if (in.get(c)) {
				switch (c) {
				case '\"': result += '\"'; break;
				case '\\': result += '\\'; break;
				case '/': result += '/'; break;
				case 'b': result += '\b'; break;
				case 'f': result += '\f'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				default: result += c; break;
				}
			}
		} else {
			result += c;
		}
	}
	return result;
}

std::shared_ptr<ValueBase> JsonInput::ParseValue(std::istream& in) {
	// { で始まるオブジェクトを読む
	if (!ExpectChar(in, '{')) {
		return nullptr;
	}

	std::string name;
	int typeID = 0;

	// name, type, value を読む
	while (true) {
		SkipWhitespace(in);
		std::string key = ReadString(in);

		SkipWhitespace(in);
		if (!ExpectChar(in, ':')) {
			break;
		}

		if (key == "name") {
			name = ReadString(in);
		} else if (key == "type") {
			SkipWhitespace(in);
			in >> typeID;
		} else if (key == "value") {
			SkipWhitespace(in);

			// 型に応じて値を読み込む
			switch (static_cast<TypeID>(typeID)) {
			case TypeID::Int:
			{
				int value;
				in >> value;
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<int>>(value, name);
			}
			case TypeID::Float:
			{
				float value;
				in >> value;
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<float>>(value, name);
			}
			case TypeID::Bool:
			{
				std::string boolStr;
				in >> boolStr;
				bool value = (boolStr == "true");
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<bool>>(value, name);
			}
			case TypeID::String:
			{
				std::string value = ReadString(in);
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<std::string>>(value, name);
			}
			case TypeID::Double:
			{
				double value;
				in >> value;
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<double>>(value, name);
			}
			case TypeID::uint8_t:
			{
				int temp;
				in >> temp;
				uint8_t value = static_cast<uint8_t>(temp);
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<uint8_t>>(value, name);
			}
			case TypeID::uint32_t:
			{
				uint32_t value;
				in >> value;
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<uint32_t>>(value, name);
			}
			case TypeID::Vector2:
			{
				Vector2 value;
				ExpectChar(in, '[');
				in >> value.x;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.y;
				SkipWhitespace(in);
				ExpectChar(in, ']');
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<Vector2>>(value, name);
			}
			case TypeID::Vector3:
			{
				Vector3 value;
				ExpectChar(in, '[');
				in >> value.x;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.y;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.z;
				SkipWhitespace(in);
				ExpectChar(in, ']');
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<Vector3>>(value, name);
			}
			case TypeID::Vector4:
			{
				Vector4 value;
				ExpectChar(in, '[');
				in >> value.x;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.y;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.z;
				SkipWhitespace(in);
				ExpectChar(in, ',');
				in >> value.w;
				SkipWhitespace(in);
				ExpectChar(in, ']');
				SkipWhitespace(in);
				ExpectChar(in, '}');
				return std::make_shared<Value<Vector4>>(value, name);
			}
			default:
				return nullptr;
			}
		}

		SkipWhitespace(in);
		char c = static_cast<char>(in.peek());
		if (c == ',') {
			in.get();
		} else if (c == '}') {
			break;
		}
	}

	return nullptr;
}

std::vector<std::shared_ptr<ValueBase>> JsonInput::ReadJson(std::istream& in) {
	std::vector<std::shared_ptr<ValueBase>> result;

	// { で始まる
	if (!ExpectChar(in, '{')) {
		return result;
	}

	// "values" キーを探す
	SkipWhitespace(in);
	std::string key = ReadString(in);

	if (key != "values") {
		return result;
	}

	SkipWhitespace(in);
	if (!ExpectChar(in, ':')) {
		return result;
	}

	// [ で配列開始
	if (!ExpectChar(in, '[')) {
		return result;
	}

	// 配列内の各要素を読む
	while (true) {
		SkipWhitespace(in);
		char c = static_cast<char>(in.peek());

		if (c == ']') {
			in.get();
			break;
		}

		auto value = ParseValue(in);
		if (value) {
			result.push_back(value);
		}

		SkipWhitespace(in);
		c = static_cast<char>(in.peek());
		if (c == ',') {
			in.get();
		} else if (c == ']') {
			in.get();
			break;
		}
	}

	return result;
}
