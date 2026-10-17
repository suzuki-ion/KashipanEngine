// Gameplay components 1-4 and 7: Health / ContactDamage / Inventory / Pickup / Lifetime / Spawner.
#include "Objects/Components/GameplayDetail.h"
#include <cmath>
#include <climits>
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {
using namespace GameplayDetail;

//==================================================
// Health
//==================================================
template <class F> bool Health::Bind(Settings &s, F &f) {
    return f.Float("max", s.max, 0.001f, 1.0e6f) && f.Float("current", s.current, 0.0f, 1.0e6f) &&
        f.Float("invulnerability", s.invulnerability, 0.0f, 60.0f) && f.Bool("integerOnly", s.integerOnly) && f.Int("onDeath", s.onDeath, 0, 2) &&
        f.EventName("damagedEvent", s.damagedEvent) && f.EventName("healedEvent", s.healedEvent) && f.EventName("changedEvent", s.changedEvent) &&
        f.EventName("deathEvent", s.deathEvent) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Health::IsValid(const Settings &s, std::string *error) {
    if (!std::isfinite(s.max) || s.max <= 0 || s.max > 1.0e6f) return Fail(error, "最大体力は 0 より大きく 1000000 以下です。");
    if (!std::isfinite(s.current) || s.current < 0 || s.current > s.max) return Fail(error, "現在の体力は 0〜最大体力です。");
    if (s.integerOnly && (std::floor(s.max) != s.max || std::floor(s.current) != s.current)) return Fail(error, "整数のみの設定では体力も整数にしてください。");
    if (!std::isfinite(s.invulnerability) || s.invulnerability < 0 || s.invulnerability > 60) return Fail(error, "無敵時間は 0〜60 秒です。");
    if (s.onDeath < 0 || s.onDeath > 2) return Fail(error, "死亡時の処理が不正です。");
    for (const auto *name : {&s.damagedEvent, &s.healedEvent, &s.changedEvent, &s.deathEvent}) if (!ValidEventName(*name)) return Fail(error, "イベント名が不正です。");
    return s.sourceText.size() <= 512 || Fail(error, "生成元の文章が長すぎます。");
}
bool Health::SetSettings(const Settings &settings) {
    if (!IsValid(settings)) return false;
    settings_ = settings;
    dead_ = settings_.current <= 0;
    return true;
}
std::unique_ptr<IObjectComponent> Health::Clone() const {
    auto copy = std::make_unique<Health>();
    copy->settings_ = settings_;
    copy->dead_ = settings_.current <= 0;
    return copy;
}
void Health::Initialize() { invulnerable_ = 0; dead_ = settings_.current <= 0; pendingRemoval_ = false; }
bool Health::CheckAmount(float amount, std::string *error) const {
    if (!std::isfinite(amount) || amount <= 0 || amount > 1.0e6f) return Fail(error, "量は 0 より大きく 1000000 以下の数値です。");
    if (settings_.integerOnly && std::floor(amount) != amount) return Fail(error, "この体力は整数の量だけ受け付けます。");
    return true;
}
void Health::Changed(float previous, float amount, const std::string &sender, const std::string &kindEvent) {
    auto *scene = GetOwnerSceneContext();
    const GameEvents::Value payload{{"amount", amount}, {"current", settings_.current}, {"max", settings_.max}, {"previous", previous}, {"sender", sender}};
    std::string error;
    if (!Gameplay::EmitEvent(scene, Owner(this), kindEvent, payload, &error)) lastError_ = error;
    if (!Gameplay::EmitEvent(scene, Owner(this), settings_.changedEvent, payload, &error)) lastError_ = error;
}
bool Health::Damage(float amount, const std::string &sender, std::string *error) {
    if (!CheckAmount(amount, error)) return false;
    if (dead_) return Fail(error, "既に体力が 0 です。");
    if (invulnerable_ > 0) return Fail(error, "無敵時間中です。");
    const float previous = settings_.current;
    settings_.current = std::max(0.0f, settings_.current - amount);
    invulnerable_ = settings_.invulnerability;
    Changed(previous, amount, sender, settings_.damagedEvent);
    if (settings_.current <= 0) {
        dead_ = true;
        std::string emitError;
        if (!Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.deathEvent,
            GameEvents::Value{{"amount", amount}, {"current", 0.0f}, {"max", settings_.max}, {"previous", previous}, {"sender", sender}}, &emitError)) lastError_ = emitError;
        // Removal happens on this component's next Update so callers never see their object destroyed mid-call.
        pendingRemoval_ = settings_.onDeath != 0;
    }
    return true;
}
bool Health::Heal(float amount, const std::string &sender, std::string *error) {
    if (!CheckAmount(amount, error)) return false;
    if (dead_) return Fail(error, "体力が 0 のため回復できません（revive を使います）。");
    const float previous = settings_.current;
    settings_.current = std::min(settings_.max, settings_.current + amount);
    Changed(previous, settings_.current - previous, sender, settings_.healedEvent);
    return true;
}
bool Health::SetCurrent(float value) {
    if (!std::isfinite(value) || value < 0 || value > settings_.max || (settings_.integerOnly && std::floor(value) != value)) return false;
    const float previous = settings_.current;
    settings_.current = value;
    dead_ = value <= 0;
    if (previous != value) {
        std::string error;
        if (!Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.changedEvent,
            GameEvents::Value{{"amount", value - previous}, {"current", value}, {"max", settings_.max}, {"previous", previous}}, &error)) lastError_ = error;
    }
    return true;
}
void Health::Revive() {
    pendingRemoval_ = false;
    invulnerable_ = 0;
    dead_ = false;
    SetCurrent(settings_.max);
}
void Health::Advance(double dt) {
    if (!ValidDt(dt)) return;
    invulnerable_ = std::max(0.0f, invulnerable_ - static_cast<float>(dt));
}
void Health::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!Gameplay::IsPlaying(scene)) return;
    if (pendingRemoval_) {
        pendingRemoval_ = false;
        Gameplay::RemoveOwner(scene, Owner(this), settings_.onDeath);
        return;
    }
    Advance(GetDeltaTime());
}
bool Health::ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) {
    double amount = 0;
    std::string from = sender;
    if (args.is_object() && args.contains("sender") && args["sender"].is_string()) from = args["sender"].get<std::string>();
    if (command == "damage") return ArgNumber(args, "amount", amount, error) && Damage(static_cast<float>(amount), from, &error);
    if (command == "heal") return ArgNumber(args, "amount", amount, error) && Heal(static_cast<float>(amount), from, &error);
    if (command == "set") {
        if (!ArgNumber(args, "value", amount, error)) return false;
        if (SetCurrent(static_cast<float>(amount))) return true;
        error = "体力は 0〜最大値です。";
        return false;
    }
    if (command == "revive") { Revive(); return true; }
    error = "Health に命令 " + command + " はありません。";
    return false;
}
std::vector<std::string> Health::GetEventCommands() const { return {"damage(amount: 数値, sender?: オブジェクト)", "heal(amount: 数値)", "set(value: 数値)", "revive()"}; }
JSON Health::SaveToJson() const { return SaveSettings<Health>(settings_); }
bool Health::LoadFromJson(const JSON &json) {
    if (!LoadSettings<Health>(json, settings_)) return false;
    dead_ = settings_.current <= 0; invulnerable_ = 0; pendingRemoval_ = false;
    return true;
}
#if defined(USE_IMGUI)
void Health::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::DragFloat("最大体力", &next.max, 1.0f, 0.001f, 1.0e6f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    next.current = std::min(next.current, next.max);
    ImGui::DragFloat("現在の体力", &next.current, 1.0f, 0.0f, next.max, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("無敵時間 (秒)", &next.invulnerability, 0.01f, 0.0f, 60.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("整数のみ", &next.integerOnly);
    ImGui::Combo("死亡時", &next.onDeath, "何もしない\0無効化\0削除\0");
    EventInput("ダメージイベント", next.damagedEvent);
    EventInput("回復イベント", next.healedEvent);
    EventInput("体力変化イベント", next.changedEvent);
    EventInput("死亡イベント", next.deathEvent);
    ApplyEdit(*this, next, settings_);
    ImGui::Text("状態: %s / 無敵残り %.2f 秒", dead_ ? "死亡" : "生存", invulnerable_);
    ImGui::TextWrapped("命令: damage(amount) / heal(amount) / set(value) / revive()。GameEventListener から呼べます。");
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

//==================================================
// ContactDamage
//==================================================
template <class F> bool ContactDamage::Bind(Settings &s, F &f) {
    return f.Text("otherTag", s.otherTag, 64) && f.Float("amount", s.amount, 0.0f, 1.0e6f) && f.Float("cooldown", s.cooldown, 0.0f, 60.0f) &&
        f.Int("mode", s.mode, 0, 1) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool ContactDamage::IsValid(const Settings &s, std::string *error) {
    if (s.otherTag.empty() || !ValidTag(s.otherTag)) return Fail(error, "相手のタグを指定してください（64バイト以内）。");
    if (!std::isfinite(s.amount) || s.amount <= 0 || s.amount > 1.0e6f) return Fail(error, "ダメージ量は 0 より大きい数値です。");
    if (!std::isfinite(s.cooldown) || s.cooldown < 0 || s.cooldown > 60) return Fail(error, "クールダウンは 0〜60 秒です。");
    return (s.mode == 0 || s.mode == 1) || Fail(error, "方向が不正です。");
}
bool ContactDamage::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; cooldowns_.clear(); return true; }
std::unique_ptr<IObjectComponent> ContactDamage::Clone() const { auto copy = std::make_unique<ContactDamage>(); copy->settings_ = settings_; return copy; }
bool ContactDamage::ApplyContact(EmptyObject *other) {
    auto *owner = Owner(this);
    if (!owner || !other || other->GetTagName() != settings_.otherTag) return false;
    const auto id = Gameplay::IdOf(other);
    if (cooldowns_.contains(id)) return false;
    Health *health = settings_.mode == 0 ? owner->GetComponent<Health>() : other->GetComponent<Health>();
    if (!health) { lastError_ = settings_.mode == 0 ? "自分に Health がありません。" : other->GetName() + " に Health がありません。"; return false; }
    std::string error;
    const bool applied = health->Damage(settings_.amount, settings_.mode == 0 ? id : Gameplay::IdOf(owner), &error);
    cooldowns_[id] = settings_.cooldown; // also while invulnerable: one hit per contact window
    if (!applied) { lastError_ = error; return false; }
    lastError_.clear();
    return true;
}
int ContactDamage::Advance(double dt) {
    if (!ValidDt(dt)) return 0;
    for (auto it = cooldowns_.begin(); it != cooldowns_.end();) {
        it->second -= static_cast<float>(dt);
        it = it->second <= 0 ? cooldowns_.erase(it) : std::next(it);
    }
    int applied = 0;
    for (auto *other : Gameplay::QueryContacts(Owner(this), GetOwnerSceneContext())) if (ApplyContact(other)) ++applied;
    return applied;
}
void ContactDamage::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(GetDeltaTime()); }
JSON ContactDamage::SaveToJson() const { return SaveSettings<ContactDamage>(settings_); }
bool ContactDamage::LoadFromJson(const JSON &json) { if (!LoadSettings<ContactDamage>(json, settings_)) return false; cooldowns_.clear(); return true; }
#if defined(USE_IMGUI)
void ContactDamage::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("相手のタグ", &next.otherTag);
    ImGui::DragFloat("ダメージ量", &next.amount, 0.1f, 0.001f, 1.0e6f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("同じ相手へのクールダウン (秒)", &next.cooldown, 0.01f, 0.0f, 60.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Combo("方向", &next.mode, "自分がダメージを受ける\0相手へダメージを与える\0");
    ApplyEdit(*this, next, settings_);
    ImGui::TextWrapped("自分と相手に Collider、シーンに SceneObjectCollider、ダメージを受ける側に Health が必要です。");
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

//==================================================
// Inventory
//==================================================
bool Inventory::IsValidItemId(const std::string &item) {
    if (item.empty() || item.size() > 64) return false;
    for (unsigned char c : item) if (c < 32 || c == 127) return false;
    return true;
}
template <class F> bool Inventory::Bind(Settings &s, F &f) {
    JSON items = JSON::object();
    if (!f.Reading()) for (const auto &[item, count] : s.items) items[item] = count;
    if (!f.Raw("items", items)) return false;
    if (f.Reading()) {
        if (!items.is_object() || items.size() > 256) return false;
        s.items.clear();
        for (const auto &[item, count] : items.items()) {
            if (!count.is_number_integer() || count.get<long long>() < 1 || count.get<long long>() > 1000000) return false;
            s.items[item] = count.get<int>();
        }
    }
    return f.Int("capacity", s.capacity, 1, 256) && f.Int("maxStack", s.maxStack, 1, 1000000) && f.EventName("changedEvent", s.changedEvent) &&
        f.Text("saveKey", s.saveKey, 64) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Inventory::IsValid(const Settings &s, std::string *error) {
    if (s.capacity < 1 || s.capacity > 256) return Fail(error, "種類数の上限は 1〜256 です。");
    if (s.maxStack < 1 || s.maxStack > 1000000) return Fail(error, "1種類の上限は 1〜1000000 です。");
    if (static_cast<int>(s.items.size()) > s.capacity) return Fail(error, "所持品の種類が上限を超えています。");
    for (const auto &[item, count] : s.items) if (!IsValidItemId(item) || count < 1 || count > s.maxStack) return Fail(error, "所持品「" + item + "」の個数が不正です。");
    if (!ValidEventName(s.changedEvent)) return Fail(error, "イベント名が不正です。");
    if (!s.saveKey.empty() && !GameEvents::IsValidName(s.saveKey)) return Fail(error, "保存キーが不正です。");
    return true;
}
bool Inventory::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Inventory::Clone() const { auto copy = std::make_unique<Inventory>(); copy->settings_ = settings_; return copy; }
int Inventory::Count(const std::string &item) const { auto it = settings_.items.find(item); return it == settings_.items.end() ? 0 : it->second; }
void Inventory::Notify(const std::string &item, int delta, const std::string &sender) {
    std::string error;
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.changedEvent,
        GameEvents::Value{{"item", item}, {"amount", delta}, {"count", Count(item)}, {"sender", sender}}, &error);
}
bool Inventory::Add(const std::string &item, int amount, const std::string &sender, std::string *error) {
    if (!IsValidItemId(item)) return Fail(error, "アイテムIDが不正です。");
    if (amount < 1 || amount > 1000000) return Fail(error, "個数は 1 以上です。");
    const int current = Count(item);
    if (current == 0 && static_cast<int>(settings_.items.size()) >= settings_.capacity) return Fail(error, "所持できる種類の上限です。");
    if (static_cast<long long>(current) + amount > settings_.maxStack) return Fail(error, "「" + item + "」は上限 " + std::to_string(settings_.maxStack) + " 個までです。");
    settings_.items[item] = current + amount;
    Notify(item, amount, sender);
    return true;
}
bool Inventory::Remove(const std::string &item, int amount, const std::string &sender, std::string *error) {
    if (amount < 1 || amount > 1000000) return Fail(error, "個数は 1 以上です。");
    const int current = Count(item);
    if (current < amount) return Fail(error, "「" + item + "」が足りません（" + std::to_string(current) + " / " + std::to_string(amount) + "）。");
    if (current == amount) settings_.items.erase(item); else settings_.items[item] = current - amount;
    Notify(item, -amount, sender);
    return true;
}
void Inventory::Clear() { settings_.items.clear(); }
bool Inventory::SaveToGlobal(std::string *error) const {
    auto *scene = GetOwnerSceneContext();
    if (!scene || settings_.saveKey.empty()) return Fail(error, "保存キーとシーンが必要です。");
    JSON items = JSON::object();
    for (const auto &[item, count] : settings_.items) items[item] = count;
    scene->RemoveGlobalSceneVariable("__inventory." + settings_.saveKey);
    scene->AddGlobalSceneVariable<std::string>("__inventory." + settings_.saveKey, items.dump());
    return true;
}
bool Inventory::LoadFromGlobal(std::string *error) {
    auto *scene = GetOwnerSceneContext();
    if (!scene || settings_.saveKey.empty()) return Fail(error, "保存キーとシーンが必要です。");
    auto *value = scene->GetGlobalSceneVariable("__inventory." + settings_.saveKey);
    if (!value || !value->IsType<std::string>()) return Fail(error, "保存されたインベントリがありません。");
    auto next = settings_;
    try {
        const auto items = JSON::parse(value->AnyCast<std::string>());
        if (!items.is_object()) return Fail(error, "保存データが不正です。");
        next.items.clear();
        for (const auto &[item, count] : items.items()) { if (!count.is_number_integer()) return Fail(error, "保存データが不正です。"); next.items[item] = count.get<int>(); }
    } catch (const std::exception &) { return Fail(error, "保存データを読めません。"); }
    if (!IsValid(next, error)) return false;
    settings_ = std::move(next);
    return true;
}
bool Inventory::ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) {
    std::string item;
    int amount = 1;
    if (args.is_object() && args.contains("amount")) {
        if (!args["amount"].is_number_integer() && !(args["amount"].is_number() && std::floor(args["amount"].get<double>()) == args["amount"].get<double>())) { error = "amount は整数です。"; return false; }
        const double n = args["amount"].get<double>();
        if (n < 1 || n > 1000000) { error = "amount は 1 以上です。"; return false; }
        amount = static_cast<int>(n);
    }
    if (command == "add") return ArgText(args, "item", item, error) && Add(item, amount, sender, &error);
    if (command == "remove" || command == "consume") return ArgText(args, "item", item, error) && Remove(item, amount, sender, &error);
    if (command == "clear") { Clear(); return true; }
    if (command == "save") return SaveToGlobal(&error);
    if (command == "load") return LoadFromGlobal(&error);
    error = "Inventory に命令 " + command + " はありません。";
    return false;
}
std::vector<std::string> Inventory::GetEventCommands() const { return {"add(item: 文字列, amount?: 整数)", "remove(item, amount?)", "consume(item, amount?)", "clear()", "save()", "load()"}; }
JSON Inventory::SaveToJson() const { return SaveSettings<Inventory>(settings_); }
bool Inventory::LoadFromJson(const JSON &json) { return LoadSettings<Inventory>(json, settings_); }
#if defined(USE_IMGUI)
void Inventory::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputInt("種類数の上限", &next.capacity);
    ImGui::InputInt("1種類の上限", &next.maxStack);
    EventInput("増減イベント", next.changedEvent);
    ImGui::InputText("保存キー（グローバル変数）", &next.saveKey);
    std::string removeItem;
    for (auto &[item, count] : next.items) {
        ImGui::PushID(item.c_str());
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt(item.c_str(), &count);
        ImGui::SameLine();
        if (ImGui::SmallButton("削除")) removeItem = item;
        ImGui::PopID();
    }
    if (!removeItem.empty()) next.items.erase(removeItem);
    ImGui::InputText("追加するアイテムID", &newItem_);
    if (ImGui::Button("アイテムを追加") && IsValidItemId(newItem_) && !next.items.contains(newItem_)) next.items[newItem_] = 1;
    ApplyEdit(*this, next, settings_);
    ImGui::TextWrapped("命令: add(item, amount) / remove / consume / clear / save / load。");
}
#endif

//==================================================
// Pickup
//==================================================
template <class F> bool Pickup::Bind(Settings &s, F &f) {
    return f.Text("recipientTag", s.recipientTag, 64) && f.Text("itemId", s.itemId, 64) && f.Int("amount", s.amount, 1, 1000000) &&
        f.Text("scoreVariable", s.scoreVariable, 64) && f.Bool("once", s.once) && f.Int("after", s.after, 0, 2) && f.EventName("pickedEvent", s.pickedEvent) &&
        f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Pickup::IsValid(const Settings &s, std::string *error) {
    if (s.recipientTag.empty() || !ValidTag(s.recipientTag)) return Fail(error, "受け取り手のタグを指定してください。");
    if (s.itemId.empty() && s.scoreVariable.empty()) return Fail(error, "アイテムIDかスコアのシーン変数を指定してください。");
    if (!s.itemId.empty() && !Inventory::IsValidItemId(s.itemId)) return Fail(error, "アイテムIDが不正です。");
    if (s.amount < 1 || s.amount > 1000000) return Fail(error, "個数は 1 以上です。");
    if (s.after < 0 || s.after > 2 || !ValidEventName(s.pickedEvent)) return Fail(error, "取得後の処理またはイベント名が不正です。");
    return true;
}
bool Pickup::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Pickup::Clone() const { auto copy = std::make_unique<Pickup>(); copy->settings_ = settings_; return copy; }
bool Pickup::TryPickup(EmptyObject *recipient, std::string *error) {
    auto *scene = GetOwnerSceneContext();
    auto *owner = Owner(this);
    if (picked_ && settings_.once) return Fail(error, "既に取得されています。");
    if (!recipient || recipient == owner || recipient->GetTagName() != settings_.recipientTag) return Fail(error, "受け取り手ではありません。");
    int count = settings_.amount;
    if (settings_.scoreVariable.empty()) {
        auto *inventory = recipient->GetComponent<Inventory>();
        if (!inventory) return Fail(error, recipient->GetName() + " に Inventory がありません。受け取り手へ Inventory を追加してください。");
        if (!inventory->Add(settings_.itemId, settings_.amount, Gameplay::IdOf(owner), error)) return false;
        count = inventory->Count(settings_.itemId);
    } else {
        if (!scene) return Fail(error, "シーンがありません。");
        auto *value = scene->GetSceneVariable(settings_.scoreVariable);
        int current = 0;
        if (value) {
            if (!value->IsType<int>()) return Fail(error, "シーン変数「" + settings_.scoreVariable + "」は整数ではありません。");
            current = value->AnyCast<int>();
        }
        if (static_cast<long long>(current) + settings_.amount > INT_MAX) return Fail(error, "スコアが上限を超えます。");
        count = current + settings_.amount;
        scene->AddSceneVariable<int>(settings_.scoreVariable, count);
    }
    picked_ = true;
    std::string emitError;
    if (!Gameplay::EmitEvent(scene, owner, settings_.pickedEvent, GameEvents::Value{{"item", settings_.itemId.empty() ? settings_.scoreVariable : settings_.itemId},
        {"amount", settings_.amount}, {"count", count}, {"recipient", Gameplay::IdOf(recipient)}}, &emitError)) lastError_ = emitError;
    return true;
}
bool Pickup::Advance() {
    if (picked_ && settings_.once) return false;
    auto *scene = GetOwnerSceneContext();
    for (auto *other : Gameplay::QueryContacts(Owner(this), scene)) {
        if (other->GetTagName() != settings_.recipientTag) continue;
        std::string error;
        if (!TryPickup(other, &error)) { lastError_ = error; continue; }
        if (settings_.after != 0) {
            Gameplay::RemoveOwner(scene, Owner(this), settings_.after); // last statement touching this component
            return true;
        }
        return true;
    }
    return false;
}
void Pickup::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(); }
JSON Pickup::SaveToJson() const { return SaveSettings<Pickup>(settings_); }
bool Pickup::LoadFromJson(const JSON &json) { if (!LoadSettings<Pickup>(json, settings_)) return false; picked_ = false; return true; }
#if defined(USE_IMGUI)
void Pickup::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("受け取り手のタグ", &next.recipientTag);
    ImGui::InputText("アイテムID（Inventoryへ追加）", &next.itemId);
    ImGui::InputInt("個数", &next.amount);
    ImGui::InputText("スコアのシーン変数（空欄ならInventory）", &next.scoreVariable);
    ImGui::Checkbox("一度だけ", &next.once);
    ImGui::Combo("取得後", &next.after, "残す\0無効化\0削除\0");
    EventInput("取得イベント", next.pickedEvent);
    ApplyEdit(*this, next, settings_);
    if (settings_.scoreVariable.empty()) ImGui::TextWrapped("受け取り手（タグ %s）に Inventory が必要です。自動では作りません。", settings_.recipientTag.c_str());
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

//==================================================
// Lifetime
//==================================================
template <class F> bool Lifetime::Bind(Settings &s, F &f) {
    return f.Float("duration", s.duration, 0.01f, 3600.0f) && f.Int("action", s.action, 1, 2) && f.EventName("expiredEvent", s.expiredEvent) &&
        f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Lifetime::IsValid(const Settings &s, std::string *error) {
    if (!std::isfinite(s.duration) || s.duration < 0.01f || s.duration > 3600) return Fail(error, "寿命は 0.01〜3600 秒です。");
    if (s.action < 1 || s.action > 2 || !ValidEventName(s.expiredEvent)) return Fail(error, "期限後の処理またはイベント名が不正です。");
    return true;
}
bool Lifetime::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Lifetime::Clone() const { auto copy = std::make_unique<Lifetime>(); copy->settings_ = settings_; return copy; }
bool Lifetime::Advance(double dt) {
    if (!ValidDt(dt)) return false;
    if (expired_) {
        // Updated again after a pool release means the object was re-activated: count from zero.
        auto *owner = Owner(this);
        if (settings_.action != 1 || !owner || !owner->IsActive()) return false;
        ResetTimer();
    }
    elapsed_ += dt;
    if (elapsed_ < settings_.duration) return false;
    expired_ = true;
    auto *scene = GetOwnerSceneContext();
    std::string error;
    Gameplay::EmitEvent(scene, Owner(this), settings_.expiredEvent, GameEvents::Value{{"elapsed", elapsed_}}, &error);
    Gameplay::RemoveOwner(scene, Owner(this), settings_.action); // last statement touching this component
    return true;
}
void Lifetime::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(GetDeltaTime()); }
JSON Lifetime::SaveToJson() const { return SaveSettings<Lifetime>(settings_); }
bool Lifetime::LoadFromJson(const JSON &json) { if (!LoadSettings<Lifetime>(json, settings_)) return false; ResetTimer(); return true; }
#if defined(USE_IMGUI)
void Lifetime::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::DragFloat("寿命 (秒)", &next.duration, 0.01f, 0.01f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    int action = next.action - 1;
    if (ImGui::Combo("期限後", &action, "無効化（プールへ返却）\0削除\0")) next.action = action + 1;
    EventInput("期限イベント", next.expiredEvent);
    ApplyEdit(*this, next, settings_);
    ImGui::TextWrapped("再生中だけ進みます。再有効化（プールからの再利用）で 0 秒に戻ります。経過 %.2f 秒", elapsed_);
}
#endif

//==================================================
// Spawner
//==================================================
template <class F> bool Spawner::Bind(Settings &s, F &f) {
    return f.Text("templateObject", s.templateObject, 128) && f.Float("interval", s.interval, 0.05f, 3600.0f) && f.Int("maxAlive", s.maxAlive, 1, 256) &&
        f.Int("perUpdate", s.perUpdate, 1, 4) && f.Vec3("offset", s.offset) && f.Bool("parentToSpawner", s.parentToSpawner) && f.Bool("reusePool", s.reusePool) &&
        f.Bool("spawnOnStart", s.spawnOnStart) && f.EventName("spawnedEvent", s.spawnedEvent) && f.EventName("limitEvent", s.limitEvent) &&
        f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Spawner::IsValid(const Settings &s, std::string *error) {
    if (!std::isfinite(s.interval) || s.interval < 0.05f || s.interval > 3600) return Fail(error, "間隔は 0.05〜3600 秒です。");
    if (s.maxAlive < 1 || s.maxAlive > 256) return Fail(error, "最大数は 1〜256 です。");
    if (s.perUpdate < 1 || s.perUpdate > 4) return Fail(error, "1回の更新で生成する数は 1〜4 です。");
    if (!ValidEventName(s.spawnedEvent) || !ValidEventName(s.limitEvent)) return Fail(error, "イベント名が不正です。");
    return s.templateObject.size() <= 128 || Fail(error, "テンプレートの指定が長すぎます。");
}
bool Spawner::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Spawner::Clone() const { auto copy = std::make_unique<Spawner>(); copy->settings_ = settings_; return copy; }
void Spawner::Initialize() { timer_ = 0; started_ = false; limitNotified_ = false; spawned_.clear(); lastError_.clear(); }
EmptyObject *Spawner::ResolveTemplate(std::string *error) {
    auto *scene = GetOwnerSceneContext();
    auto *owner = Owner(this);
    if (settings_.templateObject.empty()) { Fail(error, "生成するテンプレートオブジェクトを指定してください（既存のオブジェクト。自動では作りません）。"); return nullptr; }
    auto *source = Gameplay::ResolveObject(scene, settings_.templateObject, error);
    if (!source) return nullptr;
    for (auto *p = owner; p; p = p->GetComponent<Transform>() ? p->GetComponent<Transform>()->GetParentObject() : nullptr)
        if (p == source) { Fail(error, "自分自身や親はテンプレートにできません。"); return nullptr; }
    // A template that contains a Spawner (itself or a child) would multiply without bound.
    std::vector<EmptyObject *> stack{source};
    while (!stack.empty()) {
        auto *object = stack.back(); stack.pop_back();
        if (object->GetComponent<Spawner>()) { Fail(error, "Spawner を含むテンプレートは使えません（自己複製を防ぐため）。"); return nullptr; }
        for (auto *candidate : scene->GetSceneObjects()) {
            auto *t = candidate ? candidate->GetComponent<Transform>() : nullptr;
            if (t && t->GetParentObject() == object) stack.push_back(candidate);
        }
    }
    return source;
}
void Spawner::Prune() {
    auto *scene = GetOwnerSceneContext();
    std::erase_if(spawned_, [&](const UUID128 &id) { return !scene || !scene->GetSceneObject(id); });
}
int Spawner::GetAliveCount() {
    Prune();
    auto *scene = GetOwnerSceneContext();
    int alive = 0;
    for (const auto &id : spawned_) if (auto *object = scene->GetSceneObject(id); object && object->IsActive()) ++alive;
    return alive;
}
EmptyObject *Spawner::SpawnOne(std::string *error) {
    auto *scene = GetOwnerSceneContext();
    auto *owner = Owner(this);
    if (!scene || !owner) { Fail(error, "シーンがありません。"); return nullptr; }
    const int alive = GetAliveCount();
    if (alive >= settings_.maxAlive) {
        if (!limitNotified_) {
            limitNotified_ = true;
            std::string emitError;
            Gameplay::EmitEvent(scene, owner, settings_.limitEvent, GameEvents::Value{{"count", alive}, {"max", settings_.maxAlive}}, &emitError);
        }
        Fail(error, "最大数 " + std::to_string(settings_.maxAlive) + " に達しています。");
        return nullptr;
    }
    limitNotified_ = false;
    EmptyObject *spawned = nullptr;
    if (settings_.reusePool) {
        for (const auto &id : spawned_) if (auto *object = scene->GetSceneObject(id); object && !object->IsActive()) { spawned = object; break; }
    }
    if (!spawned) {
        auto *source = ResolveTemplate(error);
        if (!source) return nullptr;
        // The template is cloned with its children: the whole subtree must fit under the scene limit.
        size_t subtree = 0;
        for (std::vector<EmptyObject *> stack{source}; !stack.empty();) {
            auto *object = stack.back(); stack.pop_back();
            ++subtree;
            for (auto *candidate : scene->GetSceneObjects()) {
                auto *t = candidate ? candidate->GetComponent<Transform>() : nullptr;
                if (t && t->GetParentObject() == object) stack.push_back(candidate);
            }
        }
        if (scene->GetSceneObjects().size() + subtree > static_cast<size_t>(kSceneObjectLimit)) { Fail(error, "シーンのオブジェクト数の上限（4096）を超えるため生成しません。"); return nullptr; }
        if (static_cast<int>(spawned_.size()) >= settings_.maxAlive * 2) { Fail(error, "プールの上限に達しています。"); return nullptr; }
        spawned = scene->CloneObject(source, source->GetName() + " (Spawned)", true);
        if (!spawned) { Fail(error, "テンプレートを複製できませんでした。"); return nullptr; }
        spawned_.push_back(spawned->GetObjectID());
    }
    if (auto *transform = spawned->GetComponent<Transform>()) {
        if (settings_.parentToSpawner) { transform->SetParentObject(owner); transform->SetTranslate(settings_.offset); }
        else { transform->SetParentObject(nullptr); transform->SetTranslate(Gameplay::WorldPosition(owner) + settings_.offset); }
    }
    spawned->SetActive(true);
    if (auto *lifetime = spawned->GetComponent<Lifetime>()) lifetime->ResetTimer(); // pool reuse starts a new life
    std::string emitError;
    if (!Gameplay::EmitEvent(scene, owner, settings_.spawnedEvent, GameEvents::Value{{"spawned", Gameplay::IdOf(spawned)}, {"count", alive + 1}}, &emitError)) lastError_ = emitError;
    return spawned;
}
int Spawner::Advance(double dt) {
    if (!ValidDt(dt)) return 0;
    int count = 0;
    auto spawn = [&]() {
        std::string error;
        if (SpawnOne(&error)) { ++count; return true; }
        lastError_ = error;
        return false;
    };
    if (!started_) { started_ = true; if (settings_.spawnOnStart) spawn(); }
    timer_ += dt;
    while (timer_ >= settings_.interval && count < settings_.perUpdate) {
        timer_ -= settings_.interval;
        if (!spawn()) break;
    }
    if (timer_ >= settings_.interval) timer_ = std::fmod(timer_, static_cast<double>(settings_.interval)); // drop backlog beyond the per-update cap
    return count;
}
void Spawner::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(GetDeltaTime()); }
bool Spawner::ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) {
    (void)args; (void)sender;
    if (command == "spawn") return SpawnOne(&error) != nullptr;
    if (command == "reset") { timer_ = 0; return true; }
    error = "Spawner に命令 " + command + " はありません。";
    return false;
}
std::vector<std::string> Spawner::GetEventCommands() const { return {"spawn()", "reset()"}; }
JSON Spawner::SaveToJson() const { return SaveSettings<Spawner>(settings_); }
bool Spawner::LoadFromJson(const JSON &json) { if (!LoadSettings<Spawner>(json, settings_)) return false; timer_ = 0; started_ = false; return true; }
#if defined(USE_IMGUI)
void Spawner::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("テンプレート（名前またはUUID）", &next.templateObject);
    ImGui::DragFloat("間隔 (秒)", &next.interval, 0.01f, 0.05f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::InputInt("最大同時数", &next.maxAlive);
    ImGui::InputInt("1更新の最大生成数", &next.perUpdate);
    ImGui::DragFloat3("生成位置のオフセット", &next.offset.x, 0.01f);
    ImGui::Checkbox("Spawnerの子にする", &next.parentToSpawner);
    ImGui::Checkbox("無効化された生成物を再利用", &next.reusePool);
    ImGui::Checkbox("再生開始時にも1体生成", &next.spawnOnStart);
    EventInput("生成イベント", next.spawnedEvent);
    EventInput("上限イベント", next.limitEvent);
    ApplyEdit(*this, next, settings_);
    std::string error;
    if (!ResolveTemplate(&error)) ImGui::TextWrapped("必要な設定: %s", error.c_str());
    ImGui::Text("管理中 %zu 体", spawned_.size());
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

bool IsShortTextManagedGameplay(const IObjectComponent *component) {
    if (auto *c = dynamic_cast<const Health *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const ContactDamage *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Inventory *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Pickup *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Lifetime *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Spawner *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Interactable *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Respawner *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Checkpoint *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const Dialogue *>(component)) return c->GetSettings().shortTextManaged;
    if (auto *c = dynamic_cast<const AnimationStateSwitcher *>(component)) return c->GetSettings().shortTextManaged;
    return false;
}
} // namespace KashipanEngine
