#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "Objects/ObjectComponentHeader.h"
#include "Input/Key.h"
#include "Utilities/GameEvents.h"
#include "Utilities/ShortTextProgram.h"

namespace KashipanEngine {

/// @brief きっかけ（イベント受信・再生開始・接触・キー）ごとに、条件を満たしたら動作を順に実行するコンポーネント
/// @details 動作は「公開プロパティの設定」「既存の実行部品（keyframe.play / animator.play など）」「別イベントの送信」
///          「ScriptComponent の関数呼び出し」「部品の命令（Health.damage など）」。各引数は固定値・受信した引数・送信元から選べる。
///          イベントの購読は GameEventRouter に ComponentRef で登録し、配送の直前に生存・有効を確認する。
///          受信処理の中で自分や対象が削除・無効化・再購読されても、ルーターが配送対象を確定済みのため安全。
///          短文から作られた受信処理は shortTextManaged が true になり、同じ文の再適用ではその処理だけを更新する。
class GameEventListener final : public IObjectComponent {
public:
    enum class Trigger { Event, Start, Contact, Key };
    enum class ActionKind { SetProperty, EngineCommand, Emit, CallScript, ComponentCommand };
    enum class SourceKind { Fixed, Payload, Sender, EventName };
    struct ValueSource {
        SourceKind kind = SourceKind::Fixed;
        GameEvents::Value value;  // Fixed
        std::string arg;          // Payload
    };
    struct Condition {
        std::string arg;
        std::string op = "==";    // == != < <= > >=
        GameEvents::Value value;
    };
    struct Action {
        ActionKind kind = ActionKind::EngineCommand;
        std::string target;       // "" = 自分 / 一意な名前 / UUID / "$sender"
        std::string name;         // プロパティパス / 命令 / イベント名 / 関数名 / "型.命令"
        std::vector<std::pair<std::string, ValueSource>> args;
        GameEvents::TargetMode emitTarget = GameEvents::TargetMode::Broadcast; // Emit
        std::string emitTargetValue;                                            // Emit（"$sender" 可）
        bool passEvent = false;   // CallScript: void 関数(GameEvent@) として呼ぶ
    };
    struct Handler {
        std::string id;           // 一意な識別子（短文由来の更新に使う）
        Trigger trigger = Trigger::Event;
        std::string event;        // Event
        std::string contactTag;   // Contact（空欄は名前で判定）
        std::string contactName;  // Contact
        Key key = Key::E;         // Key
        std::string senderTag;    // Event: 送信元のタグ条件（空欄は全て）
        std::vector<Condition> conditions;
        bool once = false;
        float cooldown = 0.0f;
        std::vector<Action> actions;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    struct Settings { std::vector<Handler> handlers; };
    static constexpr size_t kMaxHandlers = 32;
    static constexpr size_t kMaxActions = 8;

    OBJECT_COMPONENT_CONSTRUCTOR(GameEventListener, 0x1, )
    COMPONENT_CATEGORY("Event")
    ~GameEventListener() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    const Handler *FindHandler(const std::string &id) const;

    /// @brief 実行部品のうち、毎フレームの継続実行が前提でイベントの1回実行には使えない命令か
    static bool IsContinuousCommand(const std::string &name);
    /// @brief 受信処理を実行する（条件・クールダウン・一度だけを確認）。実行したら true
    bool Fire(size_t handlerIndex, const GameEvents::Message &message);
    /// @brief キー入力（Update は実際のキーボードを渡す。テストからも呼べる）
    void NotifyKey(Key key);
    /// @brief 再生中の処理（購読・開始・接触・クールダウン）。Update から呼ばれる
    void Advance(double dt);
    /// @brief ルーターへの購読を作り直す
    void Resubscribe();
    const std::string &GetLastError() const noexcept { return lastError_; }
    int GetFireCount(size_t handlerIndex) const;

    static JSON EncodeSettings(const Settings &settings);
    static bool DecodeSettings(const JSON &json, Settings &out, std::string *error = nullptr);
    static JSON EncodeHandler(const Handler &handler);
    static bool DecodeHandler(const JSON &json, Handler &out, std::string *error = nullptr);
    static std::string DescribeHandler(const Handler &handler);

protected:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    bool RunAction(size_t handlerIndex, const Action &action, const GameEvents::Message &message, std::string &error);
    bool Resolve(const ValueSource &source, const GameEvents::Message &message, GameEvents::Value &out, std::string &error) const;
    void ResetRuntime();
    Settings settings_;
    // --- 実行時状態（保存されない） ---
    std::uint64_t revision_ = 1;
    std::uint64_t subscribedRevision_ = 0;
    std::uint64_t subscribedGeneration_ = 0; // GameEventRouter::GetGeneration of the current subscriptions
    bool started_ = false;
    std::vector<int> fired_;
    std::vector<float> cooldowns_;
    std::vector<std::vector<std::string>> contacts_;
    // Per-handler command memory (results / pools of 実行部品). Never shared between handlers or listeners.
    std::vector<ShortText::ProgramMemory> memories_;
    double lastDt_ = 0;
    std::string lastError_;
};

REGISTER_COMPONENT_OBJECT(GameEventListener)

} // namespace KashipanEngine
