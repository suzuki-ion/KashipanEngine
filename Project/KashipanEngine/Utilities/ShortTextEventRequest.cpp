#include "Utilities/ShortTextEventRequest.h"
#include <cmath>
#include <regex>
#include <sstream>

namespace KashipanEngine::ShortText {
namespace {
using GameEvents::Value;
const std::string kNumber = R"(([-+]?\d+(?:\.\d+)?)[fF]?)";

bool Match(const std::string &text, const std::string &pattern, std::smatch &m) {
    return std::regex_match(text, m, std::regex(pattern));
}
bool IsIdentifier(const std::string &text) {
    static const std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]{0,63})");
    return std::regex_match(text, identifier);
}
bool ParseNumber(const std::string &text, double &out) {
    std::smatch m;
    if (!Match(text, kNumber, m) || m[1].length() > 32) return false;
    try { out = std::stod(m[1].str()); } catch (const std::exception &) { return false; } // out_of_range: rejected, never thrown to the UI
    return std::isfinite(out);
}
std::vector<std::string> Split(const std::string &text, const std::string &separator) {
    std::vector<std::string> parts;
    size_t begin = 0;
    std::string quote;
    for (size_t pos = 0; pos < text.size();) {
        if (!quote.empty()) {
            if (quote.size() == 1 && text[pos] == '\\' && pos + 1 < text.size()) { pos += 2; continue; }
            if (text.compare(pos, quote.size(), quote) == 0) { pos += quote.size(); quote.clear(); }
            else ++pos;
            continue;
        }
        if (text.compare(pos, 3, "「") == 0) { quote = "」"; pos += 3; continue; }
        if (text.compare(pos, 3, "『") == 0) { quote = "』"; pos += 3; continue; }
        if (text[pos] == '"' || text[pos] == '\'') { quote.assign(1, text[pos++]); continue; }
        if (text.compare(pos, separator.size(), separator) == 0) {
            parts.push_back(text.substr(begin, pos - begin)); pos += separator.size(); begin = pos;
        } else ++pos;
    }
    parts.push_back(text.substr(begin));
    return parts;
}
std::string StripQuotes(const std::string &text) {
    for (const auto &[open, close] : std::vector<std::pair<std::string, std::string>>{{"「", "」"}, {"\"", "\""}, {"『", "』"}}) {
        if (text.size() >= open.size() + close.size() && text.starts_with(open) && text.ends_with(close))
            return text.substr(open.size(), text.size() - open.size() - close.size());
    }
    return text;
}
// Key names accepted in sentences, returned in the engine's KeyName spelling.
std::string KeyText(const std::string &text) {
    if (text.size() == 1 && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= '0' && text[0] <= '9'))) return text;
    if (text.size() == 1 && text[0] >= 'a' && text[0] <= 'z') return std::string(1, static_cast<char>(text[0] - 'a' + 'A'));
    if (text == "Space" || text == "スペース") return "Space";
    if (text == "Enter" || text == "エンター") return "Enter";
    return {};
}
EventValueText ValueText(const std::string &text) {
    double number = 0;
    EventValueText value{StripQuotes(text), false};
    if (text.starts_with("受信した")) { value.text = text.substr(std::string("受信した").size()); value.fromPayload = true; }
    else if (!ParseNumber(text, number) && IsIdentifier(text) && text != "true" && text != "false") value.fromPayload = true;
    return value;
}

struct Parsed {
    bool recognized = false;
    std::string error;
};
// Parses the action part of a handler sentence. Unrecognized wording leaves recognized=false (the caller falls through).
Parsed ParseAction(const std::string &text, bool receiving, EventActionRequest &action) {
    std::smatch m;
    // Tween: delegated to the property grammar by the plan.
    if (receiving && (Match(text, R"(\d+(?:\.\d+)?秒かけて.+)", m) || (text.find("かけて") != std::string::npos && text.find("まで") != std::string::npos && text.find("の") != std::string::npos))) {
        action.kind = EventActionKind::Tween;
        action.tweenText = text;
        return {true, {}};
    }
    if (Match(text, R"((.+?)関数に(?:イベント|GameEvent)を渡して呼ぶ)", m)) {
        action.kind = EventActionKind::CallScript;
        action.function = m[1];
        action.passEvent = true;
        return {true, IsIdentifier(action.function) ? std::string{} : "関数名は英数字と _ で指定してください: " + action.function};
    }
    if (Match(text, R"((.+?)関数を(?:(.+?)を渡して)?呼ぶ)", m)) {
        action.kind = EventActionKind::CallScript;
        action.function = m[1];
        if (!IsIdentifier(action.function)) return {true, "関数名は英数字と _ で指定してください: " + action.function};
        if (m[2].matched) {
            for (auto &arg : Split(m[2].str(), "と")) for (auto &name : Split(arg, ",")) {
                if (!IsIdentifier(name)) return {true, "関数へ渡す受信引数の名前が不正です: " + name};
                action.functionArgs.push_back(name);
            }
            if (!receiving) return {true, "関数へ受信した引数を渡せるのは「〜を受け取ったら」の場合だけです。"};
            if (action.functionArgs.size() > 4) return {true, "関数の引数は4個までです。"};
        }
        return {true, {}};
    }
    // 「Damageイベントで対象Enemyに10を渡す」: the value goes to the event's only argument.
    if (Match(text, R"((.+?)(?:イベント)?で(?:(対象|タグ)(.+?)に)?(.+?)を渡す)", m)) {
        action.kind = EventActionKind::Emit;
        action.event = m[1];
        if (m[2].matched) { action.targetMode = m[2] == "タグ" ? 2 : 1; action.target = m[3]; }
        action.args.emplace_back("", ValueText(m[4]));
        return {true, GameEvents::IsValidName(action.event) ? std::string{} : "イベント名が不正です: " + action.event};
    }
    if (Match(text, R"((.+?)(?:イベント)?を(?:(対象|タグ)(.+?)に|(全体)に|(送信元)に)?(?:、)?(.*?)(?:発火|送信|送る)(?:する)?)", m)) {
        action.kind = EventActionKind::Emit;
        action.event = m[1];
        if (m[2].matched) { action.targetMode = m[2] == "タグ" ? 2 : 1; action.target = m[3]; }
        else if (m[5].matched) action.targetMode = 3;
        std::string error;
        if (!GameEvents::IsValidName(action.event, &error)) return {true, "イベント名「" + action.event + "」: " + error};
        if (action.targetMode == 3 && !receiving) return {true, "送信元へ送れるのは「〜を受け取ったら」の場合だけです。"};
        const std::string args = m[6];
        if (!args.empty()) {
            for (auto part : Split(args, "、")) {
                if (part.empty()) continue;
                std::smatch a;
                if (!Match(part, R"((.+?)を(.+?)として)", a) && !Match(part, R"((.+?)=(.+))", a)) return {true, "引数は「名前を値として」の形で書いてください: " + part};
                if (!GameEvents::IsValidName(a[1].str())) return {true, "引数名が不正です: " + a[1].str()};
                auto value = ValueText(a[2]);
                // A bare word is a literal here unless it is explicitly marked as a received value.
                if (!a[2].str().starts_with("受信した")) value.fromPayload = false;
                if (value.fromPayload && !receiving) return {true, "受信した値を使えるのは「〜を受け取ったら」の場合だけです。"};
                action.args.emplace_back(a[1], value);
            }
        }
        return {true, {}};
    }
    // Commands of the gameplay components (values: number literal or a received argument name).
    struct CommandPattern { const char *pattern; const char *command; std::vector<const char *> args; };
    static const std::vector<CommandPattern> commands = {
        {R"(体力を(.+?)(?:だけ)?減らす)", "Health.damage", {"amount"}},
        {R"((.+?)ダメージを受ける)", "Health.damage", {"amount"}},
        {R"(体力を(.+?)(?:だけ)?回復(?:する)?)", "Health.heal", {"amount"}},
        {R"(インベントリに(.+?)を(.+?)個?追加(?:する)?)", "Inventory.add", {"item", "amount"}},
        {R"(会話を開始(?:する)?)", "Dialogue.start", {}},
        {R"(生成する)", "Spawner.spawn", {}},
        {R"((?:復活地点に戻る|復活する))", "Respawner.respawn", {}},
    };
    for (const auto &c : commands) {
        if (!Match(text, c.pattern, m)) continue;
        action.kind = EventActionKind::ComponentCommand;
        action.command = c.command;
        for (size_t i = 0; i < c.args.size(); ++i) {
            auto value = ValueText(m[i + 1]);
            if (std::string(c.args[i]) == "item") value.fromPayload = false;
            if (value.fromPayload && !receiving) return {true, "「" + value.text + "」は数値ではありません（受信した引数は「〜を受け取ったら」の場合だけ使えます）。"};
            action.commandArgs.emplace_back(c.args[i], value);
        }
        return {true, {}};
    }
    return {};
}

EventParseResult Done(EventRequest request, const std::string &error) {
    EventParseResult result;
    result.recognized = true;
    result.success = error.empty();
    result.message = error.empty() ? DescribeEventRequest(request) : error;
    result.request = std::move(request);
    return result;
}
bool PositiveNumber(const std::string &text, double low, double high, double &out) {
    return ParseNumber(text, out) && out >= low && out <= high;
}
} // namespace

std::string NormalizeEventText(const std::string &text) {
    std::string out;
    std::string quote;
    for (size_t i = 0; i < text.size();) {
        if (!quote.empty()) {
            if (quote.size() == 1 && text[i] == '\\' && i + 1 < text.size()) { out += text.substr(i, 2); i += 2; continue; }
            if (text.compare(i, quote.size(), quote) == 0) { out += quote; i += quote.size(); quote.clear(); }
            else out += text[i++];
            continue;
        }
        if (text.compare(i, 3, "「") == 0) { quote = "」"; out += "「"; i += 3; continue; }
        if (text.compare(i, 3, "『") == 0) { quote = "』"; out += "『"; i += 3; continue; }
        if (text[i] == '"' || text[i] == '\'') { quote.assign(1, text[i]); out += text[i++]; continue; }
        const auto c = static_cast<unsigned char>(text[i]);
        // Full-width ASCII (U+FF01..U+FF5E) → ASCII; ideographic space (U+3000) removed.
        if (c == 0xEF && i + 2 < text.size()) {
            const auto b1 = static_cast<unsigned char>(text[i + 1]), b2 = static_cast<unsigned char>(text[i + 2]);
            const int code = ((c & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            if (code >= 0xFF01 && code <= 0xFF5E) { out += static_cast<char>(code - 0xFF01 + 0x21); i += 3; continue; }
        }
        if (c == 0xE3 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 && static_cast<unsigned char>(text[i + 2]) == 0x80) { i += 3; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { ++i; continue; }
        out += text[i++];
    }
    while (out.ends_with("。")) out.resize(out.size() - std::string("。").size());
    return out;
}

bool ParseEventLiteral(GameEvents::ArgType type, const std::string &raw, GameEvents::Value &out, std::string *error) {
    using GameEvents::ArgType;
    const std::string text = StripQuotes(raw);
    auto fail = [&](const std::string &message) { if (error) *error = message; return false; };
    double number = 0;
    switch (type) {
    case ArgType::Bool:
        if (text == "true" || text == "はい" || text == "オン") { out = true; return true; }
        if (text == "false" || text == "いいえ" || text == "オフ") { out = false; return true; }
        return fail("真偽値（true/false）ではありません: " + text);
    case ArgType::Int:
        if (!ParseNumber(text, number) || std::floor(number) != number || std::abs(number) > 2147483647.0) return fail("整数ではありません: " + text);
        out = static_cast<int>(number);
        return true;
    case ArgType::Number:
        if (!ParseNumber(text, number) || std::abs(number) > GameEvents::MaxMagnitude) return fail("数値ではありません: " + text);
        out = number;
        return true;
    case ArgType::String:
    case ArgType::Object: // the plan resolves an object name to a UUID
        out = text;
        return GameEvents::IsValidArgValue(ArgType::String, out, error);
    default: {
        std::string inner = text;
        if (inner.starts_with("(") && inner.ends_with(")")) inner = inner.substr(1, inner.size() - 2);
        Value list = Value::array();
        for (const auto &part : Split(inner, ",")) {
            if (!ParseNumber(part, number)) return fail("数値の並び（例: (0,1,0)）ではありません: " + text);
            list.push_back(number);
        }
        if (!GameEvents::IsValidArgValue(type, list, error)) return false;
        out = std::move(list);
        return true;
    }
    }
}

EventParseResult ParseEventRequest(const std::string &input) {
    const std::string text = NormalizeEventText(input);
    EventRequest request;
    std::smatch m;
    double a = 0, b = 0, c = 0;
    if (text.empty() || text.size() > 512) return {};

    //--- イベント定義 ---
    if (Match(text, R"((.+?)イベントを(?:作成|作る|定義)(?:する)?(?:、(.+))?)", m)) {
        request.kind = EventRequestKind::DefineEvent;
        request.definition.name = m[1];
        request.definition.shortTextManaged = true;
        request.definition.sourceText = input.size() <= 512 ? input : std::string{};
        if (m[2].matched) {
            for (const auto &clause : Split(m[2].str(), "、")) {
                std::smatch arg;
                if (!Match(clause, R"((.+?)は(省略可能な|任意の)?(数値|整数|文字列|真偽値|真偽|Vector2|Vector3|Vector4|色|オブジェクト)(?:型)?)", arg))
                    return Done(request, "引数は「名前は型」で書いてください（型: 数値/整数/文字列/真偽値/Vector2/Vector3/Vector4/色/オブジェクト）: " + clause);
                GameEvents::ArgDef def;
                def.name = arg[1];
                def.required = !arg[2].matched;
                const std::string type = arg[3];
                def.type = type == "数値" ? GameEvents::ArgType::Number : type == "整数" ? GameEvents::ArgType::Int : type == "文字列" ? GameEvents::ArgType::String :
                    (type == "真偽値" || type == "真偽") ? GameEvents::ArgType::Bool : type == "Vector2" ? GameEvents::ArgType::Vector2 :
                    type == "Vector3" ? GameEvents::ArgType::Vector3 : type == "Vector4" ? GameEvents::ArgType::Vector4 : type == "色" ? GameEvents::ArgType::Color : GameEvents::ArgType::Object;
                request.definition.args.push_back(std::move(def));
            }
        }
        std::string error;
        GameEvents::ValidateDefinition(request.definition, &error);
        return Done(request, error);
    }

    //--- 10カテゴリの部品 ---
    if (Match(text, R"((?:最大)?体力を)" + kNumber + R"(にして(?:、)?(.+?)に触れたら)" + kNumber + R"(ダメージ(?:を受ける)?)", m)) {
        request.kind = EventRequestKind::Health;
        if (!PositiveNumber(m[1], 0.001, 1.0e6, a) || !PositiveNumber(m[3], 0.001, 1.0e6, b)) return Done(request, "体力・ダメージは 0 より大きく 1000000 以下です。");
        request.params = {{"max", a}, {"otherTag", m[2].str()}, {"amount", b}};
        return Done(request, {});
    }
    if (Match(text, R"((?:最大)?体力を)" + kNumber + R"(にする)", m)) {
        request.kind = EventRequestKind::Health;
        if (!PositiveNumber(m[1], 0.001, 1.0e6, a)) return Done(request, "体力は 0 より大きく 1000000 以下です。");
        request.params = {{"max", a}};
        return Done(request, {});
    }
    if (Match(text, R"((.+?)に触れたら)" + kNumber + R"(ダメージを与える)", m)) {
        request.kind = EventRequestKind::ContactDamage;
        if (!PositiveNumber(m[2], 0.001, 1.0e6, a)) return Done(request, "ダメージは 0 より大きく 1000000 以下です。");
        request.params = {{"otherTag", m[1].str()}, {"amount", a}};
        return Done(request, {});
    }
    if (Match(text, R"((.+?)に触れたら(.+?)を)" + kNumber + R"((?:個)?(?:増やして|追加して|取得して)(消える|消す|無効化する|無効になる|残る))", m)) {
        request.kind = EventRequestKind::Pickup;
        if (!PositiveNumber(m[3], 1, 1000000, a) || std::floor(a) != a) return Done(request, "個数は 1 以上の整数です。");
        const std::string after = m[4];
        request.params = {{"recipientTag", m[1].str()}, {"item", m[2].str()}, {"amount", static_cast<int>(a)},
            {"after", after == "残る" ? 0 : (after == "無効化する" || after == "無効になる") ? 1 : 2}};
        return Done(request, {});
    }
    if (Match(text, R"((?:(?:生成|出現)されてから)?)" + kNumber + R"(秒後に(消える|削除する|削除される|無効化する|無効になる))", m)) {
        request.kind = EventRequestKind::Lifetime;
        if (!PositiveNumber(m[1], 0.01, 3600, a)) return Done(request, "寿命は 0.01〜3600 秒です。");
        const std::string after = m[2];
        request.params = {{"duration", a}, {"action", (after == "無効化する" || after == "無効になる") ? 1 : 2}};
        return Done(request, {});
    }
    if (Match(text, kNumber + R"(秒ごとに(.+?)を生成(?:する|し|して)?(?:、)?(?:最大)" + kNumber + R"(体(?:まで|にする)?)?)", m)) {
        request.kind = EventRequestKind::Spawner;
        if (!PositiveNumber(m[1], 0.05, 3600, a)) return Done(request, "間隔は 0.05〜3600 秒です。");
        b = 5;
        if (m[3].matched && (!PositiveNumber(m[3], 1, 256, b) || std::floor(b) != b)) return Done(request, "最大数は 1〜256 の整数です。");
        request.params = {{"interval", a}, {"template", m[2].str()}, {"maxAlive", static_cast<int>(b)}};
        return Done(request, {});
    }
    if (Match(text, R"((?:近づいて|近くで)(.+?)キーを押したら(.+))", m)) {
        request.kind = EventRequestKind::Interact;
        request.key = KeyText(m[1]);
        if (request.key.empty()) return Done(request, "キーは A〜Z / 0〜9 / Space / Enter で指定してください: " + m[1].str());
        const std::string act = m[2];
        std::smatch x;
        if (Match(act, R"((?:扉|ドア)?を?開(?:く|ける))", x)) request.params = {{"open", true}};
        else if (Match(act, R"(会話を開始(?:する)?)", x)) request.params = {{"dialogue", true}};
        else if (Match(act, R"((.+?)を再生(?:する)?)", x)) request.params = {{"clip", x[1].str()}};
        else if (Match(act, R"((.+?)(?:イベント)?を(?:発火|送る|送信)(?:する)?)", x)) {
            if (!GameEvents::IsValidName(x[1].str())) return Done(request, "イベント名が不正です: " + x[1].str());
            request.params = {{"event", x[1].str()}};
        } else return Done(request, "インタラクトの処理は「扉を開く」「Openを再生」「会話を開始」「Openedを発火」のいずれかで書いてください: " + act);
        return Done(request, {});
    }
    if (Match(text, R"(触れた(?:場所|チェックポイント)を復活地点にして(?:、)?(.+?)たら(?:復活地点に)?戻る)", m)) {
        request.kind = EventRequestKind::Respawn;
        const std::string condition = m[1];
        std::smatch y;
        if (condition == "落下し") request.params = {{"useFall", true}, {"fallY", -10.0}, {"onDeath", false}};
        else if (Match(condition, R"(Y(?:座標)?が)" + kNumber + R"((?:より下|未満)にな(?:っ|り))", y)) {
            if (!ParseNumber(y[1], a) || std::abs(a) > 1.0e6) return Done(request, "高さが不正です。");
            request.params = {{"useFall", true}, {"fallY", a}, {"onDeath", false}};
        } else if (condition == "倒れ" || condition == "体力が0になっ") request.params = {{"useFall", false}, {"fallY", -10.0}, {"onDeath", true}};
        else return Done(request, "復活の条件は「落下し」「Yが-20より下になっ」「倒れ」のいずれかで書いてください: " + condition + "たら");
        return Done(request, {});
    }
    if (Match(text, R"(インベントリを(?:追加|作成)(?:する)?(?:、)?(?:最大)" + kNumber + R"(種類(?:まで)?)?)", m)) {
        request.kind = EventRequestKind::Inventory;
        a = 32;
        if (m[1].matched && (!PositiveNumber(m[1], 1, 256, a) || std::floor(a) != a)) return Done(request, "種類数の上限は 1〜256 の整数です。");
        request.params = {{"capacity", static_cast<int>(a)}};
        return Done(request, {});
    }
    if (Match(text, R"((.+?)を取得し(?:て)?(?:、)?(.+?)を持っていたら(?:扉|ドア)を開(?:ける|く))", m)) {
        request.kind = EventRequestKind::KeyDoor;
        if (m[1] != m[2]) return Done(request, "取得するアイテムと必要なアイテムが異なります: " + m[1].str() + " / " + m[2].str());
        request.params = {{"item", m[1].str()}};
        return Done(request, {});
    }
    if (Match(text, R"((?:近づいて)?(?:(.+?)キーで)?話しかけたら文章を順番に表示する)", m)) {
        request.kind = EventRequestKind::Dialogue;
        request.key = m[1].matched ? KeyText(m[1]) : "E";
        if (request.key.empty()) return Done(request, "キーは A〜Z / 0〜9 / Space / Enter で指定してください: " + m[1].str());
        return Done(request, {});
    }
    if (Match(text, R"((?:速度)?(?:)" + kNumber + R"((?:以上|より速い)で)?移動中は(.+?)(?:、)?停止中は(.+?)を再生(?:する)?)", m)) {
        request.kind = EventRequestKind::AnimationState;
        a = 0.1;
        if (m[1].matched && !PositiveNumber(m[1], 0.0001, 1.0e6, a)) return Done(request, "速度のしきい値が不正です。");
        if (m[2] == m[3]) return Done(request, "移動中と停止中に同じ状態は指定できません。");
        request.params = {{"threshold", a}, {"moving", m[2].str()}, {"idle", m[3].str()}};
        return Done(request, {});
    }
    (void)c;

    //--- きっかけ＋動作 ---
    request.kind = EventRequestKind::Handler;
    std::string rest = text;
    if (Match(text, R"((.+?)(?:イベント)?を受け取ったら(?:、)?(.+))", m)) {
        request.trigger = EventTrigger::Receive;
        request.triggerEvent = m[1];
        rest = m[2];
    } else if (Match(text, R"((?:(.+?)が)?(.+?)に触れたら(?:、)?(.+))", m)) {
        request.trigger = EventTrigger::Contact;
        request.contactSource = m[1].matched ? m[1].str() : std::string{};
        request.contactOther = m[2];
        rest = m[3];
    } else if (Match(text, R"((.+?)キーを押したら(?:、)?(.+))", m)) {
        request.trigger = EventTrigger::Key;
        request.key = KeyText(m[1]);
        rest = m[2];
    }
    // A condition this grammar does not know (「Spaceを押したら」 etc.) belongs to the other grammars.
    if (request.trigger == EventTrigger::Start)
        for (const char *word : {"たら", "とき", "なら", "時に", "間"}) if (rest.find(word) != std::string::npos) return {};
    const auto parsed = ParseAction(rest, request.trigger == EventTrigger::Receive, request.action);
    if (!parsed.recognized) return {};
    if (!parsed.error.empty()) return Done(request, parsed.error);
    if (request.trigger == EventTrigger::Receive) {
        std::string error;
        if (!GameEvents::IsValidName(request.triggerEvent, &error)) return Done(request, "受け取るイベント名「" + request.triggerEvent + "」: " + error);
    }
    if (request.trigger == EventTrigger::Key && request.key.empty()) return Done(request, "キーは A〜Z / 0〜9 / Space / Enter で指定してください。");
    if (request.action.kind == EventActionKind::Tween && request.trigger != EventTrigger::Receive) return {};
    return Done(request, {});
}

std::string DescribeEventRequest(const EventRequest &r) {
    std::ostringstream out;
    auto values = [](const std::vector<std::pair<std::string, EventValueText>> &args) {
        std::string text;
        for (const auto &[name, value] : args) text += (text.empty() ? "" : ", ") + (name.empty() ? std::string("（唯一の引数）") : name) + "=" + (value.fromPayload ? "受信." : "") + value.text;
        return text;
    };
    switch (r.kind) {
    case EventRequestKind::DefineEvent: out << "イベント定義 " << GameEvents::DescribeDefinition(r.definition); break;
    case EventRequestKind::Handler:
        switch (r.trigger) {
        case EventTrigger::Start: out << "再生開始時に"; break;
        case EventTrigger::Receive: out << "「" << r.triggerEvent << "」を受け取ったら"; break;
        case EventTrigger::Contact: out << (r.contactSource.empty() ? std::string("選択中") : r.contactSource) << " が " << r.contactOther << " に触れたら"; break;
        case EventTrigger::Key: out << r.key << " キーで"; break;
        }
        switch (r.action.kind) {
        case EventActionKind::Tween: out << " キーフレームを再生: " << r.action.tweenText; break;
        case EventActionKind::CallScript: out << " スクリプト関数 " << r.action.function << (r.action.passEvent ? "(GameEvent@)" : "(" + [&] { std::string s; for (const auto &a : r.action.functionArgs) s += (s.empty() ? "" : ", ") + a; return s; }() + ")") << " を呼ぶ"; break;
        case EventActionKind::Emit: out << " イベント「" << r.action.event << "」を" << (r.action.targetMode == 0 ? "全体" : r.action.targetMode == 3 ? "送信元" : (r.action.targetMode == 2 ? "タグ " : "対象 ") + r.action.target) << "へ送る" << (r.action.args.empty() ? "" : "（" + values(r.action.args) + "）"); break;
        case EventActionKind::ComponentCommand: out << " " << r.action.command << (r.action.commandArgs.empty() ? "" : "（" + values(r.action.commandArgs) + "）"); break;
        }
        break;
    case EventRequestKind::Health: out << "Health（最大 " << r.params.value("max", 0.0) << "）" << (r.params.contains("otherTag") ? " ＋ ContactDamage（タグ " + r.params["otherTag"].get<std::string>() + " に触れたら " + GameEvents::DescribeValue(r.params["amount"]) + " ダメージ）" : ""); break;
    case EventRequestKind::ContactDamage: out << "ContactDamage（タグ " << r.params["otherTag"].get<std::string>() << " へ " << r.params["amount"] << " ダメージを与える）"; break;
    case EventRequestKind::Pickup: out << "Pickup（" << r.params["recipientTag"].get<std::string>() << " が取得 → " << r.params["item"].get<std::string>() << " +" << r.params["amount"] << "）"; break;
    case EventRequestKind::Lifetime: out << "Lifetime（" << r.params["duration"] << " 秒後に" << (r.params["action"] == 1 ? "無効化" : "削除") << "）"; break;
    case EventRequestKind::Spawner: out << "Spawner（" << r.params["interval"] << " 秒ごとに " << r.params["template"].get<std::string>() << "、最大 " << r.params["maxAlive"] << " 体）"; break;
    case EventRequestKind::Interact: out << "Interactable（" << r.key << " キー）: " << r.params.dump(); break;
    case EventRequestKind::Respawn: out << "Respawner: " << r.params.dump(); break;
    case EventRequestKind::Inventory: out << "Inventory（種類数の上限 " << r.params["capacity"] << "）"; break;
    case EventRequestKind::KeyDoor: out << "「" << r.params["item"].get<std::string>() << "」を持っていたら開く扉"; break;
    case EventRequestKind::Dialogue: out << "Dialogue ＋ Interactable（" << r.key << " キーで話しかける）"; break;
    case EventRequestKind::AnimationState: out << "AnimationStateSwitcher（移動中 " << r.params["moving"].get<std::string>() << " / 停止中 " << r.params["idle"].get<std::string>() << "）"; break;
    }
    return out.str();
}

const std::vector<std::string> &EventExampleSentences() {
    static const std::vector<std::string> examples = {
        "GoalReachedイベントを作成",
        "Damageイベントを作成、amountは数値",
        "PlayerがGoalに触れたらGoalReachedを発火",
        "GoalReachedを受け取ったら1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動",
        "Damageイベントを対象Enemyに、amountを10として送る",
        "GoalReachedを受け取ったらOnGoal関数を呼ぶ",
        "Damageを受け取ったら体力をamountだけ減らす",
        "体力を100にして、Enemyに触れたら10ダメージ",
        "Playerに触れたらコインを1増やして消える",
        "生成されてから3秒後に消える",
        "2秒ごとにEnemyを生成し、最大5体にする",
        "近づいてEキーを押したら扉を開く",
        "触れた場所を復活地点にして、落下したら戻る",
        "インベントリを追加",
        "鍵を取得し、鍵を持っていたら扉を開ける",
        "話しかけたら文章を順番に表示する",
        "移動中はWalk、停止中はIdleを再生する",
        "Damageイベントで対象Enemyに10を渡す",
    };
    return examples;
}
} // namespace KashipanEngine::ShortText
