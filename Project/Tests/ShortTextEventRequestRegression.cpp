// Pure regression: event schemas / typed payloads (GameEvents) and the short-text event grammar.
// No scene, window, GPU, audio, network or model. Built by RunShortTextEventRequestRegression.ps1.
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include "Utilities/GameEvents.h"
#include "Utilities/ShortTextEventRequest.h"

using namespace KashipanEngine;
using GameEvents::Value;
namespace {
int failures = 0;
void Check(bool value, const std::string &message) {
    if (!value) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
ShortText::EventParseResult Ok(const std::string &text) {
    auto result = ShortText::ParseEventRequest(text);
    Check(result.recognized && result.success, "accept: " + text + " / " + result.message);
    return result;
}
void Rejected(const std::string &text) {
    const auto result = ShortText::ParseEventRequest(text);
    Check(result.recognized && !result.success && !result.message.empty(), "reject with message: " + text);
}
void Ignored(const std::string &text) {
    Check(!ShortText::ParseEventRequest(text).recognized, "not claimed: " + text);
}
GameEvents::Definition Damage() {
    GameEvents::Definition d;
    d.name = "Damage";
    d.args.push_back({"amount", GameEvents::ArgType::Number, true, nullptr});
    d.args.push_back({"kind", GameEvents::ArgType::String, false, "normal"});
    d.args.push_back({"count", GameEvents::ArgType::Int, false, nullptr});
    d.args.push_back({"target", GameEvents::ArgType::Object, false, nullptr});
    d.args.push_back({"dir", GameEvents::ArgType::Vector3, false, nullptr});
    d.args.push_back({"tint", GameEvents::ArgType::Color, false, nullptr});
    d.args.push_back({"flag", GameEvents::ArgType::Bool, false, nullptr});
    return d;
}
}

int main() {
    //--- names ---
    Check(GameEvents::IsValidName("GoalReached") && GameEvents::IsValidName("ゴール到達"), "valid names");
    for (const char *bad : {"", "a b", "a.b", "a:b", "x\x01", "a\xE3\x80\x80" "b", "\xE3\x81"}) Check(!GameEvents::IsValidName(bad), std::string("invalid name rejected: ") + bad);
    Check(!GameEvents::IsValidName(std::string(65, 'a')) && GameEvents::IsValidName(std::string(64, 'a')), "name length limit 64 bytes");
    Check(GameEvents::SameName("GoalReached", "goalreached") && !GameEvents::SameName("Goal", "Goal2"), "case-insensitive name equality");

    //--- schemas ---
    auto d = Damage();
    Check(GameEvents::ValidateDefinition(d), "damage definition valid");
    auto dup = d; dup.args.push_back({"AMOUNT", GameEvents::ArgType::Number, false, nullptr});
    Check(!GameEvents::ValidateDefinition(dup), "duplicate argument names (case-insensitive) rejected");
    auto badDefault = d; badDefault.args[1].defaultValue = 3;
    Check(!GameEvents::ValidateDefinition(badDefault), "default must match the type");
    auto requiredDefault = d; requiredDefault.args[0].defaultValue = 1.0;
    Check(!GameEvents::ValidateDefinition(requiredDefault), "required argument cannot have a default");
    {
        std::vector<GameEvents::Definition> list{d, d};
        list[1].name = "DAMAGE";
        std::vector<GameEvents::Definition> out{d};
        Check(!GameEvents::DecodeDefinitions(GameEvents::EncodeDefinitions(list), out) && out.size() == 1, "duplicate definitions rejected, output unchanged");
        std::vector<GameEvents::Definition> single;
        Check(GameEvents::DecodeDefinitions(GameEvents::EncodeDefinitions({d}), single) && GameEvents::EncodeDefinitions(single) == GameEvents::EncodeDefinitions({d}), "definition JSON round trip");
        Check(!GameEvents::DecodeDefinitions(Value::parse(R"([{"name":"E","args":[{"name":"a","type":"pointer"}]}])"), single), "unknown argument type rejected");
    }

    //--- typed payloads ---
    Value out;
    std::string error;
    Check(GameEvents::BuildPayload(d, Value{{"amount", 10}}, out) && out["amount"] == 10 && out["kind"] == "normal" && !out.contains("count"), "defaults filled, absent optional stays absent");
    Check(!GameEvents::BuildPayload(d, Value::object(), out, &error) && error.find("amount") != std::string::npos, "missing required argument rejected");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", "10"}}, out), "string for number rejected");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"count", 1.5}}, out), "fraction for int rejected");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"unknown", 1}}, out), "undeclared argument rejected");
    Check(GameEvents::BuildPayload(d, Value{{"amount", 1}, {"unknown", 1}, {"count", 2.0}}, out, nullptr, true) && !out.contains("unknown") && out["count"].is_number_integer(), "component mode drops undeclared and converts integral numbers");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"target", "Enemy"}}, out), "object argument requires a UUID");
    Check(GameEvents::BuildPayload(d, Value{{"amount", 1}, {"target", "01234567-89ab-cdef-0123-456789abcdef"}}, out), "object UUID accepted");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"dir", {1, 2}}}, out), "vector size checked");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"tint", {1, 2, 0, 1}}}, out), "color range checked");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"flag", 1}}, out), "number for bool rejected");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"count", 18446744073709551615ULL}}, out), "unsigned overflow cannot wrap into int");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"kind", std::string("\xC3", 1)}}, out), "invalid UTF-8 payload is rejected without throwing");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1e300}}, out), "huge number rejected");
    Check(!GameEvents::BuildPayload(d, Value{{"amount", 1}, {"kind", std::string(2000, 'x')}}, out), "long string rejected");
    {
        GameEvents::Definition big; big.name = "Big";
        for (int i = 0; i < 5; ++i) big.args.push_back({"s" + std::to_string(i), GameEvents::ArgType::String, false, nullptr});
        Value input = Value::object();
        for (int i = 0; i < 5; ++i) input["s" + std::to_string(i)] = std::string(1000, 'x');
        Check(!GameEvents::BuildPayload(big, input, out, &error), "payload size limit 4096 bytes");
    }
    {
        GameEvents::Message message;
        message.payload = Value{{"amount", 10}, {"ratio", 0.5}, {"name", "x"}, {"v", {1, 2, 3}}};
        int i = 0; double n = 0; std::string s; float v[3] = {};
        Check(message.GetInt("amount", i) && i == 10 && message.GetNumber("ratio", n) && n == 0.5 && message.GetString("name", s) && s == "x" &&
            message.GetVector("v", v, 3) && v[2] == 3 && !message.GetInt("ratio", i) && !message.GetVector("v", v, 2), "typed getters");
    }

    //--- literals ---
    Check(ShortText::ParseEventLiteral(GameEvents::ArgType::Number, "10", out) && out == 10.0, "number literal");
    Check(ShortText::ParseEventLiteral(GameEvents::ArgType::Number, "2.5f", out) && out == 2.5, "float literal with f");
    Check(!ShortText::ParseEventLiteral(GameEvents::ArgType::Int, "2.5", out), "int literal rejects fraction");
    Check(ShortText::ParseEventLiteral(GameEvents::ArgType::Vector3, "(0,1,2)", out) && out == Value({0.0, 1.0, 2.0}), "vector literal");
    Check(ShortText::ParseEventLiteral(GameEvents::ArgType::Bool, "はい", out) && out == true, "bool literal");
    Check(ShortText::ParseEventLiteral(GameEvents::ArgType::String, "「こんにちは」", out) && out == "こんにちは", "quoted string literal");
    {
        const std::string huge = "1" + std::string(400, '0');
        bool threw = false;
        try {
            Check(!ShortText::ParseEventLiteral(GameEvents::ArgType::Number, huge, out) && !ShortText::ParseEventLiteral(GameEvents::ArgType::Int, huge, out), "huge literal rejected");
            const auto r = ShortText::ParseEventRequest("体力を" + huge + "にする");
            Check(r.recognized && !r.success, "huge number in a sentence rejected with a message");
            const auto s = ShortText::ParseEventRequest(huge + "秒ごとにEnemyを生成し、最大5体にする");
            Check(!s.success, "huge interval rejected");
        } catch (...) { threw = true; }
        Check(!threw, "no exception escapes the grammar");
    }

    //--- required sentences ---
    {
        auto r = Ok("GoalReachedイベントを作成");
        Check(r.request.kind == ShortText::EventRequestKind::DefineEvent && r.request.definition.name == "GoalReached" && r.request.definition.args.empty() && r.request.definition.shortTextManaged, "define GoalReached");
        r = Ok("Damageイベントを作成、amountは数値");
        Check(r.request.definition.name == "Damage" && r.request.definition.args.size() == 1 && r.request.definition.args[0].type == GameEvents::ArgType::Number && r.request.definition.args[0].required, "define Damage(amount number)");
        r = Ok("Ｈｉｔイベントを作成、ｐｏｗｅｒは整数、ｍｓｇは省略可能な文字列、ｄｉｒはVector3");
        Check(r.request.definition.name == "Hit" && r.request.definition.args.size() == 3 && r.request.definition.args[0].type == GameEvents::ArgType::Int &&
            !r.request.definition.args[1].required && r.request.definition.args[2].type == GameEvents::ArgType::Vector3, "full-width normalized, several typed args");
        Rejected("Damageイベントを作成、amountはポインタ");
        Rejected("Damage.Xイベントを作成");
        Rejected("Damageイベントを作成、amountは数値、amountは整数");

        r = Ok("PlayerがGoalに触れたらGoalReachedを発火");
        Check(r.request.kind == ShortText::EventRequestKind::Handler && r.request.trigger == ShortText::EventTrigger::Contact && r.request.contactSource == "Player" &&
            r.request.contactOther == "Goal" && r.request.action.kind == ShortText::EventActionKind::Emit && r.request.action.event == "GoalReached" && r.request.action.targetMode == 0, "contact emit");
        r = Ok("GoalReachedを受け取ったら1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動");
        Check(r.request.trigger == ShortText::EventTrigger::Receive && r.request.triggerEvent == "GoalReached" && r.request.action.kind == ShortText::EventActionKind::Tween &&
            r.request.action.tweenText == "1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動", "receive tween");
        r = Ok("Damageイベントを対象Enemyに、amountを10として送る");
        Check(r.request.trigger == ShortText::EventTrigger::Start && r.request.action.event == "Damage" && r.request.action.targetMode == 1 && r.request.action.target == "Enemy" &&
            r.request.action.args.size() == 1 && r.request.action.args[0].first == "amount" && r.request.action.args[0].second.text == "10" && !r.request.action.args[0].second.fromPayload, "send with typed argument");
        r = Ok("Damageイベントを対象Enemyに、amountを25として送る");
        Check(r.request.action.args[0].second.text == "25", "numbers are grammar values");
        r = Ok("GoalReachedを受け取ったらOnGoal関数を呼ぶ");
        Check(r.request.action.kind == ShortText::EventActionKind::CallScript && r.request.action.function == "OnGoal" && !r.request.action.passEvent, "receive call script");
        r = Ok("Damageを受け取ったらOnDamage関数をamountを渡して呼ぶ");
        Check(r.request.action.functionArgs == std::vector<std::string>{"amount"}, "call script with payload argument");
        r = Ok("Damageを受け取ったらOnDamage関数にイベントを渡して呼ぶ");
        Check(r.request.action.passEvent, "call script with GameEvent@");
        r = Ok("Damageを受け取ったら体力をamountだけ減らす");
        Check(r.request.action.kind == ShortText::EventActionKind::ComponentCommand && r.request.action.command == "Health.damage" && r.request.action.commandArgs[0].second.fromPayload &&
            r.request.action.commandArgs[0].second.text == "amount", "payload-driven health damage");
        r = Ok("Damageイベントで対象Enemyに10を渡す");
        Check(r.request.action.kind == ShortText::EventActionKind::Emit && r.request.action.args.size() == 1 && r.request.action.args[0].first.empty(), "only-argument send");
        r = Ok("Fキーを押したらGoalReachedを発火");
        Check(r.request.trigger == ShortText::EventTrigger::Key && r.request.key == "F", "key trigger");
        r = Ok("Pingを受け取ったらPongを送信元に送る");
        Check(r.request.action.targetMode == 3, "reply to sender");
        Rejected("GoalReachedを受け取ったらOn-Goal関数を呼ぶ");
        Ignored("GoalReachedを発火したらOnGoal関数を呼ぶ"); // unknown condition: left to the other grammars, never partially applied
        Rejected("Damageイベントを対象Enemyに、amountは10で送る");
        Rejected("体力をamountだけ減らす");
        Rejected("Pongを送信元に送る");
    }

    //--- the 10 gameplay categories ---
    {
        auto r = Ok("体力を100にして、Enemyに触れたら10ダメージ");
        Check(r.request.kind == ShortText::EventRequestKind::Health && r.request.params["max"] == 100.0 && r.request.params["otherTag"] == "Enemy" && r.request.params["amount"] == 10.0, "1 health");
        Rejected("体力を0にして、Enemyに触れたら10ダメージ");
        Rejected("体力を100にして、Enemyに触れたら-5ダメージ");
        r = Ok("Playerに触れたらコインを1増やして消える");
        Check(r.request.kind == ShortText::EventRequestKind::Pickup && r.request.params["recipientTag"] == "Player" && r.request.params["item"] == "コイン" && r.request.params["amount"] == 1 && r.request.params["after"] == 2, "2 pickup");
        Rejected("Playerに触れたらコインを1.5増やして消える");
        r = Ok("生成されてから3秒後に消える");
        Check(r.request.kind == ShortText::EventRequestKind::Lifetime && r.request.params["duration"] == 3.0 && r.request.params["action"] == 2, "3 lifetime");
        Check(Ok("5秒後に無効化する").request.params["action"] == 1, "3 lifetime pool");
        Rejected("生成されてから0秒後に消える");
        r = Ok("2秒ごとにEnemyを生成し、最大5体にする");
        Check(r.request.kind == ShortText::EventRequestKind::Spawner && r.request.params["interval"] == 2.0 && r.request.params["template"] == "Enemy" && r.request.params["maxAlive"] == 5, "4 spawner");
        Rejected("2秒ごとにEnemyを生成し、最大0体にする");
        Rejected("0.01秒ごとにEnemyを生成し、最大5体にする");
        r = Ok("近づいてEキーを押したら扉を開く");
        Check(r.request.kind == ShortText::EventRequestKind::Interact && r.request.key == "E" && r.request.params["open"] == true, "5 interact");
        Check(Ok("近づいてFキーを押したらOpenを再生する").request.params["clip"] == "Open", "5 interact clip");
        Rejected("近づいてEキーを押したら空を飛ぶ");
        r = Ok("触れた場所を復活地点にして、落下したら戻る");
        Check(r.request.kind == ShortText::EventRequestKind::Respawn && r.request.params["useFall"] == true && r.request.params["fallY"] == -10.0, "6 respawn");
        Check(Ok("触れた場所を復活地点にして、Yが-20より下になったら戻る").request.params["fallY"] == -20.0, "6 respawn threshold");
        r = Ok("インベントリを追加");
        Check(r.request.kind == ShortText::EventRequestKind::Inventory && r.request.params["capacity"] == 32, "7 inventory");
        r = Ok("鍵を取得し、鍵を持っていたら扉を開ける");
        Check(r.request.kind == ShortText::EventRequestKind::KeyDoor && r.request.params["item"] == "鍵", "7 key door");
        Rejected("鍵を取得し、剣を持っていたら扉を開ける");
        r = Ok("話しかけたら文章を順番に表示する");
        Check(r.request.kind == ShortText::EventRequestKind::Dialogue && r.request.key == "E", "8 dialogue");
        r = Ok("移動中はWalk、停止中はIdleを再生する");
        Check(r.request.kind == ShortText::EventRequestKind::AnimationState && r.request.params["moving"] == "Walk" && r.request.params["idle"] == "Idle", "9 animation state");
        Rejected("移動中はIdle、停止中はIdleを再生する");
        r = Ok("Damageイベントを対象Enemyに、amountを10として送る");
        Check(r.request.action.args.size() == 1, "10 typed argument event");
    }
    for (const auto &example : ShortText::EventExampleSentences()) Ok(example);
    {
        const auto r = Ok("Greetingイベントを全体に、textを「Hello world、Ｆｏｏ　bar」として送る");
        Check(r.request.action.args.size() == 1, "quoted comma does not split arguments");
        Value literal;
        Check(ShortText::ParseEventLiteral(GameEvents::ArgType::String, r.request.action.args[0].second.text, literal) &&
            literal == "Hello world、Ｆｏｏ　bar", "quoted string preserves spaces and width");
    }

    //--- unrelated / legacy sentences are not claimed ---
    for (const char *text : {"上下にふわふわ動かして", "Playerに触れたらscoreを1増やす", "Eキーを押したらXYZすべてのスケールを2に設定する",
        "1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動", "Transformの移動X軸を-1.0fから1.0fの間で往復移動", "こんにちは"}) Ignored(text);
    {
        std::ifstream file(SHORT_TEXT_DEFAULTS_PATH);
        Check(static_cast<bool>(file), "defaults dictionary readable");
        if (file) {
            const auto json = Value::parse(file);
            size_t count = 0;
            for (const auto &entry : json["entries"]) {
                ++count;
                const auto phrase = entry.value("phrase", std::string{});
                if (ShortText::ParseEventRequest(phrase).recognized) Check(false, "standard phrase claimed by the event grammar: " + phrase);
            }
            Check(count == 486, "486 standard phrases checked (" + std::to_string(count) + ")");
        }
    }

    if (failures) { std::printf("ShortTextEventRequestRegression: %d failure(s)\n", failures); return 1; }
    std::printf("ShortTextEventRequestRegression: PASS\n");
    return 0;
}
