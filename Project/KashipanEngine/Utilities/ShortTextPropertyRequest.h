#pragma once
#include <cmath>
#include <functional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "Utilities/ShortTextBehavior.h"
#include "Utilities/ShortTextPropertyAliasCatalog.generated.h"

namespace KashipanEngine::ShortText {

// 「〇〇の〇〇」grammar. A request names one registered component member and either sets it
// once at edit time, drives it between two absolute local values while playing, or changes it
// once from one value to another over an explicit duration (Tween → a real KeyframeAnimation).
enum class PropertyMode { Set, PingPong, Tween };
// Degrees: the user writes degrees and the member stores radians (Transform.rotate_).
enum class PropertyUnit { Raw, Degrees };
enum class PropertyWave { Linear, Smooth };

inline constexpr float PropertyValueLimit = 10000.0f;
inline constexpr float PropertyDefaultPeriod = 2.0f;
inline constexpr float PropertyTweenMinDuration = 0.01f;
inline constexpr float PropertyTweenMaxDuration = 3600.0f;

struct PropertyRequest {
    std::string component;   // registered component type, e.g. "Transform"
    std::string member;      // registered member, e.g. "translate_"
    int axis = -1;           // -1: scalar member
    PropertyMode mode = PropertyMode::PingPong;
    float from = 0;          // user units (degrees when unit == Degrees); Set uses from only
    float to = 0;
    float period = PropertyDefaultPeriod; // seconds for one full from→to→from cycle
    PropertyUnit unit = PropertyUnit::Raw;
    PropertyWave wave = PropertyWave::Linear;
    bool positiveOnly = false; // sizes and scales must stay above zero
    // Tween only: seconds from `from` to `to` (played once, the end value is kept) and the
    // EaseType name used from the first key to the last key.
    float duration = 0;
    std::string ease;
    bool easeDefaulted = false; // no easing was written; Linear is used and shown in the preview
    std::vector<float> values; // Set only: an explicit whole Vector2/3/4 or Color value, never inferred/broadcast.
};

// Formal EaseType names (Utilities/MathUtils/Easings.h, same order). Kept here so the grammar
// stays engine independent; the native regression checks it against the engine table.
inline const std::vector<std::string> &PropertyEaseNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list = {"Linear"};
        for (const char *family : {"Quad", "Cubic", "Quart", "Quint", "Sine", "Expo", "Circ", "Back", "Elastic", "Bounce"})
            for (const char *kind : {"EaseIn", "EaseOut", "EaseInOut", "EaseOutIn"}) list.push_back(std::string(kind) + family);
        return list;
    }();
    return names;
}
// Case-insensitive lookup of a formal ease name; empty when unknown.
inline std::string ResolvePropertyEase(const std::string &name) {
    std::string lower;
    for (char c : name) lower += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    for (const auto &formal : PropertyEaseNames()) {
        std::string key;
        for (char c : formal) key += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        if (key == lower) return formal;
    }
    return {};
}
// Back and Elastic leave the 0..1 range, so the written value passes beyond from/to.
inline bool PropertyEaseOvershoots(const std::string &formal) {
    return formal.find("Back") != std::string::npos || formal.find("Elastic") != std::string::npos;
}

struct PropertyMemberInfo {
    bool exists = false;
    bool writable = false;
    int components = 0; // 1 scalar, 2 Vector2, 3 Vector3, 4 Vector4; 0 unsupported type
};

// The engine supplies the real registry; tests can use BuiltinPropertySchema().
struct PropertySchema {
    std::function<std::string(const std::string &lowerName)> resolveComponent;
    std::function<std::string(const std::string &component, const std::string &lowerMember)> resolveMember;
    std::function<PropertyMemberInfo(const std::string &component, const std::string &member)> member;
};

struct PropertyParseResult {
    bool recognized = false; // the sentence uses the 「〇〇の〇〇」 form; its message is authoritative
    bool success = false;
    PropertyRequest request;
    std::string message;
};

inline std::string AsciiLower(std::string s) {
    for (auto &c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return s;
}
inline bool IsAsciiIdentifier(const std::string &s) {
    if (s.empty() || s.size() > 64 || !((s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return false;
    for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

inline bool IsAngleMember(const std::string &component, const std::string &member) {
    return component == "Transform" && member == "rotate_";
}
inline bool IsPositiveMember(const std::string &component, const std::string &member) {
    return (component == "Transform" && member == "scale_") ||
        ((member == "size_" || member == "radius_") && component.size() > 8 && component.ends_with("Collider"));
}

inline PropertySchema BuiltinPropertySchema() {
    struct Entry { const char *component; const char *member; int components; bool writable; };
    static const std::vector<Entry> entries = {
        {"Transform", "translate_", 3, true}, {"Transform", "rotate_", 3, true}, {"Transform", "scale_", 3, true},
        {"Transform", "worldMatrix_", 0, false},
        {"BoxCollider", "size_", 3, true}, {"BoxCollider", "center_", 3, true},
        {"Box2DCollider", "size_", 2, true}, {"Box2DCollider", "center_", 2, true},
        {"Light", "color_", 4, true}, {"Light", "intensity_", 1, true} };
    PropertySchema schema;
    schema.resolveComponent = [](const std::string &lower) -> std::string {
        for (const auto &e : entries) if (AsciiLower(e.component) == lower) return e.component;
        return {};
    };
    schema.resolveMember = [](const std::string &component, const std::string &lower) -> std::string {
        for (const auto &e : entries) if (e.component == component) {
            const auto name = AsciiLower(e.member);
            if (name == lower || name == lower + "_") return e.member;
        }
        return {};
    };
    schema.member = [](const std::string &component, const std::string &member) {
        PropertyMemberInfo info;
        for (const auto &e : entries) if (e.component == component && e.member == member) { info = {true, e.writable, e.components}; break; }
        return info;
    };
    return schema;
}

// Natural names for registered members. Formal member names are always accepted as well.
inline std::vector<std::pair<std::string, std::string>> PropertyAliases(const std::string &component) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &entry : PropertyAliasCatalog()) if (!entry.scene && entry.component == component)
        for (const auto &member : entry.members) for (auto alias : member.aliases) {
            ReplaceAll(alias, "（", "("); ReplaceAll(alias, "）", ")"); ReplaceAll(alias, "，", ",");
            result.emplace_back(Normalize(alias), member.member);
        }
    if (component == "Transform") {
        const std::vector<std::pair<std::string, std::string>> legacy = {
        {"平行移動", "translate_"}, {"移動", "translate_"}, {"位置", "translate_"}, {"座標", "translate_"}, {"position", "translate_"},
        {"回転", "rotate_"}, {"角度", "rotate_"}, {"向き", "rotate_"}, {"rotation", "rotate_"},
        {"拡大縮小", "scale_"}, {"大きさ", "scale_"}, {"拡縮", "scale_"}, {"スケール", "scale_"} };
        result.insert(result.end(), legacy.begin(), legacy.end());
    } else {
        const std::vector<std::pair<std::string, std::string>> legacy = {
        {"サイズ", "size_"}, {"大きさ", "size_"}, {"寸法", "size_"},
        {"中心位置", "center_"}, {"中心", "center_"}, {"センター", "center_"} };
        result.insert(result.end(), legacy.begin(), legacy.end());
    }
    return result;
}

inline std::string ResolvePropertyComponent(const std::string &lower, const PropertySchema &schema) {
    if (schema.resolveComponent) if (auto formal = schema.resolveComponent(lower); !formal.empty()) return formal;
    for (const auto &entry : PropertyAliasCatalog()) if (!entry.scene)
        for (const auto &alias : entry.aliases) if (Normalize(alias) == lower)
            return schema.resolveComponent ? schema.resolveComponent(AsciiLower(entry.component)) : std::string{};
    static const std::vector<std::pair<std::string, std::string>> aliases = {
        {"トランスフォーム", "Transform"}, {"ボックスコライダー", "BoxCollider"}, {"箱コライダー", "BoxCollider"},
        {"ボックス2dコライダー", "Box2DCollider"}, {"2dボックスコライダー", "Box2DCollider"} };
    for (const auto &[alias, type] : aliases) if (lower == alias) return type;
    return schema.resolveComponent ? schema.resolveComponent(lower) : std::string{};
}

inline std::string DescribePropertyRequest(const PropertyRequest &r) {
    std::ostringstream out;
    out << r.component << "." << r.member;
    if (r.axis >= 0) out << "." << "xyz"[r.axis];
    const char *unit = r.unit == PropertyUnit::Degrees ? "度" : IsAngleMember(r.component, r.member) ? "ラジアン" : "";
    if (r.mode == PropertyMode::Set) {
        out << " を ";
        if (r.values.empty()) out << r.from;
        else { out << '('; for (size_t i = 0; i < r.values.size(); ++i) { if (i) out << ", "; out << r.values[i]; } out << ')'; }
        out << unit << " に設定（編集時に一度だけ）";
    }
    else if (r.mode == PropertyMode::Tween) out << " を " << r.from << unit << " → " << r.to << unit << " へ " << r.duration << " 秒かけて変化（"
        << r.ease << (r.easeDefaulted ? "（イージング指定なしのため既定）" : "") << "、ローカル絶対値、再生開始時に1回だけ再生し終了値を保持）";
    else out << " を " << r.from << unit << "〜" << r.to << unit << " の間で往復（ローカル絶対値、1往復 " << r.period
        << " 秒、" << (r.wave == PropertyWave::Smooth ? "なめらか" : "一定速度") << "）";
    return out.str();
}

inline bool ValidatePropertyRequest(const PropertyRequest &r, const PropertyMemberInfo &info, std::string &message) {
    auto fail = [&](std::string text) { message = std::move(text); return false; };
    const std::string name = r.component + "." + r.member;
    if (!info.exists) return fail(name + " という項目は登録されていません。");
    if (!info.writable) return fail(name + " は読み取り専用のため変更できません。");
    if (info.components < 1 || info.components > 4) return fail(name + " は数値の項目ではないため対応していません。");
    if (!r.values.empty()) {
        if (r.mode != PropertyMode::Set || r.axis != -1) return fail("成分の一括指定は、軸を指定せず固定値を設定する場合にだけ使えます。");
        if (info.components < 2 || r.values.size() != static_cast<size_t>(info.components)) return fail(name + " の成分数（" + std::to_string(info.components) + "）と指定値の数が一致しません。");
        for (float value : r.values) {
            if (!std::isfinite(value) || std::abs(value) > PropertyValueLimit) return fail("各成分は -10000〜10000 の有限値で指定してください。");
            if (r.positiveOnly && value <= 0) return fail(name + " の各成分は0より大きい値で指定してください。");
        }
        if (r.unit == PropertyUnit::Degrees && !IsAngleMember(r.component, r.member)) return fail("度の指定は Transform の回転にだけ使えます。");
        message = DescribePropertyRequest(r); return true;
    }
    if (info.components == 1 && r.axis >= 0) return fail(name + " は単一の数値です。軸は指定しないでください。");
    if (info.components > 1 && r.axis < 0) return fail(name + " は" + std::to_string(info.components) +
        "成分あります。どの軸を変えるか X軸・Y軸" + (info.components > 2 ? "・Z軸" : "") + " で指定してください（例：「" + r.component + "の…X軸を…」）。軸を推測して決めることはしません。");
    if (r.axis >= info.components || r.axis > 2) return fail(name + " には " + std::string(1, "XYZ"[std::min(r.axis, 2)]) + " 成分がありません（" + std::to_string(info.components) + "成分）。");
    if (r.unit == PropertyUnit::Degrees && !IsAngleMember(r.component, r.member)) return fail("度の指定は Transform の回転にだけ使えます。");
    auto finite = [](float v) { return std::isfinite(v) && std::abs(v) <= PropertyValueLimit; };
    const bool ranged = r.mode != PropertyMode::Set;
    if (!finite(r.from) || (ranged && !finite(r.to)))
        return fail("数値は -10000〜10000 の範囲で指定してください。");
    if (r.positiveOnly && (r.from <= 0 || (ranged && r.to <= 0)))
        return fail(name + " は0より大きい値で指定してください。");
    if (r.mode == PropertyMode::Tween) {
        if (!std::isfinite(r.duration) || r.duration < PropertyTweenMinDuration || r.duration > PropertyTweenMaxDuration)
            return fail("所要時間は0.01〜3600秒で指定してください（例：1秒かけて）。");
        if (ResolvePropertyEase(r.ease) != r.ease || r.ease.empty()) return fail("イージング「" + r.ease + "」は使えません。Linear・EaseInOutCubic などの正式名で指定してください。");
        if (r.positiveOnly && PropertyEaseOvershoots(r.ease))
            return fail(name + " は0より大きい値が必要なため、範囲の外へはみ出す " + r.ease + "（Back/Elastic系）は使えません。Linear・EaseInOutCubic などを指定してください。");
    }
    if (r.mode == PropertyMode::PingPong) {
        if (!(r.from < r.to)) return fail("範囲は小さい値から大きい値の順で、異なる値を指定してください（例：-1から1の間）。");
        if (!std::isfinite(r.period) || r.period < 0.05f || r.period > 3600) return fail("周期は0.05〜3600秒で指定してください。");
    }
    message = DescribePropertyRequest(r);
    return true;
}

// Absolute value at time t: from at t=0, to at half a period, back at a full period.
inline float EvaluatePropertyPingPong(float from, float to, double period, double elapsed, PropertyWave wave) {
    if (!std::isfinite(elapsed) || !std::isfinite(period) || period <= 0) return from;
    const double phase = std::fmod(std::max(0.0, elapsed), period) / period; // 0..1
    double weight = 0;
    if (wave == PropertyWave::Smooth) weight = 0.5 - 0.5 * std::cos(phase * 2 * 3.14159265358979323846);
    else weight = phase < 0.5 ? phase * 2 : 2 - phase * 2;
    weight = std::clamp(weight, 0.0, 1.0);
    return static_cast<float>(from + (static_cast<double>(to) - from) * weight);
}

inline PropertyParseResult ParsePropertyRequest(const std::string &input, const PropertySchema &schema) {
    PropertyParseResult result;
    auto fail = [&](const std::string &message) { result.success = false; result.message = message; return result; };
    if (input.empty() || input.size() > 512) return result;
    std::string prepared = input;
    for (const auto &[from, to] : std::vector<std::pair<std::string_view, std::string_view>>{
        {"\xE2\x88\x92", "-"}, {"ｆ", "f"}, {"Ｆ", "f"}, {"〜", "から"}, {"～", "から"},
        {"（", "("}, {"）", ")"}, {"，", ","} }) ReplaceAll(prepared, from, to);
    std::string text = Normalize(prepared);
    // Duration of a one-shot change: 「N秒かけて」 before the component or after the range.
    std::vector<std::string> durations;
    {
        const std::regex durationWord("([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+))f?秒(?:間)?かけて");
        std::smatch match;
        while (std::regex_search(text, match, durationWord)) {
            durations.push_back(match[1].str());
            text.erase(static_cast<size_t>(match.position()), static_cast<size_t>(match.length()));
        }
    }
    auto separator = text.find("の");
    // An alias itself may contain の: prefer the longest registered component prefix.
    for (const auto &entry : PropertyAliasCatalog()) if (!entry.scene) for (const auto &alias : entry.aliases) {
        const auto key = Normalize(alias);
        if (text.starts_with(key + "の") && key.size() > separator && !ResolvePropertyComponent(key, schema).empty()) separator = key.size();
    }
    if (separator == std::string::npos || separator == 0) return result;
    const std::string componentText = text.substr(0, separator);
    std::string rest = text.substr(separator + std::string("の").size());
    const bool markers = rest.find("から") != std::string::npos || rest.find("往復") != std::string::npos ||
        std::regex_search(rest, std::regex("(?:に|へ)(?:する|して|設定|変更|します)"));
    const std::string component = ResolvePropertyComponent(componentText, schema);
    if (component.empty()) {
        if (!IsAsciiIdentifier(componentText) || !markers) return result;
        result.recognized = true;
        return fail("コンポーネント「" + componentText + "」は登録されていません。Transform・BoxCollider・Box2DCollider など登録済みの型名を指定してください。");
    }
    result.recognized = true;
    auto &r = result.request;
    r.component = component;

    // Axis words may appear anywhere after the component: 「移動X軸を」「X軸の移動を」.
    int axes = 0;
    {
        const std::regex axisWord("([xyz])軸(?:方向|成分)?(?:の)?");
        std::smatch match;
        while (std::regex_search(rest, match, axisWord)) {
            r.axis = match[1].str()[0] - 'x';
            ++axes;
            rest.erase(static_cast<size_t>(match.position()), static_cast<size_t>(match.length()));
        }
    }
    // Property: the longest natural alias or a formal member name (translate_ / translate).
    std::string member, propertyText;
    for (const auto &[alias, name] : PropertyAliases(component)) {
        if (rest.starts_with(alias) && alias.size() > propertyText.size()) {
            const auto info = schema.member ? schema.member(component, name) : PropertyMemberInfo{};
            if (info.exists) { member = name; propertyText = alias; }
        }
    }
    {
        std::smatch match;
        if (std::regex_search(rest, match, std::regex("^[a-z_][a-z0-9_]*")) && match.length() > static_cast<std::ptrdiff_t>(propertyText.size())) {
            const auto formal = schema.resolveMember ? schema.resolveMember(component, match[0].str()) : std::string{};
            if (!formal.empty()) { member = formal; propertyText = match[0].str(); }
            else if (member.empty()) return fail(component + " に「" + match[0].str() + "」という項目はありません。");
        }
    }
    if (member.empty()) {
        const auto end = rest.find("を");
        const auto shown = rest.substr(0, end == std::string::npos ? std::min<size_t>(rest.size(), 24) : end);
        return fail(component + " の項目「" + shown + "」が分かりません。例：Transformの移動 / 回転 / スケール、BoxColliderのサイズ / 中心、または登録名（translate_ など）。");
    }
    r.member = member;
    rest.erase(0, propertyText.size());
    {
        std::smatch match;
        if (std::regex_search(rest, match, std::regex("^\\.([xyz])"))) { r.axis = match[1].str()[0] - 'x'; ++axes; rest.erase(0, 2); }
    }
    if (axes > 1) return fail("軸はひとつだけ指定してください。");
    // A trailing axis can follow the property: 「移動をX軸で」 has been removed above.
    for (bool stripped = true; stripped;) {
        stripped = false;
        for (const std::string particle : {"を", "は", "の", "で"}) if (rest.starts_with(particle)) { rest.erase(0, particle.size()); stripped = true; }
    }

    bool periodSet = false;
    {
        const std::vector<std::regex> periods = {
            std::regex("(?:1往復|周期)([0-9]+(?:\\.[0-9]+)?)f?秒(?:で|の|周期)?"), std::regex("([0-9]+(?:\\.[0-9]+)?)f?秒周期(?:で|の)?") };
        for (const auto &expression : periods) {
            std::smatch match;
            if (!std::regex_search(rest, match, expression)) continue;
            if (periodSet) return fail("周期は一度だけ指定してください。");
            try { r.period = std::stof(match[1].str()); } catch (const std::exception &) { return fail("周期の数値が不正です。"); }
            periodSet = true;
            rest.erase(static_cast<size_t>(match.position()), static_cast<size_t>(match.length()));
            if (std::regex_search(rest, expression)) return fail("周期は一度だけ指定してください。");
        }
    }
    // Easing word: 「EaseInOutCubicで」. Formal names only (case-insensitive); unknown names are rejected.
    std::string ease;
    {
        std::smatch match;
        if (std::regex_search(rest, match, std::regex("([a-z][a-z0-9]{3,})で"))) {
            ease = ResolvePropertyEase(match[1].str());
            if (ease.empty()) return fail("イージング「" + match[1].str() + "」は登録されていません。Linear・EaseInQuad・EaseOutQuad・EaseInOutQuad・EaseInCubic・EaseOutCubic・EaseInOutCubic などの正式名で指定してください。");
            rest.erase(static_cast<size_t>(match.position()), static_cast<size_t>(match.length()));
            if (std::regex_search(rest, match, std::regex("([a-z][a-z0-9]{3,})で")) && !ResolvePropertyEase(match[1].str()).empty())
                return fail("イージングはひとつだけ指定してください。");
        }
    }
    const bool smooth = Consume(rest, {"なめらかに", "滑らかに", "ゆるやかに"});
    const bool linear = Consume(rest, {"一定の速さで", "一定速度で", "等速で", "直線的に"});
    if (smooth && linear) return fail("往復の方式はひとつにしてください。");
    r.wave = smooth ? PropertyWave::Smooth : PropertyWave::Linear;
    const bool radians = rest.find("ラジアン") != std::string::npos || rest.find("rad") != std::string::npos;
    const bool degrees = rest.find("度") != std::string::npos;
    if (radians && degrees) return fail("度とラジアンを同時に指定しないでください。");
    for (bool stripped = true; stripped;) {
        stripped = false;
        for (const std::string particle : {"を", "は", "で"}) if (rest.starts_with(particle)) { rest.erase(0, particle.size()); stripped = true; }
    }

    const std::string number = "([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+))f?(?:度|ラジアン|rad)?";
    const std::regex range("^" + number + "から" + number +
        "(?:までの間|までの範囲|の範囲内|の範囲|の間|まで)?(?:で|を)?(?:往復移動|往復回転|往復拡縮|往復変化|往復|行ったり来たり)(?:させてください|してください|させて|させる|させ|して|する|します|し)?$");
    const std::regex set("^" + number + "(?:に|へ)(?:設定してください|変更してください|してください|設定する|設定して|設定|変更する|変更して|変更|する|して|します)?$");
    const std::regex tupleSet("^\\(([^()]*)\\)(?:に|へ)(?:設定してください|変更してください|してください|設定する|設定して|設定|変更する|変更して|変更|する|して|します)?$");
    auto parseNumber = [](const std::string &text, float &value) {
        try {
            const double parsed = std::stod(text);
            if (!std::isfinite(parsed) || std::abs(parsed) > 1e30) return false;
            value = static_cast<float>(parsed);
            return true;
        } catch (const std::exception &) { return false; }
    };
    // One-shot change: 「AからBまで(…で)移動/回転/変更」 together with 「N秒かけて」.
    const std::regex tween("^" + number + "から" + number +
        "(?:まで|へ|に)?(?:で)?(?:移動|回転|変更|変化|拡縮|拡大|縮小|補間|アニメーション)(?:させてください|してください|させて|させる|させ|して|する|します|し)?$");
    if (durations.size() > 1) return fail("所要時間（N秒かけて）は一度だけ指定してください。");
    const bool timed = !durations.empty();
    std::smatch match;
    if (std::regex_match(rest, match, tupleSet)) {
        r.mode = PropertyMode::Set;
        if (timed || !ease.empty() || periodSet || smooth || linear) return fail("成分の一括指定は固定値の設定です。時間・イージング・往復を同時に指定できません。");
        const auto content = match[1].str();
        size_t begin = 0;
        for (;;) {
            const auto end = content.find(',', begin);
            const auto token = content.substr(begin, end == std::string::npos ? end : end - begin);
            std::smatch scalar;
            float value = 0;
            if (!std::regex_match(token, scalar, std::regex("^" + number + "$")) || !parseNumber(scalar[1].str(), value)) return fail("各成分に数値を指定してください（例：(0.5f, 0.5f, 1.0f, 1.0f)）。");
            r.values.push_back(value);
            if (r.values.size() > 4) return fail("一括指定は2〜4成分までです。");
            if (end == std::string::npos) break;
            begin = end + 1;
        }
    } else if (std::regex_match(rest, match, range)) {
        r.mode = PropertyMode::PingPong;
        if (timed || !ease.empty()) return fail("往復の速さは「周期N秒」で指定してください。「N秒かけて」とイージングは一回だけの変化（AからBまでN秒かけて…で移動）で使います。");
        if (!parseNumber(match[1].str(), r.from) || !parseNumber(match[2].str(), r.to)) return fail("数値が不正です。");
    } else if (std::regex_match(rest, match, tween)) {
        r.mode = PropertyMode::Tween;
        if (!timed) return fail("一回だけの変化には所要時間を「N秒かけて」で指定してください（例：1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動）。");
        if (periodSet || smooth || linear) return fail("周期・往復の方式は往復（AからBの間で往復）でだけ使えます。一回だけの変化ではイージング名（EaseInOutCubic など）で指定してください。");
        if (!parseNumber(match[1].str(), r.from) || !parseNumber(match[2].str(), r.to)) return fail("数値が不正です。");
        if (!parseNumber(durations.front(), r.duration)) return fail("所要時間の数値が不正です。");
        r.ease = ease.empty() ? std::string("Linear") : ease;
        r.easeDefaulted = ease.empty();
    } else if (std::regex_match(rest, match, set)) {
        r.mode = PropertyMode::Set;
        if (periodSet || smooth || linear) return fail("周期・往復の方式は「AからBの間で往復」と組み合わせて指定してください。");
        if (timed || !ease.empty()) return fail("「N秒かけて」とイージングは開始値と終了値の両方が必要です（例：0から4まで1秒かけてEaseInOutCubicで移動）。");
        if (!parseNumber(match[1].str(), r.from)) return fail("数値が不正です。");
        r.to = r.from;
    } else if (rest.find("から") != std::string::npos) {
        return fail("範囲は「-1.0fから1.0fの間で往復」または「0から4まで1秒かけてEaseInOutCubicで移動」のように指定してください。解釈できない部分：「" + rest + "」");
    } else {
        return fail("「AからBの間で往復」「AからBまでN秒かけて移動」または「Nにする」の形で指定してください。解釈できない部分：「" + rest + "」");
    }
    const bool angle = IsAngleMember(r.component, r.member);
    if (degrees && !angle) return fail("度の指定は Transform の回転にだけ使えます。");
    if (radians && !angle) return fail("ラジアンの指定は Transform の回転にだけ使えます。");
    r.unit = angle && !radians ? PropertyUnit::Degrees : PropertyUnit::Raw;
    r.positiveOnly = IsPositiveMember(r.component, r.member);
    const auto info = schema.member ? schema.member(r.component, r.member) : PropertyMemberInfo{};
    std::string message;
    if (!ValidatePropertyRequest(r, info, message)) return fail(message);
    result.success = true;
    result.message = message;
    return result;
}
} // namespace KashipanEngine::ShortText
