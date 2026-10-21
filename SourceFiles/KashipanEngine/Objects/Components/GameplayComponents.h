#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "Objects/ObjectComponentHeader.h"
#include "Input/Key.h"
#include "Math/Vector3.h"
#include "Utilities/GameEvents.h"

namespace KashipanEngine {

class EmptyObject;
class SceneContext;

//==================================================
// 共通
//==================================================

/// @brief イベントの受信処理（GameEventListener の「部品の命令」）から呼べる部品
/// @details 命令名と引数（型付き JSON）で操作する。引数は呼び出し中だけ有効なコピー。
class IGameEventCommandTarget {
public:
    virtual ~IGameEventCommandTarget() = default;
    /// @brief 命令を実行する（未知の命令・不正な引数は error に理由を入れて false）
    virtual bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) = 0;
    /// @brief 受け付ける命令と引数の説明（Inspector・短文の構成表示用）
    virtual std::vector<std::string> GetEventCommands() const = 0;
};

namespace Gameplay {
/// @brief 保存・読込の両方に使う項目表（読込時は範囲・型を検証し、無い項目は現在値のまま）
class Fields {
public:
    explicit Fields(JSON &out) : out_(&out) {}
    explicit Fields(const JSON &in) : in_(&in) {}
    bool Reading() const noexcept { return in_ != nullptr; }
    bool Float(const char *key, float &value, float low, float high);
    bool Int(const char *key, int &value, int low, int high);
    bool Bool(const char *key, bool &value);
    bool Text(const char *key, std::string &value, size_t maxBytes = 256);
    bool EventName(const char *key, std::string &value); // 空 または 正しいイベント名
    bool Vec3(const char *key, Vector3 &value);
    bool KeyCode(const char *key, Key &value);
    bool Raw(const char *key, JSON &value);
private:
    const JSON *Get(const char *key) const;
    JSON *out_ = nullptr;
    const JSON *in_ = nullptr;
};

bool IsPlaying(SceneContext *scene);
/// @brief 自分の Collider と重なっている相手（自分以外、重複なし、最大32件）。Collider/SceneObjectCollider が無ければ空
std::vector<EmptyObject *> QueryContacts(const EmptyObject *owner, SceneContext *scene);
/// @brief "" は nullptr。UUID 文字列、または一意なオブジェクト名で解決する（同名が複数なら失敗）
EmptyObject *ResolveObject(SceneContext *scene, const std::string &reference, std::string *error = nullptr);
Vector3 WorldPosition(EmptyObject *object);
void SetWorldPosition(EmptyObject *object, const Vector3 &world);
/// @brief 部品のイベント送信。名前が空なら何もしない（true）。定義に無い標準項目は送らない
bool EmitEvent(SceneContext *scene, const EmptyObject *sender, const std::string &eventName, const GameEvents::Value &payload, std::string *error);
std::string IdOf(const EmptyObject *object);
/// @brief 所有オブジェクトを削除または無効化する（0: 何もしない, 1: 無効化, 2: 削除）。削除後は呼び出し元のメンバへ触れないこと
void RemoveOwner(SceneContext *scene, EmptyObject *owner, int mode);
}

//==================================================
// 1. 体力・ダメージ・回復
//==================================================

/// @brief 体力。最大値・現在値（0〜最大）、無敵時間、死亡時の処理と、HP変化・ダメージ・回復・死亡イベント
/// @details 送るイベントの標準項目: amount(数値) / current(数値) / max(数値) / previous(数値) / sender(オブジェクト)。
///          イベント定義にある項目だけが送られる。死亡は1回だけ通知し、死亡中のダメージは無視する。
class Health final : public IObjectComponent, public IGameEventCommandTarget {
public:
    struct Settings {
        float max = 100.0f;
        float current = 100.0f;
        float invulnerability = 0.5f; // ダメージ後の無敵時間（秒）
        bool integerOnly = false;     // 量を整数に限定する
        int onDeath = 0;              // 0: 何もしない, 1: 無効化, 2: 削除
        std::string damagedEvent, healedEvent, changedEvent, deathEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Health, 0x1,
        AddMemberProperty<float>("max_", &settings_.max, [this](const float &v) { auto n = settings_; n.max = v; n.current = std::min(n.current, v); return SetSettings(n); });
        AddMemberProperty<float>("current_", &settings_.current, [this](const float &v) { return SetCurrent(v); });
        AddMemberProperty<float>("invulnerability_", &settings_.invulnerability, [this](const float &v) { auto n = settings_; n.invulnerability = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);

    bool Damage(float amount, const std::string &sender, std::string *error = nullptr);
    bool Heal(float amount, const std::string &sender, std::string *error = nullptr);
    bool SetCurrent(float value);
    void Revive();
    bool IsDead() const noexcept { return dead_; }
    float GetCurrent() const noexcept { return settings_.current; }
    float GetMax() const noexcept { return settings_.max; }
    float GetInvulnerableRemaining() const noexcept { return invulnerable_; }
    void Advance(double dt);
    const std::string &GetLastError() const noexcept { return lastError_; }

    bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) override;
    std::vector<std::string> GetEventCommands() const override;
    template <class F> static bool Bind(Settings &s, F &f);

protected:
    void Initialize() override;
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    bool CheckAmount(float amount, std::string *error) const;
    void Changed(float previous, float amount, const std::string &sender, const std::string &kindEvent);
    Settings settings_;
    float invulnerable_ = 0;
    bool dead_ = false;
    bool pendingRemoval_ = false; // 死亡時の無効化／削除は次の Update で行う
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(Health)

/// @brief 接触ダメージ。相手のタグで判定し、自分が受ける（Receive）か相手へ与える（Deal）
/// @details 相手ごとのクールダウンで、接触し続けても毎フレームのダメージにはならない。
class ContactDamage final : public IObjectComponent {
public:
    struct Settings {
        std::string otherTag = "Enemy";
        float amount = 10.0f;
        float cooldown = 0.5f;
        int mode = 0; // 0: 自分がダメージを受ける, 1: 相手へダメージを与える
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(ContactDamage, 0xFF,
        AddMemberProperty<float>("amount_", &settings_.amount, [this](const float &v) { auto n = settings_; n.amount = v; return SetSettings(n); });
        AddMemberProperty<float>("cooldown_", &settings_.cooldown, [this](const float &v) { auto n = settings_; n.cooldown = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    /// @brief 接触を調べてダメージを適用する。適用した回数を返す
    int Advance(double dt);
    /// @brief 1つの接触相手への適用（Advance から呼ばれる。テストからも呼べる）
    bool ApplyContact(EmptyObject *other);
    const std::string &GetLastError() const noexcept { return lastError_; }
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { cooldowns_.clear(); }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
    std::map<std::string, float> cooldowns_;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(ContactDamage)

//==================================================
// 2. アイテム取得 / 7. インベントリ
//==================================================

/// @brief アイテムID→個数。種類数の上限・1種類の上限、追加/削除/所持確認/消費、増減イベント、グローバル保存
/// @details 増減イベントの標準項目: item(文字列) / amount(整数, 増減量) / count(整数, 変化後) / sender(オブジェクト)。
class Inventory final : public IObjectComponent, public IGameEventCommandTarget {
public:
    struct Settings {
        std::map<std::string, int> items;
        int capacity = 32;  // 種類数の上限
        int maxStack = 999; // 1種類の上限
        std::string changedEvent;
        std::string saveKey; // グローバルシーン変数 __inventory.<saveKey> へ保存
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Inventory, 0x1, )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    static bool IsValidItemId(const std::string &item);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    int Count(const std::string &item) const;
    bool Has(const std::string &item, int amount = 1) const { return Count(item) >= amount; }
    /// @brief 追加（上限を超える場合は何も変えずに false）
    bool Add(const std::string &item, int amount, const std::string &sender = "", std::string *error = nullptr);
    /// @brief 削除・消費（不足している場合は何も変えずに false）
    bool Remove(const std::string &item, int amount, const std::string &sender = "", std::string *error = nullptr);
    void Clear();
    bool SaveToGlobal(std::string *error = nullptr) const;
    bool LoadFromGlobal(std::string *error = nullptr);
    bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) override;
    std::vector<std::string> GetEventCommands() const override;
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    void Notify(const std::string &item, int delta, const std::string &sender);
    Settings settings_;
#if defined(USE_IMGUI)
    std::string newItem_;
#endif
};
REGISTER_COMPONENT_OBJECT(Inventory)

/// @brief 拾えるアイテム。受け取り手（タグ）の Inventory へ追加、またはシーン変数のスコアへ加算し、一度だけ取得されて消える
/// @details 受け取り手に Inventory が無い場合は取得せず、必要な設定をエラーとして示す（別の Player を作らない）。
///          取得イベントの標準項目: item / amount / count / recipient(オブジェクト) / sender(オブジェクト)。
class Pickup final : public IObjectComponent {
public:
    struct Settings {
        std::string recipientTag = "Player";
        std::string itemId = "Coin";
        int amount = 1;
        std::string scoreVariable; // 空でなければ Inventory の代わりにシーン変数(int)へ加算
        bool once = true;
        int after = 2; // 0: 残す, 1: 無効化, 2: 削除
        std::string pickedEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Pickup, 0x1, )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    bool TryPickup(EmptyObject *recipient, std::string *error = nullptr);
    /// @brief 接触を調べて取得する。取得したら true（所有オブジェクトが削除されている可能性があるため、以後メンバへ触れないこと）
    bool Advance();
    bool IsPicked() const noexcept { return picked_; }
    const std::string &GetLastError() const noexcept { return lastError_; }
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { picked_ = false; lastError_.clear(); }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
    bool picked_ = false;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(Pickup)

//==================================================
// 3. 寿命・自動削除 / 4. 生成管理
//==================================================

/// @brief 生成・再有効化から指定秒数後に削除または無効化（プール返却）する。編集中は進まない
class Lifetime final : public IObjectComponent {
public:
    struct Settings {
        float duration = 3.0f;
        int action = 2; // 1: 無効化（プールへ返却）, 2: 削除
        std::string expiredEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Lifetime, 0x1,
        AddMemberProperty<float>("duration_", &settings_.duration, [this](const float &v) { auto n = settings_; n.duration = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    /// @brief 時間を進める。期限に達したら true（削除された場合、以後メンバへ触れないこと）。不正な dt は無視
    bool Advance(double dt);
    double GetElapsed() const noexcept { return elapsed_; }
    void ResetTimer() noexcept { elapsed_ = 0; expired_ = false; }
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { ResetTimer(); }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
    double elapsed_ = 0;
    bool expired_ = false;
};
REGISTER_COMPONENT_OBJECT(Lifetime)

/// @brief 一定間隔でテンプレートオブジェクトを複製して生成する。最大同時数・1更新の生成数・シーン全体の上限を守る
/// @details テンプレートはシーン内の既存オブジェクト（UUID または一意な名前。通常は無効化しておく）。
///          Spawner を含むテンプレート・自分自身・祖先は拒否する（自己複製による無限生成を防ぐ）。
///          生成物は UUID で管理し、無効化されたもの（Lifetime の無効化など）はプールとして再利用できる。
///          生成イベントの標準項目: spawned(オブジェクト) / count(整数) / sender(オブジェクト)。上限イベントは count / max。
class Spawner final : public IObjectComponent, public IGameEventCommandTarget {
public:
    struct Settings {
        std::string templateObject;
        float interval = 2.0f;
        int maxAlive = 5;
        int perUpdate = 1;
        Vector3 offset{0.0f, 0.0f, 0.0f};
        bool parentToSpawner = false;
        bool reusePool = true;
        bool spawnOnStart = false;
        std::string spawnedEvent, limitEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    static constexpr int kSceneObjectLimit = 4096;
    OBJECT_COMPONENT_CONSTRUCTOR(Spawner, 0xFF,
        AddMemberProperty<float>("interval_", &settings_.interval, [this](const float &v) { auto n = settings_; n.interval = v; return SetSettings(n); });
        AddMemberProperty<int>("maxAlive_", &settings_.maxAlive, [this](const int &v) { auto n = settings_; n.maxAlive = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    /// @brief 時間を進め、間隔ごとに生成する。生成した数を返す
    int Advance(double dt);
    /// @brief 1体生成（上限・テンプレート検証あり）。生成物を返す
    EmptyObject *SpawnOne(std::string *error = nullptr);
    int GetAliveCount();
    const std::vector<UUID128> &GetSpawned() const noexcept { return spawned_; }
    const std::string &GetLastError() const noexcept { return lastError_; }
    bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) override;
    std::vector<std::string> GetEventCommands() const override;
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override;
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    EmptyObject *ResolveTemplate(std::string *error);
    void Prune();
    Settings settings_;
    double timer_ = 0;
    bool started_ = false;
    bool limitNotified_ = false;
    std::vector<UUID128> spawned_;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(Spawner)

//==================================================
// 5. インタラクト
//==================================================

/// @brief 近くの相手（タグ）がキー／入力アクションを押したときに、クリップ再生・会話開始・イベント送信を行う
/// @details 必要アイテム（相手の Inventory）を指定すると、持っていない場合は拒否イベントを送る。
///          開く方法（クリップ・イベント）が未設定なら実行せず、必要な設定をエラーとして示す。
///          イベントの標準項目: interactor(オブジェクト) / item(文字列) / sender(オブジェクト)。
class Interactable final : public IObjectComponent {
public:
    struct Settings {
        std::string interactorTag = "Player";
        float distance = 2.0f;
        Key key = Key::E;
        std::string inputAction;  // InputCommand のアクション名（空なら key）
        std::string requiredItem;
        int requiredAmount = 1;
        bool consumeItem = false;
        std::string keyframeClip; // 自分の KeyFrameAnimator のアニメーション名
        std::string animatorClip; // 自分の Animator のクリップ名
        bool startDialogue = false; // 自分の Dialogue を開始／進める
        bool once = false;
        std::string interactEvent, deniedEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Interactable, 0xFF,
        AddMemberProperty<float>("distance_", &settings_.distance, [this](const float &v) { auto n = settings_; n.distance = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    bool HasAction() const;
    /// @brief 距離内で最も近い相手（タグ一致）
    EmptyObject *FindInteractor();
    /// @brief 1フレーム分の入力で判定する（Update は実際のキー／入力アクションを渡す）。実行したら true
    bool Advance(bool pressed);
    bool Interact(EmptyObject *interactor, std::string *error = nullptr);
    bool IsUsed() const noexcept { return used_; }
    const std::string &GetLastError() const noexcept { return lastError_; }
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { used_ = false; lastError_.clear(); }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
    bool used_ = false;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(Interactable)

//==================================================
// 6. チェックポイント・復活
//==================================================

/// @brief 復活。復活地点（ワールド座標・回転）を持ち、落下（Y が閾値未満）・死亡（Health）で戻る
/// @details 復活時は Velocity を 0 にし（設定時）、Health を最大へ戻す（設定時）。チェックポイントのタグに触れると地点を更新する。
///          再生開始時の位置が最初の復活地点。イベントの標準項目: reason(文字列 fall/death/command) / position(Vector3) / sender。
class Respawner final : public IObjectComponent, public IGameEventCommandTarget {
public:
    struct Settings {
        std::string checkpointTag = "Checkpoint";
        bool useFall = true;
        float fallY = -10.0f;
        bool onDeath = true;
        bool resetVelocity = true;
        bool restoreHealth = true;
        std::string respawnedEvent, checkpointEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Respawner, 0x1,
        AddMemberProperty<float>("fallY_", &settings_.fallY, [this](const float &v) { auto n = settings_; n.fallY = v; return SetSettings(n); });
    )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    /// @brief 復活地点を更新する（ワールド座標）。変化した場合 true
    bool SetRespawnPoint(const Vector3 &world, const Vector3 *rotate = nullptr, const std::string &sender = "");
    bool Respawn(const std::string &reason);
    /// @brief 接触・落下・死亡を調べる（Update が再生中に呼ぶ）。復活したら true
    bool Advance();
    bool HasPoint() const noexcept { return hasPoint_; }
    const Vector3 &GetPoint() const noexcept { return point_; }
    bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) override;
    std::vector<std::string> GetEventCommands() const override;
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { hasPoint_ = false; hasRotate_ = false; }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
    Vector3 point_{0.0f, 0.0f, 0.0f};
    Vector3 rotate_{0.0f, 0.0f, 0.0f};
    bool hasPoint_ = false;
    bool hasRotate_ = false;
};
REGISTER_COMPONENT_OBJECT(Respawner)

/// @brief チェックポイント側の設定。対象タグが触れると、その Respawner の復活地点を（自分の位置＋オフセット）へ更新する
class Checkpoint final : public IObjectComponent {
public:
    struct Settings {
        std::string playerTag = "Player";
        Vector3 offset{0.0f, 0.0f, 0.0f};
        std::string activatedEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Checkpoint, 0x1, )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    Vector3 GetRespawnWorldPosition();
    /// @brief 接触を調べて更新する。更新した相手の数を返す
    int Advance();
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    Settings settings_;
};
REGISTER_COMPONENT_OBJECT(Checkpoint)

//==================================================
// 8. 会話・選択肢
//==================================================

/// @brief 会話。行を順番に表示し、選択肢で分岐する。表示先は TextRenderer / BitmapTextRenderer
/// @details 文章は利用者が入力する（自動生成しない）。行が無い場合は開始できない。
///          イベントの標準項目: line(整数) / text(文字列) / choice(整数) / label(文字列) / sender(オブジェクト)。
class Dialogue final : public IObjectComponent, public IGameEventCommandTarget {
public:
    struct Choice { std::string label; int next = -1; };      // next: -1 で終了
    struct Line { std::string speaker; std::string text; std::vector<Choice> choices; int next = -2; }; // -2: 次の行, -1: 終了
    struct Settings {
        std::vector<Line> lines;
        std::string outputObject;  // "" = 自分
        Key advanceKey = Key::Enter;
        bool choiceKeys = true;    // 1〜4キーで選択
        std::string startedEvent, lineEvent, choiceEvent, endedEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(Dialogue, 0x1, )
    COMPONENT_CATEGORY("Gameplay")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    bool Start(std::string *error = nullptr);
    /// @brief 次の行へ（選択肢のある行では進まない）
    bool Advance(std::string *error = nullptr);
    bool Choose(int index, std::string *error = nullptr);
    void End();
    bool IsRunning() const noexcept { return running_; }
    int GetLineIndex() const noexcept { return index_; }
    std::string GetDisplayText() const;
    /// @brief 1フレーム分の入力（Update は実際のキーを渡す）
    void HandleInput(bool advancePressed, int choicePressed);
    const std::string &GetLastError() const noexcept { return lastError_; }
    bool ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) override;
    std::vector<std::string> GetEventCommands() const override;
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { running_ = false; index_ = -1; }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    bool Show(int index);
    void Output(const std::string &text);
    Settings settings_;
    bool running_ = false;
    int index_ = -1;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(Dialogue)

//==================================================
// 9. アニメーション状態切り替え
//==================================================

/// @brief 移動速度の条件から状態（Idle/Walk/Run など）を選び、状態が変わったときだけクリップを再生する
/// @details 速度は Velocity があればその大きさ、無ければ Transform の位置変化から求める。
///          条件に合う状態のうち優先度が最も高いものを選ぶ。同じ状態の間は再生し直さない。
///          再生先は自分の KeyFrameAnimator（アニメーション名）または Animator（クリップ名）。存在しない名前は推測せずエラーにする。
///          遷移イベントの標準項目: from(文字列) / to(文字列) / speed(数値) / sender(オブジェクト)。
class AnimationStateSwitcher final : public IObjectComponent {
public:
    struct State {
        std::string name;
        float minSpeed = 0.0f;
        float maxSpeed = -1.0f; // -1: 上限なし
        int priority = 0;
        std::string clip;
        int player = 0;         // 0: KeyFrameAnimator, 1: Animator
    };
    struct Settings {
        std::vector<State> states;
        std::string transitionEvent;
        bool shortTextManaged = false;
        std::string sourceText;
    };
    OBJECT_COMPONENT_CONSTRUCTOR(AnimationStateSwitcher, 0x1, )
    COMPONENT_CATEGORY("Animation")
    std::unique_ptr<IObjectComponent> Clone() const override;
    static bool IsValid(const Settings &settings, std::string *error = nullptr);
    const Settings &GetSettings() const noexcept { return settings_; }
    bool SetSettings(const Settings &settings);
    /// @brief 速度から状態を選んで適用する。状態が変わったら true
    bool Evaluate(float speed);
    /// @brief 時間を進めて速度を求め、Evaluate する
    bool Advance(double dt);
    const std::string &GetCurrentState() const noexcept { return current_; }
    int GetPlayCount() const noexcept { return playCount_; }
    const std::string &GetLastError() const noexcept { return lastError_; }
    template <class F> static bool Bind(Settings &s, F &f);
protected:
    void Initialize() override { current_.clear(); hasLast_ = false; }
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    bool Play(const State &state, std::string &error);
    Settings settings_;
    std::string current_;
    Vector3 last_{0.0f, 0.0f, 0.0f};
    bool hasLast_ = false;
    int playCount_ = 0;
    std::string lastError_;
};
REGISTER_COMPONENT_OBJECT(AnimationStateSwitcher)

/// @brief 短文由来かどうか（設定の shortTextManaged）。対象外の型は false
bool IsShortTextManagedGameplay(const IObjectComponent *component);

} // namespace KashipanEngine
