#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Scene/Components/SceneComponentHeader.h"
#include "Objects/ComponentRef.h"
#include "Utilities/GameEvents.h"

namespace KashipanEngine {

class IObjectComponent;

/// @brief シーン単位のユーザー定義イベント（定義・送信キュー・購読）を管理するシーンコンポーネント
/// @details - 定義（名前＋型付き引数スキーマ＋説明）はシーンに保存される。名前は大文字小文字を区別せず一意。
///          - 送信先: シーン全体 / オブジェクトUUID / 一意な名前（送信時にUUIDへ解決。無い・重複は拒否）/ タグ（複数可）。
///          - Emit は定義に照らして引数を検証してからキューへ積む（不正な引数は送らない）。配送は
///            ProcessQueue（再生中の Update から呼ばれる）で行い、1回の処理件数・連鎖の深さ・キュー長を制限する。
///            同名イベントが同じ更新で複数届いても、1件ずつ別々に配送する。
///          - 購読は所有コンポーネントの ComponentRef（安定UUID＋追加ID）で保持し、配送の直前に毎回解決する。
///            削除済み・無効のコンポーネントへは配送しない（削除済みは自動で購読解除）。
///            配送時は購読一覧のIDを先に確定させ、ハンドラ内での購読解除・追加・削除でも反復子を壊さない。
///          - シーンごとに1つ。シーン切り替えでシーンと共に破棄されるため、古いシーンへは配送しない。
///          - 旧 event.emit（シーン変数 __shortTextEvent.NAME のカウンタ）とは独立。旧経路は、同名の定義がある場合に
///            限りこちらへも1回だけ送る（ルーター側からカウンタは変更しないため二重配送しない）。
class GameEventRouter final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(GameEventRouter, 1, SetUpdatePriority(-10);)
    COMPONENT_CATEGORY("Event")
    ~GameEventRouter() override = default;

    struct Limits {
        int maxQueue = 1024;     // 未配送で保持できる件数
        int maxPerUpdate = 256;  // 1回の ProcessQueue で配送するメッセージ数
        int maxDepth = 8;        // ハンドラ内の送信の連鎖の深さ
    };
    /// @brief 受信処理。receiver は所有コンポーネント（所有者なしの購読では nullptr）。呼び出し中だけ有効
    using Handler = std::function<void(IObjectComponent *receiver, const GameEvents::Message &message)>;

    std::unique_ptr<ISceneComponent> Clone() const override;

    /// @brief シーンのルーターを取得する（無ければ nullptr。自動では追加しない）
    static GameEventRouter *Find(SceneContext *scene);

    //--- 定義 ---
    const std::vector<GameEvents::Definition> &GetDefinitions() const noexcept { return definitions_; }
    const GameEvents::Definition *FindDefinition(const std::string &name) const { return GameEvents::Find(definitions_, name); }
    /// @brief 定義を追加する（名前の重複・不正なスキーマは拒否）
    bool AddDefinition(const GameEvents::Definition &definition, std::string *error = nullptr);
    /// @brief name の定義を置き換える（改名先が別の定義と重複する場合は拒否）
    bool ReplaceDefinition(const std::string &name, const GameEvents::Definition &definition, std::string *error = nullptr);
    bool RemoveDefinition(const std::string &name);
    /// @brief 定義一覧をまとめて置き換える（検証に失敗した場合は変更しない）
    bool SetDefinitions(const std::vector<GameEvents::Definition> &definitions, std::string *error = nullptr);
    /// @brief 別シーンの定義を取り込む（同名は同じスキーマなら無視、異なれば拒否して変更しない）
    bool ImportDefinitions(const GameEvents::Value &json, std::string *error = nullptr);

    //--- 送信 ---
    /// @brief 引数を検証してキューへ積む。ハンドラ内からの送信は連鎖の深さ+1として扱う
    /// @param dropUndeclared 定義に無い引数を無視する（部品が標準の項目を提供する場合）
    bool Emit(const std::string &name, const GameEvents::Value &payload, const std::string &sender,
        GameEvents::TargetMode target = GameEvents::TargetMode::Broadcast, const std::string &targetValue = "",
        std::string *error = nullptr, bool dropUndeclared = false);
    /// @brief キューを配送する（再生中の Update から呼ばれる。テストからも直接呼べる）。配送した件数を返す
    size_t ProcessQueue();
    size_t GetQueuedCount() const noexcept { return queue_.size(); }
    void ClearQueue() { queue_.clear(); }

    //--- 購読 ---
    /// @brief 購読する。owner が有効な場合はその生存・有効状態を配送ごとに確認する
    /// @param key 同じ owner・イベント・key の購読は置き換える（重複購読しない）
    std::uint64_t Subscribe(const std::string &eventName, const ComponentRef &owner, Handler handler, const std::string &key = "");
    bool Unsubscribe(std::uint64_t id);
    size_t UnsubscribeOwner(const ComponentRef &owner, const std::string &key = "");
    size_t GetSubscriptionCount() const noexcept { return subscriptions_.size(); }

    const Limits &GetLimits() const noexcept { return limits_; }
    bool SetLimits(const Limits &limits);
    /// @brief 直近の診断（不正な送信・連鎖の打ち切り・ハンドラの例外など）。最大32件
    const std::deque<std::string> &GetDiagnostics() const noexcept { return diagnostics_; }
    void AddDiagnostic(const std::string &message);
    std::uint64_t GetDeliveredCount() const noexcept { return delivered_; }
    std::uint64_t GetDroppedCount() const noexcept { return dropped_; }
    /// @brief 処理中のメッセージ（ハンドラ内のみ。それ以外は nullptr）
    const GameEvents::Message *GetCurrentMessage() const noexcept { return current_; }
    /// @brief 購読を保持している世代。ルーターの追加・置き換え・終了処理ごとに変わる（購読者は変化で購読し直す）
    std::uint64_t GetGeneration() const noexcept { return generation_; }

protected:
    void Update() override;
    void Finalize() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    struct Subscription {
        std::string event; // folded
        ComponentRef owner;
        std::string key;
        Handler handler;
    };
    bool Matches(const Subscription &subscription, const GameEvents::Message &message, IObjectComponent *&receiver, bool &stale) const;
    void Deliver(const GameEvents::Message &message);

    std::vector<GameEvents::Definition> definitions_;
    Limits limits_;
    // --- 実行時状態（保存されない） ---
    std::deque<GameEvents::Message> queue_;
    std::map<std::uint64_t, Subscription> subscriptions_;
    std::uint64_t nextSubscription_ = 1;
    std::uint64_t nextSerial_ = 1;
    std::uint64_t delivered_ = 0;
    std::uint64_t dropped_ = 0;
    const GameEvents::Message *current_ = nullptr;
    std::deque<std::string> diagnostics_;
    std::shared_ptr<int> alive_ = std::make_shared<int>(0);
    std::uint64_t generation_ = NextGeneration();
    static std::uint64_t NextGeneration();
#if defined(USE_IMGUI)
    std::string editError_;
    std::string testEvent_;
    std::string testPayload_ = "{}";
#endif
};

REGISTER_COMPONENT_SCENE(GameEventRouter)

} // namespace KashipanEngine
