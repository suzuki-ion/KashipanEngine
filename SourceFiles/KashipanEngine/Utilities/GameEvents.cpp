#include "Utilities/GameEvents.h"
#include <cmath>
#include <limits>
#include <sstream>
#include <string_view>

namespace KashipanEngine::GameEvents {
namespace {
bool Fail(std::string *error, const std::string &message) { if (error) *error = message; return false; }
bool FiniteNumber(const Value &v) {
    if (!v.is_number()) return false;
    const double n = v.get<double>();
    return std::isfinite(n) && std::abs(n) <= MaxMagnitude;
}
bool IsUuidText(const std::string &text) {
    // 8-4-4-4-12 hexadecimal, the format written by UUID128::ToString.
    if (text.size() != 36) return false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; continue; }
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
bool ValidText(const std::string &text, size_t limit) {
    if (text.size() > limit) return false;
    for (unsigned char c : text) if ((c < 32 && c != '\n' && c != '\t') || c == 127) return false;
    try { (void)Value(text).dump(); } catch (const Value::exception &) { return false; }
    return true;
}
int VectorSize(ArgType type) {
    switch (type) {
    case ArgType::Vector2: return 2;
    case ArgType::Vector3: return 3;
    case ArgType::Vector4: case ArgType::Color: return 4;
    default: return 0;
    }
}
}

bool Message::GetBool(const std::string &arg, bool &out) const {
    if (!Has(arg) || !payload[arg].is_boolean()) return false;
    out = payload[arg].get<bool>(); return true;
}
bool Message::GetInt(const std::string &arg, int &out) const {
    if (!Has(arg) || !IsValidArgValue(ArgType::Int, payload[arg])) return false;
    out = payload[arg].get<int>(); return true;
}
bool Message::GetNumber(const std::string &arg, double &out) const {
    if (!Has(arg) || !payload[arg].is_number()) return false;
    out = payload[arg].get<double>(); return true;
}
bool Message::GetString(const std::string &arg, std::string &out) const {
    if (!Has(arg) || !payload[arg].is_string()) return false;
    out = payload[arg].get<std::string>(); return true;
}
bool Message::GetVector(const std::string &arg, float *out, int count) const {
    if (!out || !Has(arg) || !payload[arg].is_array() || static_cast<int>(payload[arg].size()) != count) return false;
    for (int i = 0; i < count; ++i) { if (!payload[arg][i].is_number()) return false; out[i] = payload[arg][i].get<float>(); }
    return true;
}

bool IsValidName(const std::string &name, std::string *error) {
    if (name.empty()) return Fail(error, "イベント名が空です。");
    if (name.size() > MaxNameBytes) return Fail(error, "イベント名が長すぎます（64バイトまで）。");
    for (unsigned char c : name) {
        if (c <= 32 || c == 127) return Fail(error, "イベント名に空白・制御文字は使えません。");
        if (std::string_view(".:,\"'\\/[]{}#@").find(static_cast<char>(c)) != std::string_view::npos)
            return Fail(error, "イベント名に記号 . : , \" ' \\ / [ ] { } # @ は使えません。");
    }
    // Reject the ideographic space and truncated UTF-8 so names stay printable and comparable.
    if (name.find("\xE3\x80\x80") != std::string::npos) return Fail(error, "イベント名に全角空白は使えません。");
    for (size_t i = 0; i < name.size();) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        const size_t length = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (length == 0 || i + length > name.size()) return Fail(error, "イベント名が正しいUTF-8ではありません。");
        for (size_t k = 1; k < length; ++k) if ((static_cast<unsigned char>(name[i + k]) >> 6) != 2) return Fail(error, "イベント名が正しいUTF-8ではありません。");
        i += length;
    }
    return true;
}

std::string FoldName(const std::string &name) {
    std::string folded = name;
    for (auto &c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return folded;
}
bool SameName(const std::string &a, const std::string &b) { return a.size() == b.size() && FoldName(a) == FoldName(b); }

bool ParseArgType(const std::string &text, ArgType &out) {
    const std::string folded = FoldName(text);
    for (int i = 0; i < ArgTypeCount; ++i) if (folded == ArgTypeNames[i]) { out = static_cast<ArgType>(i); return true; }
    if (folded == "float" || folded == "double") { out = ArgType::Number; return true; }
    return false;
}
const char *ArgTypeName(ArgType type) {
    const int index = static_cast<int>(type);
    return index >= 0 && index < ArgTypeCount ? ArgTypeNames[index] : "number";
}
bool ParseTargetMode(const std::string &text, TargetMode &out) {
    for (int i = 0; i < 4; ++i) if (text == TargetModeNames[i]) { out = static_cast<TargetMode>(i); return true; }
    return false;
}

bool IsValidArgValue(ArgType type, const Value &value, std::string *error) {
    switch (type) {
    case ArgType::Bool:
        return value.is_boolean() || Fail(error, "真偽値（true/false）が必要です。");
    case ArgType::Int:
        if (!value.is_number_integer()) return Fail(error, "整数が必要です。");
        if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return Fail(error, "整数が範囲外です。");
        if (value.get<long long>() < std::numeric_limits<int>::min() || value.get<long long>() > std::numeric_limits<int>::max()) return Fail(error, "整数が範囲外です。");
        return true;
    case ArgType::Number:
        return FiniteNumber(value) || Fail(error, "有限の数値が必要です。");
    case ArgType::String:
        return (value.is_string() && ValidText(value.get<std::string>(), MaxStringBytes)) || Fail(error, "1024バイト以内の文字列（制御文字なし）が必要です。");
    case ArgType::Object:
        return (value.is_string() && (value.get<std::string>().empty() || IsUuidText(value.get<std::string>()))) || Fail(error, "オブジェクトUUIDの文字列が必要です。");
    default: {
        const int size = VectorSize(type);
        if (!value.is_array() || static_cast<int>(value.size()) != size) return Fail(error, std::to_string(size) + "個の数値の配列が必要です。");
        for (const auto &item : value) if (!FiniteNumber(item)) return Fail(error, "配列の要素は有限の数値にしてください。");
        if (type == ArgType::Color) for (const auto &item : value) if (item.get<double>() < 0 || item.get<double>() > 1) return Fail(error, "色の各要素は0〜1です。");
        return true;
    }
    }
}

bool ValidateDefinition(const Definition &d, std::string *error) {
    if (!IsValidName(d.name, error)) return false;
    if (!ValidText(d.description, MaxDescriptionBytes)) return Fail(error, "説明が長すぎるか制御文字を含みます。");
    if (d.sourceText.size() > 512) return Fail(error, "生成元の文章が長すぎます。");
    if (d.args.size() > MaxArgs) return Fail(error, "引数は16個までです。");
    for (size_t i = 0; i < d.args.size(); ++i) {
        const auto &arg = d.args[i];
        std::string reason;
        if (!IsValidName(arg.name, &reason)) return Fail(error, "引数名「" + arg.name + "」: " + reason);
        for (size_t k = 0; k < i; ++k) if (SameName(d.args[k].name, arg.name)) return Fail(error, "引数名「" + arg.name + "」が重複しています。");
        if (static_cast<int>(arg.type) < 0 || static_cast<int>(arg.type) >= ArgTypeCount) return Fail(error, "引数の型が不正です。");
        if (!arg.defaultValue.is_null()) {
            if (arg.required) return Fail(error, "必須の引数「" + arg.name + "」に省略値は設定できません。");
            if (!IsValidArgValue(arg.type, arg.defaultValue, &reason)) return Fail(error, "引数「" + arg.name + "」の省略値: " + reason);
        }
    }
    return true;
}

bool BuildPayload(const Definition &d, const Value &input, Value &out, std::string *error, bool dropUndeclared) {
    if (!(input.is_null() || input.is_object())) return Fail(error, "引数はオブジェクト形式で渡してください。");
    Value result = Value::object();
    if (input.is_object()) {
        for (const auto &[key, value] : input.items()) {
            const ArgDef *arg = nullptr;
            for (const auto &candidate : d.args) if (candidate.name == key) { arg = &candidate; break; }
            if (!arg) {
                if (dropUndeclared) continue;
                return Fail(error, "イベント「" + d.name + "」に引数「" + key + "」はありません。");
            }
            Value converted = value;
            if (dropUndeclared && arg->type == ArgType::Int && value.is_number_float()) {
                const double n = value.get<double>();
                if (std::isfinite(n) && std::floor(n) == n && std::abs(n) <= 2147483647.0) converted = static_cast<int>(n);
            }
            std::string reason;
            if (!IsValidArgValue(arg->type, converted, &reason)) {
                if (dropUndeclared) continue; // a component offered a field the schema types differently
                return Fail(error, "引数「" + key + "」(" + ArgTypeName(arg->type) + "): " + reason);
            }
            result[key] = std::move(converted);
        }
    }
    for (const auto &arg : d.args) {
        if (result.contains(arg.name)) continue;
        if (arg.required) return Fail(error, "必須の引数「" + arg.name + "」がありません。");
        if (!arg.defaultValue.is_null()) result[arg.name] = arg.defaultValue;
    }
    if (result.dump().size() > MaxPayloadBytes) return Fail(error, "引数が大きすぎます（4096バイトまで）。");
    out = std::move(result);
    return true;
}

Value EncodeDefinition(const Definition &d) {
    Value args = Value::array();
    for (const auto &arg : d.args) {
        Value a{{"name", arg.name}, {"type", ArgTypeName(arg.type)}, {"required", arg.required}};
        if (!arg.defaultValue.is_null()) a["default"] = arg.defaultValue;
        args.push_back(std::move(a));
    }
    return Value{{"name", d.name}, {"description", d.description}, {"args", std::move(args)},
        {"shortTextManaged", d.shortTextManaged}, {"sourceText", d.sourceText}};
}

bool DecodeDefinition(const Value &json, Definition &out, std::string *error) {
    if (!json.is_object()) return Fail(error, "イベント定義はオブジェクトです。");
    Definition d;
    auto text = [&](const char *key, std::string &value) {
        if (!json.contains(key)) return true;
        if (!json[key].is_string()) return false;
        value = json[key].get<std::string>(); return true;
    };
    if (!text("name", d.name) || !text("description", d.description) || !text("sourceText", d.sourceText)) return Fail(error, "イベント定義の文字列項目が不正です。");
    if (json.contains("shortTextManaged")) {
        if (!json["shortTextManaged"].is_boolean()) return Fail(error, "shortTextManaged は真偽値です。");
        d.shortTextManaged = json["shortTextManaged"].get<bool>();
    }
    if (json.contains("args")) {
        if (!json["args"].is_array() || json["args"].size() > MaxArgs) return Fail(error, "args は16個以内の配列です。");
        for (const auto &a : json["args"]) {
            if (!a.is_object() || !a.contains("name") || !a["name"].is_string() || !a.contains("type") || !a["type"].is_string()) return Fail(error, "引数定義には name と type が必要です。");
            ArgDef arg;
            arg.name = a["name"].get<std::string>();
            if (!ParseArgType(a["type"].get<std::string>(), arg.type)) return Fail(error, "未知の引数型: " + a["type"].get<std::string>());
            if (a.contains("required")) {
                if (!a["required"].is_boolean()) return Fail(error, "required は真偽値です。");
                arg.required = a["required"].get<bool>();
            }
            if (a.contains("default")) arg.defaultValue = a["default"];
            d.args.push_back(std::move(arg));
        }
    }
    if (!ValidateDefinition(d, error)) return false;
    out = std::move(d);
    return true;
}

Value EncodeDefinitions(const std::vector<Definition> &definitions) {
    Value list = Value::array();
    for (const auto &d : definitions) list.push_back(EncodeDefinition(d));
    return list;
}

bool DecodeDefinitions(const Value &json, std::vector<Definition> &out, std::string *error) {
    if (!json.is_array() || json.size() > MaxDefinitions) return Fail(error, "イベント定義は256個以内の配列です。");
    std::vector<Definition> list;
    for (const auto &item : json) {
        Definition d;
        if (!DecodeDefinition(item, d, error)) return false;
        if (Find(list, d.name)) return Fail(error, "イベント名「" + d.name + "」が重複しています（大文字小文字は区別しません）。");
        list.push_back(std::move(d));
    }
    out = std::move(list);
    return true;
}

const Definition *Find(const std::vector<Definition> &definitions, const std::string &name) {
    for (const auto &d : definitions) if (SameName(d.name, name)) return &d;
    return nullptr;
}

std::string DescribeValue(const Value &value) {
    if (value.is_string()) return "\"" + value.get<std::string>() + "\"";
    return value.dump();
}

std::string DescribeDefinition(const Definition &d) {
    std::ostringstream out;
    out << d.name << "(";
    for (size_t i = 0; i < d.args.size(); ++i) {
        const auto &arg = d.args[i];
        if (i) out << ", ";
        out << arg.name << ": " << ArgTypeLabels[static_cast<int>(arg.type)];
        if (arg.required) out << " 必須";
        else if (!arg.defaultValue.is_null()) out << " = " << DescribeValue(arg.defaultValue);
    }
    out << ")";
    return out.str();
}
} // namespace KashipanEngine::GameEvents
