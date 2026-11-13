#pragma once
#include <string>
#include <utility>
#include <vector>
#include "Utilities/GameEvents.h"

namespace KashipanEngine::ShortText {
// ユーザー定義イベントとゲームプレイ用コンポーネント向けの範囲を限定した文法（シーンへのアクセスなし）。
// 文は完全に認識する（検証済みリクエストまたは明示的なエラー）か、全く認識しないかのどちらかとし、
// 部分的な適用は行わない。シーンが必要な名前・値（オブジェクト・スキーマ・クリップ）はここでは
// 文字列のまま保持し、曖昧さを拒否するエディターの計画処理（Scene/Editor/ShortTextEventPlan）で解決する。
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

// 文中の値。リテラル（数値・テキスト）または受信引数名（スキーマと照合して解決）。
struct EventValueText {
    std::string text;
    bool fromPayload = false; // 「amountだけ減らす」のように受信した引数を使う
};
struct EventActionRequest {
    EventActionKind kind = EventActionKind::Emit;
    std::string tweenText;                                     // Tween: プロパティ文法の文章（「1秒かけて…」）
    std::string function;                                      // スクリプト呼び出し
    std::vector<std::string> functionArgs;                     // CallScript: 順序付きの受信引数名
    bool passEvent = false;                                    // CallScript: void 関数(GameEvent@)
    std::string event;                                         // 送信
    int targetMode = 0;                                        // Emit: 0 全体, 1 オブジェクト名, 2 タグ, 3 送信元
    std::string target;                                        // 送信
    std::vector<std::pair<std::string, EventValueText>> args;  // Emit: 引数名（"" = 唯一の引数）と値
    std::string command;                                       // ComponentCommand: 「型.命令」
    std::vector<std::pair<std::string, EventValueText>> commandArgs;
};
struct EventRequest {
    EventRequestKind kind = EventRequestKind::DefineEvent;
    GameEvents::Definition definition;                         // イベント定義
    EventTrigger trigger = EventTrigger::Start;                // ハンドラー
    std::string triggerEvent;                                  // 受信
    std::string contactSource;                                 // Contact: 「AがBに触れたら」の A（空 = 選択中）
    std::string contactOther;                                  // Contact: B（名前またはタグ）/ 各部品の相手
    std::string key;                                           // キー・インタラクション
    EventActionRequest action;                                 // ハンドラー
    GameEvents::Value params = GameEvents::Value::object();    // ゲームプレイ用コンポーネント
};
struct EventParseResult {
    bool recognized = false;
    bool success = false;
    EventRequest request;
    std::string message;
};

// 全角・半角と空白のみ正規化する。イベント名・オブジェクト名の大文字・小文字はそのまま保持する。
std::string NormalizeEventText(const std::string &text);
EventParseResult ParseEventRequest(const std::string &text);
std::string DescribeEventRequest(const EventRequest &request);
// 型付き引数のリテラルを解析する。オブジェクト値はここでは名前とし、計画側でUUIDへ解決する。
bool ParseEventLiteral(GameEvents::ArgType type, const std::string &text, GameEvents::Value &out, std::string *error = nullptr);
// エディターに表示し、回帰テストでも検証する固定の例文。
const std::vector<std::string> &EventExampleSentences();
} // namespace KashipanEngine::ShortText
