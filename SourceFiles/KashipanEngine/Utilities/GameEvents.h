#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <json.hpp>

namespace KashipanEngine::GameEvents {
// ユーザー定義のシーンイベント。名前と型付き引数スキーマの定義、および検証済みでコピー所有するペイロード。
// 純粋なデータと検証のみを扱う。シーンのルーターはScene/Components/GameEventRouterにある。
using Value = nlohmann::json;

// ペイロードに格納できる引数型。Objectは安定したオブジェクトUUID文字列であり、ポインターではない。
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
    Value defaultValue; // nullは既定値なしを示す。既定値のない省略可能な引数は、単に省略する。
};
struct Definition {
    std::string name;
    std::string description;
    std::vector<ArgDef> args;
    bool shortTextManaged = false;
    std::string sourceText;
};

// メッセージの配信先。Broadcastはシーン内でそのイベントを購読するすべての受信者へ届く。
enum class TargetMode { Broadcast, Object, Name, Tag };
inline constexpr const char *TargetModeNames[] = {"broadcast", "object", "name", "tag"};

// キュー内の1配信分。ペイロードは検証済みでメッセージが所有し、受信者の呼び出しごとにコピーする。
struct Message {
    std::string name;
    Value payload = Value::object();
    std::string sender;          // 送信元オブジェクトのUUID（空文字列はエンジン、または所有元のないスクリプト）
    TargetMode target = TargetMode::Broadcast;
    std::string targetValue;     // UUID・オブジェクト名・タグ
    int depth = 0;               // 連鎖の深さ（ハンドラーから送信した場合は深さに1を加える）
    std::uint64_t serial = 0;    // ルーターが割り当てる順序

    bool Has(const std::string &arg) const { return payload.is_object() && payload.contains(arg); }
    bool GetBool(const std::string &arg, bool &out) const;
    bool GetInt(const std::string &arg, int &out) const;
    bool GetNumber(const std::string &arg, double &out) const; // 整数型・数値型の引数
    bool GetString(const std::string &arg, std::string &out) const; // 文字列型・オブジェクト型の引数
    bool GetVector(const std::string &arg, float *out, int count) const; // 2～4次元ベクトル・色
};

// 名前の規則: UTF-8で1～64バイト。制御文字・空白と、. : , " ' \ / [ ] { } # @は使用不可。
// 名前はASCIIの大文字・小文字を区別せず一意とする。検索時も区別せず、保存された表記を返す。
bool IsValidName(const std::string &name, std::string *error = nullptr);
std::string FoldName(const std::string &name);
bool SameName(const std::string &a, const std::string &b);
bool ParseArgType(const std::string &text, ArgType &out);
const char *ArgTypeName(ArgType type);
bool ParseTargetMode(const std::string &text, TargetMode &out);

// 1つの値を型に対して厳密に検証する。整数型はint32範囲内の整数値に限定する。
bool IsValidArgValue(ArgType type, const Value &value, std::string *error = nullptr);
bool ValidateDefinition(const Definition &definition, std::string *error = nullptr);
// 配信用のペイロードを構築し、未知の引数・必須引数の欠落・型の不一致を拒否する。
// 省略された引数には既定値を補う。dropUndeclared指定時は余分なキーを無視し、整数値を
// 整数型の引数に使用できる（amount/current/senderなどの標準フィールドを提供するゲームプレイ用コンポーネント向け）。
bool BuildPayload(const Definition &definition, const Value &input, Value &out, std::string *error = nullptr, bool dropUndeclared = false);

Value EncodeDefinition(const Definition &definition);
bool DecodeDefinition(const Value &json, Definition &out, std::string *error = nullptr);
Value EncodeDefinitions(const std::vector<Definition> &definitions);
// 大文字・小文字を区別しない重複も含めて一覧全体を検証する。失敗時はoutを変更しない。
bool DecodeDefinitions(const Value &json, std::vector<Definition> &out, std::string *error = nullptr);
const Definition *Find(const std::vector<Definition> &definitions, const std::string &name);

std::string DescribeDefinition(const Definition &definition);
std::string DescribeValue(const Value &value);
} // namespace KashipanEngine::GameEvents
