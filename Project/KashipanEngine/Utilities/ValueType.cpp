#include "ValueType.h"

namespace {

std::string Trim(std::string s) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::vector<std::string> SplitTemplateArguments(const std::string &argsStr) {
    std::vector<std::string> result;
    std::string current;
    int depth = 0;

    for (char ch : argsStr) {
        if (ch == '<') {
            ++depth;
            current.push_back(ch);
        } else if (ch == '>') {
            --depth;
            current.push_back(ch);
        } else if (ch == ',' && depth == 0) {
            result.push_back(Trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    if (!current.empty()) {
        result.push_back(Trim(current));
    }

    return result;
}

} // namespace

std::string ValueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::None:         return "None";
        case ValueType::Bool:         return "Bool";
        case ValueType::Int8:         return "Int8";
        case ValueType::UInt8:        return "UInt8";
        case ValueType::Int16:        return "Int16";
        case ValueType::UInt16:       return "UInt16";
        case ValueType::Int32:        return "Int32";
        case ValueType::UInt32:       return "UInt32";
        case ValueType::Int64:        return "Int64";
        case ValueType::UInt64:       return "UInt64";
        case ValueType::Float:        return "Float";
        case ValueType::Double:       return "Double";
        case ValueType::String:       return "String";
        case ValueType::Pointer:      return "Pointer";
        case ValueType::Class:        return "Class";
        case ValueType::Any:          return "Any";
        case ValueType::Vector2:      return "Vector2";
        case ValueType::Vector3:      return "Vector3";
        case ValueType::Vector4:      return "Vector4";
        case ValueType::Matrix3x3:    return "Matrix3x3";
        case ValueType::Matrix4x4:    return "Matrix4x4";
        case ValueType::Quaternion:   return "Quaternion";
        case ValueType::Color:        return "Color";
        case ValueType::TextureRef:   return "TextureRef";
        case ValueType::Vector:       return "Vector";
        case ValueType::UnorderedMap: return "UnorderedMap";
        case ValueType::Pair:         return "Pair";
        default:                      return "Unknown";
    }
}

ValueType StringToValueType(const std::string &str) {
    if (str == "Bool") return ValueType::Bool;
    if (str == "Int8") return ValueType::Int8;
    if (str == "UInt8") return ValueType::UInt8;
    if (str == "Int16") return ValueType::Int16;
    if (str == "UInt16") return ValueType::UInt16;
    if (str == "Int32") return ValueType::Int32;
    if (str == "UInt32") return ValueType::UInt32;
    if (str == "Int64") return ValueType::Int64;
    if (str == "UInt64") return ValueType::UInt64;
    if (str == "Float") return ValueType::Float;
    if (str == "Double") return ValueType::Double;
    if (str == "String") return ValueType::String;
    if (str == "Pointer") return ValueType::Pointer;
    if (str == "Class") return ValueType::Class;
    if (str == "Any") return ValueType::Any;
    if (str == "Vector2") return ValueType::Vector2;
    if (str == "Vector3") return ValueType::Vector3;
    if (str == "Vector4") return ValueType::Vector4;
    if (str == "Matrix3x3") return ValueType::Matrix3x3;
    if (str == "Matrix4x4") return ValueType::Matrix4x4;
    if (str == "Quaternion") return ValueType::Quaternion;
    if (str == "Color") return ValueType::Color;
    if (str == "TextureRef") return ValueType::TextureRef;
    if (str.find("Vector") != std::string::npos) return ValueType::Vector;
    if (str.find("UnorderedMap") != std::string::npos) return ValueType::UnorderedMap;
    if (str.find("Pair") != std::string::npos) return ValueType::Pair;
    return ValueType::None; // 未定義
}

std::string TypeInfo::ToString() const {
    std::string result = ValueTypeToString(baseType_);

    // テンプレート引数（内部型）が存在する場合、< > で囲んで再帰的に文字列化
    if (!templateArguments_.empty()) {
        result += "<";
        for (size_t i = 0; i < templateArguments_.size(); ++i) {
            result += templateArguments_[i].ToString();
            if (i < templateArguments_.size() - 1) {
                result += ", ";
            }
        }
        result += ">";
    }
    return result;
}

ValueType TypeInfo::GetBaseType() const { return baseType_; }

const std::vector<TypeInfo> &TypeInfo::GetTemplateArguments() const { return templateArguments_; }

TypeInfo GetValueType(const std::string &typeStr) {
    std::string s = Trim(typeStr);

    size_t pos = s.find('<');
    if (pos == std::string::npos) {
        return TypeInfo(StringToValueType(s));
    }

    std::string baseTypeStr = Trim(s.substr(0, pos));
    size_t endPos = s.rfind('>');
    if (endPos == std::string::npos || endPos <= pos) {
        return TypeInfo(StringToValueType(baseTypeStr));
    }

    std::string argsStr = s.substr(pos + 1, endPos - pos - 1);
    std::vector<TypeInfo> templateArgs;

    for (const auto &arg : SplitTemplateArguments(argsStr)) {
        templateArgs.push_back(GetValueType(arg));
    }

    return TypeInfo(StringToValueType(baseTypeStr), templateArgs);
}