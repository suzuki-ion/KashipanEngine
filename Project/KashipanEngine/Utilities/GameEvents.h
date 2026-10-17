#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <json.hpp>

namespace KashipanEngine::GameEvents {
// User-defined scene events: definitions (name + typed argument schema) and validated, copy-owned payloads.
// Pure data and validation only; the scene router lives in Scene/Components/GameEventRouter.
using Value = nlohmann::json;

// Argument types a payload may carry. Object = stable object UUID string (never a pointer).
enum class ArgType { Bool, Int, Number, String, Vector2, Vector3, Vector4, Color, Object };
inline constexpr const char *ArgTypeNames[] = {"bool", "int", "number", "string", "vector2", "vector3", "vector4", "color", "object"};
inline constexpr const char *ArgTypeLabels[] = {"真偽", "整数", "数値", "文字列", "Vector2", "Vector3", "Vector4", "色", "オブジェクト"};
inline constexpr int ArgTypeCount = 9;

inline constexpr size_t MaxNameBytes = 64;
inline constexpr size_t MaxDescriptionBytes = 512;
inline constexpr size_t MaxArgs = 16;
inline constexpr size_t MaxDefinitions = 256;
inline constexpr size_t MaxStringBytes = 1024;
inline constexpr size_t MaxPayloadBytes = 4096;
inline constexpr double MaxMagnitude = 1.0e9;

struct ArgDef {
    std::string name;
    ArgType type = ArgType::Number;
    bool required = false;
    Value defaultValue; // null = no default (optional args without default are simply absent)
};
struct Definition {
    std::string name;
    std::string description;
    std::vector<ArgDef> args;
    bool shortTextManaged = false;
    std::string sourceText;
};

// Where a message is delivered. Broadcast reaches every subscriber of the event in the scene.
enum class TargetMode { Broadcast, Object, Name, Tag };
inline constexpr const char *TargetModeNames[] = {"broadcast", "object", "name", "tag"};

// One queued delivery. Payload is validated and owned by the message (copied into every receiver call).
struct Message {
    std::string name;
    Value payload = Value::object();
    std::string sender;          // sender object UUID ("" = engine / script without owner)
    TargetMode target = TargetMode::Broadcast;
    std::string targetValue;     // UUID / object name / tag
    int depth = 0;               // chain depth (handler emits get depth + 1)
    std::uint64_t serial = 0;    // router-assigned order

    bool Has(const std::string &arg) const { return payload.is_object() && payload.contains(arg); }
    bool GetBool(const std::string &arg, bool &out) const;
    bool GetInt(const std::string &arg, int &out) const;
    bool GetNumber(const std::string &arg, double &out) const; // int and number args
    bool GetString(const std::string &arg, std::string &out) const; // string and object args
    bool GetVector(const std::string &arg, float *out, int count) const; // vector2/3/4/color
};

// Name rules: 1..64 bytes UTF-8, no control/space characters and none of . : , " ' \ / [ ] { } # @.
// Names are unique ignoring ASCII case; lookups also ignore ASCII case and return the stored spelling.
bool IsValidName(const std::string &name, std::string *error = nullptr);
std::string FoldName(const std::string &name);
bool SameName(const std::string &a, const std::string &b);
bool ParseArgType(const std::string &text, ArgType &out);
const char *ArgTypeName(ArgType type);
bool ParseTargetMode(const std::string &text, TargetMode &out);

// Validates one value against a type (strict; ints must be integral numbers within int32).
bool IsValidArgValue(ArgType type, const Value &value, std::string *error = nullptr);
bool ValidateDefinition(const Definition &definition, std::string *error = nullptr);
// Builds the delivered payload: unknown arguments, missing required ones and type mismatches are rejected,
// optional defaults are filled. With dropUndeclared, extra keys are ignored and integral numbers may fill
// int args (used by gameplay components that offer standard fields such as amount/current/sender).
bool BuildPayload(const Definition &definition, const Value &input, Value &out, std::string *error = nullptr, bool dropUndeclared = false);

Value EncodeDefinition(const Definition &definition);
bool DecodeDefinition(const Value &json, Definition &out, std::string *error = nullptr);
Value EncodeDefinitions(const std::vector<Definition> &definitions);
// Validates the whole list (including case-insensitive duplicates); on failure `out` is unchanged.
bool DecodeDefinitions(const Value &json, std::vector<Definition> &out, std::string *error = nullptr);
const Definition *Find(const std::vector<Definition> &definitions, const std::string &name);

std::string DescribeDefinition(const Definition &definition);
std::string DescribeValue(const Value &value);
} // namespace KashipanEngine::GameEvents
