#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "Utilities/ShortTextProgram.h"

namespace KashipanEngine::ShortText {

// A deliberately bounded grammar. Unknown text must never become a partial action.
enum class Kind { Oscillate, Rotate, Move, Rules };
struct Recipe {
    Kind kind = Kind::Oscillate;
    int axis = 1; // local Transform coordinates: X/Y/Z
    int direction = 1;
    float amplitude = 0.5f;
    float period = 2.0f;
    float speed = 1.0f; // units/sec for Move, degrees/sec for Rotate
    std::vector<Rule> rules;
};
struct Result {
    bool success = false;
    Recipe recipe;
    std::string message;
};

inline bool IsValid(const Recipe &r) {
    if (r.kind == Kind::Rules) {
        if (r.rules.empty() || r.rules.size() > 16) return false;
        for (const auto &rule : r.rules) if (!IsValidRule(rule)) return false;
        return true;
    }
    return r.rules.empty() && r.kind >= Kind::Oscillate && r.kind <= Kind::Move && r.axis >= 0 && r.axis <= 2 &&
        (r.direction == 1 || r.direction == -1) &&
        std::isfinite(r.amplitude) && r.amplitude > 0 && r.amplitude <= 10000 &&
        std::isfinite(r.period) && r.period >= 0.05f && r.period <= 3600 &&
        std::isfinite(r.speed) && r.speed > 0 && r.speed <= 10000;
}

inline void ReplaceAll(std::string &s, std::string_view from, std::string_view to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

inline std::string Normalize(std::string s) {
    const std::array<std::string_view, 10> digits = { "０", "１", "２", "３", "４", "５", "６", "７", "８", "９" };
    for (size_t i = 0; i < digits.size(); ++i) ReplaceAll(s, digits[i], std::to_string(i));
    for (const auto &[from, to] : std::vector<std::pair<std::string_view, std::string_view>>{
        {"Ｘ", "x"}, {"Ｙ", "y"}, {"Ｚ", "z"}, {"ｘ", "x"}, {"ｙ", "y"}, {"ｚ", "z"},
        {"．", "."}, {"－", "-"}, {"＋", "+"}, {"　", ""}, {"。", ""}, {"、", ""}, {"！", ""} }) ReplaceAll(s, from, to);
    std::string out;
    for (unsigned char c : s) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '!') continue;
        out += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    }
    return out;
}

inline bool Consume(std::string &s, const std::vector<std::string> &words) {
    bool found = false;
    for (const auto &word : words) {
        if (s.find(word) != std::string::npos) { found = true; ReplaceAll(s, word, ""); }
    }
    return found;
}

inline bool HasUnsupportedGrammar(const std::string &normalizedText) {
    for (const auto &word : { "ない", "なく", "禁止", "以外", "せず", "ずに", "たら", "なら", "とき", "時に", "時は", "場合", "けど", "ただし", "そして", "ながら", "止め", "停止" }) {
        if (normalizedText.find(word) != std::string::npos) return true;
    }
    std::string remaining = normalizedText;
    const bool rotate = Consume(remaining, {"回転", "回して", "回す", "回る", "回れ"});
    const bool other = Consume(remaining, {"ふわふわ", "ゆらゆら", "往復", "浮いたり沈んだり", "行ったり来たり", "移動", "動か", "動く", "進む", "進め", "進ませ"});
    if (rotate && other) return true;
    return false;
}

inline std::string Describe(const Recipe &r) {
    if (!IsValid(r)) return "不正な挙動設定です。";
    if (r.kind == Kind::Rules) {
        std::ostringstream message;
        message << "イベント・条件・動作：" << r.rules.size() << " ルール";
        for (const auto &rule : r.rules) {
            message << "\n" << EventLabels[static_cast<size_t>(rule.event)];
            if (rule.event >= Event::KeyPressed && rule.event <= Event::KeyReleased) message << " [" << KeyName(rule.key) << "]";
            if (rule.event == Event::Timer) message << " (" << rule.interval << "秒)";
            for (const auto &condition : rule.conditions) message << " / " << "XYZ"[condition.axis] << TestLabels[static_cast<size_t>(condition.test)] << " " << condition.value;
            message << " → ";
            for (const auto &action : rule.actions) message << OperationLabels[static_cast<size_t>(action.operation)] << " " << "XYZ"[action.axis] << "=" << action.value << "; ";
        }
        return message.str();
    }
    std::ostringstream message;
    message << "ローカル" << "XYZ"[r.axis] << "軸：";
    if (r.kind == Kind::Oscillate) message << "往復、振幅 " << r.amplitude << "、周期 " << r.period << " 秒";
    else if (r.kind == Kind::Rotate) message << "回転、" << r.direction * r.speed << " 度/秒";
    else message << "移動、" << r.direction * r.speed << " 単位/秒";
    return message.str();
}

inline Result Parse(const std::string &input) {
    Result result;
    auto fail = [&](const std::string &message) { result.message = message; return result; };
    if (input.empty() || input.size() > 512) return fail("1〜512バイトの短い文章を入力してください。");
    std::string text = Normalize(input);
    if (text.empty()) return fail("挙動を入力してください。");
    // Refuse negation/conditions/compound requests before extracting any keywords.
    if (HasUnsupportedGrammar(text)) return fail("条件・否定・停止・複数の挙動は未対応です。ひとつの動きを指定してください。");
    const bool rotate = Consume(text, {"回転させ", "回転し", "回転", "回して", "回す", "回る", "回れ"});
    const bool oscillate = Consume(text, {"浮いたり沈んだり", "行ったり来たり", "ふわふわ", "ゆらゆら", "往復させ", "往復し", "往復", "揺らして", "揺れる"});
    const bool move = Consume(text, {"移動させ", "移動し", "移動", "進ませ", "進めて", "進む", "動かして", "動かす", "動かせ", "動く"});
    if (rotate && (oscillate || move)) return fail("回転と移動は分けて入力してください。");
    if (!rotate && !oscillate && !move) return fail("往復・回転・移動に対応しています。例：上下にふわふわ動かして");
    auto &r = result.recipe;
    const bool pairedDirection = text.find("上下") != std::string::npos || text.find("左右") != std::string::npos || text.find("前後") != std::string::npos;
    r.kind = rotate ? Kind::Rotate : (oscillate || pairedDirection) ? Kind::Oscillate : Kind::Move;
    r.axis = rotate ? 2 : 1;
    r.speed = rotate ? 90.0f : 1.0f;

    bool amplitudeSet = false, periodSet = false, speedSet = false;
    auto number = [&](const char *pattern, float &value, bool &wasSet) -> bool {
        std::smatch match;
        const std::regex expression(pattern);
        if (!std::regex_search(text, match, expression)) return true;
        if (wasSet) return false;
        try { value = std::stof(match[1].str()); } catch (...) { return false; }
        wasSet = true;
        text.erase(static_cast<size_t>(match.position()), static_cast<size_t>(match.length()));
        return !std::regex_search(text, expression);
    };
    if (!number("(?:振幅|幅)([0-9]+(?:\\.[0-9]+)?)(?:単位)?", r.amplitude, amplitudeSet) ||
        !number("周期([0-9]+(?:\\.[0-9]+)?)秒", r.period, periodSet) ||
        !number("([0-9]+(?:\\.[0-9]+)?)秒周期", r.period, periodSet) ||
        !number(rotate ? "毎秒([0-9]+(?:\\.[0-9]+)?)(?:度)?" : "毎秒([0-9]+(?:\\.[0-9]+)?)(?:単位)?", r.speed, speedSet) ||
        !number(rotate ? "速度([0-9]+(?:\\.[0-9]+)?)(?:度/秒)?" : "速度([0-9]+(?:\\.[0-9]+)?)(?:単位/秒)?", r.speed, speedSet))
        return fail("数値の重複または不正な数値があります。");
    if ((r.kind != Kind::Oscillate && (amplitudeSet || periodSet)) || (r.kind == Kind::Oscillate && speedSet))
        return fail("往復は振幅・周期、回転と移動は毎秒の速度を指定してください。");

    const bool slow = Consume(text, {"ゆっくりめ", "ゆっくり", "遅く"});
    const bool fast = Consume(text, {"すばやく", "素早く", "速く", "高速で"});
    if ((slow && fast) || ((slow || fast) && (periodSet || speedSet))) return fail("速さの指定はひとつにしてください。");
    if (slow) { r.period = 4; r.speed *= 0.5f; }
    if (fast) { r.period = 1; r.speed *= 2; }

    int axes = 0;
    for (int axis = 0; axis < 3; ++axis) {
        const std::string name(1, "xyz"[axis]);
        const bool explicitAxis = Consume(text, {name + "軸方向", name + "軸"});
        const bool pair = axis == 0 ? Consume(text, {"左右", "横方向", "横"}) :
            axis == 1 ? Consume(text, {"上下", "縦方向", "縦"}) : Consume(text, {"前後"});
        if (explicitAxis || pair) { r.axis = axis; ++axes; }
    }
    int directions = 0;
    struct Direction { std::vector<std::string> words; int axis; int sign; };
    const std::vector<Direction> directionWords = rotate ? std::vector<Direction>{
        {{"反時計回り", "左回り", "左"}, 2, 1}, {{"時計回り", "右回り", "右"}, 2, -1} } : std::vector<Direction>{
        {{"右方向", "右"}, 0, 1}, {{"左方向", "左"}, 0, -1},
        {{"上方向", "上"}, 1, 1}, {{"下方向", "下"}, 1, -1},
        {{"前方向", "前"}, 2, 1}, {{"後ろ方向", "後ろ", "後"}, 2, -1} };
    for (const auto &direction : directionWords) {
        if (Consume(text, direction.words)) {
            if (axes == 0) r.axis = direction.axis;
            else if (!rotate && r.axis != direction.axis) return fail("軸と方向が一致しません。");
            r.direction = direction.sign;
            ++directions;
        }
    }
    if (axes > 1 || directions > 1) return fail("軸・方向はひとつだけ指定してください。");
    if (r.kind == Kind::Move && (axes == 0 && directions == 0)) return fail("移動方向を指定してください。例：右に毎秒2で動かして");
    if (r.kind == Kind::Move && axes > 0 && directions == 0) return fail("一方向の移動には右・左・上・下・前・後ろを指定してください。");
    if (r.kind == Kind::Oscillate && directions > 0) return fail("往復には上下・左右・前後・X軸などを指定してください。");
    Consume(text, {"選択中のオブジェクトを", "このオブジェクトを", "オブジェクトを", "これを", "ください", "させて", "して", "させる", "する", "て", "ずっと", "常に", "方向", "に", "へ", "で", "を"});
    if (!text.empty()) return fail("解釈できない部分があります：「" + text + "」。例文に近い短い文章で指定してください。");
    if (!IsValid(r)) return fail("振幅と速度は0より大きく10000以下、周期は0.05〜3600秒で指定してください。");
    result.success = true;
    result.message = Describe(r);
    return result;
}

struct Delta { std::array<float, 3> translation{}; std::array<float, 3> rotation{}; };
// Oscillation is additive: its full-cycle displacement is zero, with no scene edit-time drift.
inline Delta EvaluateDelta(const Recipe &r, double elapsed, double dt) {
    Delta delta;
    if (!IsValid(r) || r.kind == Kind::Rules || !std::isfinite(elapsed) || !std::isfinite(dt) || dt < 0) return delta;
    constexpr double pi = 3.14159265358979323846;
    if (r.kind == Kind::Oscillate) {
        const double phase = std::fmod(elapsed, r.period) * (2 * pi / r.period);
        delta.translation[r.axis] = static_cast<float>(r.amplitude * (std::sin(phase + dt * (2 * pi / r.period)) - std::sin(phase)));
    } else if (r.kind == Kind::Rotate) {
        delta.rotation[r.axis] = static_cast<float>(std::remainder(r.direction * r.speed * dt * pi / 180, 2 * pi));
    } else delta.translation[r.axis] = static_cast<float>(r.direction * r.speed * dt);
    return delta;
}
} // namespace KashipanEngine::ShortText
