#pragma once
#include <string>
#include <utility>
#include <vector>
#include "Utilities/GameEvents.h"

namespace KashipanEngine::ShortText {
// Bounded grammar for user-defined events and gameplay components (pure: no scene access).
// A sentence is either fully recognized (with a validated request or an explicit error) or not recognized at all,
// so nothing is ever applied partially. Names / values that need the scene (objects, schemas, clips) are kept as
// text here and resolved by the editor plan (Scene/Editor/ShortTextEventPlan), which rejects ambiguity.
enum class EventRequestKind {
    DefineEvent,    // 「GoalReachedイベントを作成」「Damageイベントを作成、amountは数値」
    Handler,        // [きっかけ]+[動作]（受け取ったら / 触れたら / キーを押したら / きっかけ無し=再生開始時）
    Health,         // 「体力を100にして、Enemyに触れたら10ダメージ」
    ContactDamage,  // 「Enemyに触れたら10ダメージを与える」
    Pickup,         // 「Playerに触れたらコインを1増やして消える」
    Lifetime,       // 「生成されてから3秒後に消える」
    Spawner,        // 「2秒ごとにEnemyを生成し、最大5体にする」
    Interact,       // 「近づいてEキーを押したら扉を開く」
    Respawn,        // 「触れた場所を復活地点にして、落下したら戻る」
    Inventory,      // 「インベントリを追加」
    KeyDoor,        // 「鍵を取得し、鍵を持っていたら扉を開ける」
    Dialogue,       // 「話しかけたら文章を順番に表示する」
    AnimationState, // 「移動中はWalk、停止中はIdleを再生する」
};
enum class EventTrigger { Start, Receive, Contact, Key };
enum class EventActionKind { Tween, CallScript, Emit, ComponentCommand };

// A value written in the sentence: a literal (number / text) or a received argument name (resolved against the schema).
struct EventValueText {
    std::string text;
    bool fromPayload = false; // 「amountだけ減らす」のように受信した引数を使う
};
struct EventActionRequest {
    EventActionKind kind = EventActionKind::Emit;
    std::string tweenText;                                     // Tween: property grammar text (「1秒かけて…」)
    std::string function;                                      // CallScript
    std::vector<std::string> functionArgs;                     // CallScript: received argument names, in order
    bool passEvent = false;                                    // CallScript: void 関数(GameEvent@)
    std::string event;                                         // Emit
    int targetMode = 0;                                        // Emit: 0 全体, 1 オブジェクト名, 2 タグ, 3 送信元
    std::string target;                                        // Emit
    std::vector<std::pair<std::string, EventValueText>> args;  // Emit: 引数名（"" = 唯一の引数）と値
    std::string command;                                       // ComponentCommand: 「型.命令」
    std::vector<std::pair<std::string, EventValueText>> commandArgs;
};
struct EventRequest {
    EventRequestKind kind = EventRequestKind::DefineEvent;
    GameEvents::Definition definition;                         // DefineEvent
    EventTrigger trigger = EventTrigger::Start;                // Handler
    std::string triggerEvent;                                  // Receive
    std::string contactSource;                                 // Contact: 「AがBに触れたら」の A（空 = 選択中）
    std::string contactOther;                                  // Contact: B（名前またはタグ）/ 各部品の相手
    std::string key;                                           // Key / Interact
    EventActionRequest action;                                 // Handler
    GameEvents::Value params = GameEvents::Value::object();    // gameplay components
};
struct EventParseResult {
    bool recognized = false;
    bool success = false;
    EventRequest request;
    std::string message;
};

// Normalizes width and spacing only (case is kept: event and object names are case-sensitive text).
std::string NormalizeEventText(const std::string &text);
EventParseResult ParseEventRequest(const std::string &text);
std::string DescribeEventRequest(const EventRequest &request);
// Parses a literal for a typed argument. Object values are names here; the plan resolves them to UUIDs.
bool ParseEventLiteral(GameEvents::ArgType type, const std::string &text, GameEvents::Value &out, std::string *error = nullptr);
// The fixed sample sentences shown in the editor and checked by the regressions.
const std::vector<std::string> &EventExampleSentences();
} // namespace KashipanEngine::ShortText
