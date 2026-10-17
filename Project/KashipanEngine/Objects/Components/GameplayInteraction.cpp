// Gameplay components 5, 6, 8, 9: Interactable / Respawner / Checkpoint / Dialogue / AnimationStateSwitcher.
#include "Objects/Components/GameplayDetail.h"
#include <cmath>
#include <limits>
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Input/Keyboard.h"
#include "Objects/Components/Animator.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Render/BitmapTextRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Scene/SceneContext.h"
#include "Utilities/ShortTextProgram.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {
using namespace GameplayDetail;
namespace {
bool KeyTriggered(SceneContext *scene, Key key) {
    auto *input = scene ? scene->GetInput() : nullptr;
    return input && key != Key::Unknown && input->GetKeyboard().IsTrigger(key);
}
#if defined(USE_IMGUI)
void KeyCombo(const char *label, Key &key) {
    if (ImGui::BeginCombo(label, ShortText::KeyName(key).c_str())) {
        for (int k = static_cast<int>(Key::A); k <= static_cast<int>(Key::F12); ++k) {
            const auto name = ShortText::KeyName(static_cast<Key>(k));
            if (!name.empty() && ImGui::Selectable(name.c_str(), key == static_cast<Key>(k))) key = static_cast<Key>(k);
        }
        ImGui::EndCombo();
    }
}
#endif
}

//==================================================
// Interactable
//==================================================
template <class F> bool Interactable::Bind(Settings &s, F &f) {
    return f.Text("interactorTag", s.interactorTag, 64) && f.Float("distance", s.distance, 0.01f, 1000.0f) && f.KeyCode("key", s.key) &&
        f.Text("inputAction", s.inputAction, 64) && f.Text("requiredItem", s.requiredItem, 64) && f.Int("requiredAmount", s.requiredAmount, 1, 1000000) &&
        f.Bool("consumeItem", s.consumeItem) && f.Text("keyframeClip", s.keyframeClip, 128) && f.Text("animatorClip", s.animatorClip, 128) &&
        f.Bool("startDialogue", s.startDialogue) && f.Bool("once", s.once) && f.EventName("interactEvent", s.interactEvent) &&
        f.EventName("deniedEvent", s.deniedEvent) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Interactable::IsValid(const Settings &s, std::string *error) {
    if (s.interactorTag.empty() || !ValidTag(s.interactorTag)) return Fail(error, "操作する相手のタグを指定してください。");
    if (!std::isfinite(s.distance) || s.distance < 0.01f || s.distance > 1000) return Fail(error, "距離は 0.01〜1000 です。");
    if (s.inputAction.empty() && ShortText::KeyName(s.key).empty()) return Fail(error, "キーか入力アクションを指定してください。");
    if (!s.requiredItem.empty() && !Inventory::IsValidItemId(s.requiredItem)) return Fail(error, "必要アイテムIDが不正です。");
    if (s.requiredAmount < 1) return Fail(error, "必要数は 1 以上です。");
    if (!ValidEventName(s.interactEvent) || !ValidEventName(s.deniedEvent)) return Fail(error, "イベント名が不正です。");
    return true;
}
bool Interactable::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Interactable::Clone() const { auto copy = std::make_unique<Interactable>(); copy->settings_ = settings_; return copy; }
bool Interactable::HasAction() const {
    return !settings_.keyframeClip.empty() || !settings_.animatorClip.empty() || settings_.startDialogue || !settings_.interactEvent.empty();
}
EmptyObject *Interactable::FindInteractor() {
    auto *scene = GetOwnerSceneContext();
    auto *owner = Owner(this);
    if (!scene || !owner) return nullptr;
    const Vector3 self = Gameplay::WorldPosition(owner);
    EmptyObject *best = nullptr;
    float bestDistance = settings_.distance;
    for (auto *object : scene->GetSceneObjects()) {
        if (!object || object == owner || !object->IsActive() || object->GetTagName() != settings_.interactorTag) continue;
        const Vector3 d = Gameplay::WorldPosition(object) - self;
        const float distance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (distance <= bestDistance) { best = object; bestDistance = distance; }
    }
    return best;
}
bool Interactable::Interact(EmptyObject *interactor, std::string *error) {
    auto *scene = GetOwnerSceneContext();
    auto *owner = Owner(this);
    if (!owner || !interactor) return Fail(error, "操作する相手がいません。");
    if (settings_.once && used_) return Fail(error, "既に使用済みです。");
    if (!HasAction()) return Fail(error, "実行する処理が未設定です。開くクリップ（KeyFrameAnimator / Animator）、会話、またはイベントを設定してください。");
    if (!settings_.requiredItem.empty()) {
        auto *inventory = interactor->GetComponent<Inventory>();
        if (!inventory || !inventory->Has(settings_.requiredItem, settings_.requiredAmount)) {
            std::string emitError;
            Gameplay::EmitEvent(scene, owner, settings_.deniedEvent, GameEvents::Value{{"interactor", Gameplay::IdOf(interactor)}, {"item", settings_.requiredItem}}, &emitError);
            return Fail(error, inventory ? "「" + settings_.requiredItem + "」を持っていません。" : interactor->GetName() + " に Inventory がありません。");
        }
    }
    // Validate every target before changing anything so a missing clip never consumes the item.
    KeyFrameAnimator *keyframes = nullptr;
    Animator *animator = nullptr;
    Dialogue *dialogue = nullptr;
    if (!settings_.keyframeClip.empty()) {
        keyframes = owner->GetComponent<KeyFrameAnimator>();
        if (!keyframes || !keyframes->HasAnimation(settings_.keyframeClip)) return Fail(error, "KeyFrameAnimator のアニメーション「" + settings_.keyframeClip + "」がありません。");
    }
    if (!settings_.animatorClip.empty() && !(animator = owner->GetComponent<Animator>())) return Fail(error, "Animator がありません。");
    if (settings_.startDialogue && !(dialogue = owner->GetComponent<Dialogue>())) return Fail(error, "Dialogue がありません。");
    if (!settings_.requiredItem.empty() && settings_.consumeItem && !interactor->GetComponent<Inventory>()->Remove(settings_.requiredItem, settings_.requiredAmount, Gameplay::IdOf(owner), error)) return false;
    if (keyframes) keyframes->Play(settings_.keyframeClip);
    if (animator) { animator->SetClipName(settings_.animatorClip); animator->Play(); }
    if (dialogue) {
        std::string dialogueError;
        const bool ok = dialogue->IsRunning() ? dialogue->Advance(&dialogueError) : dialogue->Start(&dialogueError);
        if (!ok) lastError_ = dialogueError;
    }
    used_ = true;
    std::string emitError;
    if (!Gameplay::EmitEvent(scene, owner, settings_.interactEvent, GameEvents::Value{{"interactor", Gameplay::IdOf(interactor)}, {"item", settings_.requiredItem}}, &emitError)) lastError_ = emitError;
    return true;
}
bool Interactable::Advance(bool pressed) {
    if (!pressed || (settings_.once && used_)) return false;
    auto *interactor = FindInteractor();
    if (!interactor) return false;
    std::string error;
    if (!Interact(interactor, &error)) { lastError_ = error; return false; }
    return true;
}
void Interactable::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!Gameplay::IsPlaying(scene)) return;
    bool pressed = false;
    if (!settings_.inputAction.empty()) { if (auto *command = scene->GetInputCommand()) pressed = command->Evaluate(settings_.inputAction).Triggered(); }
    else pressed = KeyTriggered(scene, settings_.key);
    Advance(pressed);
}
JSON Interactable::SaveToJson() const { return SaveSettings<Interactable>(settings_); }
bool Interactable::LoadFromJson(const JSON &json) { if (!LoadSettings<Interactable>(json, settings_)) return false; used_ = false; return true; }
#if defined(USE_IMGUI)
void Interactable::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("操作する相手のタグ", &next.interactorTag);
    ImGui::DragFloat("距離", &next.distance, 0.01f, 0.01f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    KeyCombo("キー", next.key);
    ImGui::InputText("入力アクション（空欄はキー）", &next.inputAction);
    ImGui::InputText("必要アイテム", &next.requiredItem);
    ImGui::InputInt("必要数", &next.requiredAmount);
    ImGui::Checkbox("アイテムを消費する", &next.consumeItem);
    ImGui::InputText("再生する KeyFrameAnimator アニメーション", &next.keyframeClip);
    ImGui::InputText("再生する Animator クリップ", &next.animatorClip);
    ImGui::Checkbox("Dialogue を開始／進める", &next.startDialogue);
    ImGui::Checkbox("一度だけ", &next.once);
    EventInput("実行イベント", next.interactEvent);
    EventInput("拒否イベント（アイテム不足）", next.deniedEvent);
    ApplyEdit(*this, next, settings_);
    if (!HasAction()) ImGui::TextWrapped("必要な設定: 開くクリップ・会話・イベントのいずれかを設定してください。");
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

//==================================================
// Respawner
//==================================================
template <class F> bool Respawner::Bind(Settings &s, F &f) {
    return f.Text("checkpointTag", s.checkpointTag, 64) && f.Bool("useFall", s.useFall) && f.Float("fallY", s.fallY, -1.0e6f, 1.0e6f) &&
        f.Bool("onDeath", s.onDeath) && f.Bool("resetVelocity", s.resetVelocity) && f.Bool("restoreHealth", s.restoreHealth) &&
        f.EventName("respawnedEvent", s.respawnedEvent) && f.EventName("checkpointEvent", s.checkpointEvent) &&
        f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Respawner::IsValid(const Settings &s, std::string *error) {
    if (!ValidTag(s.checkpointTag)) return Fail(error, "チェックポイントのタグが不正です。");
    if (!std::isfinite(s.fallY) || std::abs(s.fallY) > 1.0e6f) return Fail(error, "落下判定の高さが不正です。");
    if (!ValidEventName(s.respawnedEvent) || !ValidEventName(s.checkpointEvent)) return Fail(error, "イベント名が不正です。");
    return true;
}
bool Respawner::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Respawner::Clone() const { auto copy = std::make_unique<Respawner>(); copy->settings_ = settings_; return copy; }
bool Respawner::SetRespawnPoint(const Vector3 &world, const Vector3 *rotate, const std::string &sender) {
    if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.z)) return false;
    const bool changed = !hasPoint_ || world.x != point_.x || world.y != point_.y || world.z != point_.z;
    point_ = world;
    hasPoint_ = true;
    if (rotate) { rotate_ = *rotate; hasRotate_ = true; }
    if (changed) {
        std::string error;
        Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.checkpointEvent,
            GameEvents::Value{{"position", {world.x, world.y, world.z}}, {"sender", sender}}, &error);
    }
    return changed;
}
bool Respawner::Respawn(const std::string &reason) {
    auto *owner = Owner(this);
    if (!owner || !hasPoint_) return false;
    Gameplay::SetWorldPosition(owner, point_);
    if (hasRotate_) if (auto *transform = owner->GetComponent<Transform>()) transform->SetRotate(rotate_);
    if (settings_.resetVelocity) if (auto *velocity = owner->GetComponent<Velocity>()) velocity->SetVelocity(Vector3(0, 0, 0));
    if (settings_.restoreHealth) if (auto *health = owner->GetComponent<Health>(); health && (health->IsDead() || reason != "checkpoint")) health->Revive();
    std::string error;
    Gameplay::EmitEvent(GetOwnerSceneContext(), owner, settings_.respawnedEvent,
        GameEvents::Value{{"reason", reason}, {"position", {point_.x, point_.y, point_.z}}}, &error);
    return true;
}
bool Respawner::Advance() {
    auto *owner = Owner(this);
    if (!owner) return false;
    if (!hasPoint_) SetRespawnPoint(Gameplay::WorldPosition(owner)); // the play-start position is the first point
    if (!settings_.checkpointTag.empty()) {
        for (auto *other : Gameplay::QueryContacts(owner, GetOwnerSceneContext())) {
            if (other->GetTagName() != settings_.checkpointTag) continue;
            const auto *checkpoint = other->GetComponent<Checkpoint>();
            const Vector3 point = checkpoint ? const_cast<Checkpoint *>(checkpoint)->GetRespawnWorldPosition() : Gameplay::WorldPosition(other);
            SetRespawnPoint(point, nullptr, Gameplay::IdOf(other));
        }
    }
    if (settings_.useFall && Gameplay::WorldPosition(owner).y < settings_.fallY) return Respawn("fall");
    if (settings_.onDeath) if (auto *health = owner->GetComponent<Health>(); health && health->IsDead()) return Respawn("death");
    return false;
}
void Respawner::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(); }
bool Respawner::ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) {
    if (command == "respawn") return Respawn("command") || (error = "復活地点がありません。", false);
    if (command == "setPoint") {
        if (!args.is_object() || !args.contains("position") || !GameEvents::IsValidArgValue(GameEvents::ArgType::Vector3, args["position"])) { error = "position (Vector3) が必要です。"; return false; }
        const auto &p = args["position"];
        SetRespawnPoint(Vector3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>()), nullptr, sender);
        return true;
    }
    error = "Respawner に命令 " + command + " はありません。";
    return false;
}
std::vector<std::string> Respawner::GetEventCommands() const { return {"respawn()", "setPoint(position: Vector3)"}; }
JSON Respawner::SaveToJson() const { return SaveSettings<Respawner>(settings_); }
bool Respawner::LoadFromJson(const JSON &json) { if (!LoadSettings<Respawner>(json, settings_)) return false; hasPoint_ = false; hasRotate_ = false; return true; }
#if defined(USE_IMGUI)
void Respawner::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("チェックポイントのタグ（空欄は接触で更新しない）", &next.checkpointTag);
    ImGui::Checkbox("落下で復活", &next.useFall);
    ImGui::DragFloat("落下とみなすワールドY（未満）", &next.fallY, 0.1f);
    ImGui::Checkbox("Health が 0 で復活", &next.onDeath);
    ImGui::Checkbox("Velocity を 0 にする", &next.resetVelocity);
    ImGui::Checkbox("Health を最大へ戻す", &next.restoreHealth);
    EventInput("復活イベント", next.respawnedEvent);
    EventInput("復活地点の更新イベント", next.checkpointEvent);
    ApplyEdit(*this, next, settings_);
    if (hasPoint_) ImGui::Text("復活地点 (%.2f, %.2f, %.2f)", point_.x, point_.y, point_.z);
    else ImGui::TextWrapped("再生開始時の位置が最初の復活地点になります。");
}
#endif

//==================================================
// Checkpoint
//==================================================
template <class F> bool Checkpoint::Bind(Settings &s, F &f) {
    return f.Text("playerTag", s.playerTag, 64) && f.Vec3("offset", s.offset) && f.EventName("activatedEvent", s.activatedEvent) &&
        f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Checkpoint::IsValid(const Settings &s, std::string *error) {
    if (s.playerTag.empty() || !ValidTag(s.playerTag)) return Fail(error, "対象のタグを指定してください。");
    return ValidEventName(s.activatedEvent) || Fail(error, "イベント名が不正です。");
}
bool Checkpoint::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; return true; }
std::unique_ptr<IObjectComponent> Checkpoint::Clone() const { auto copy = std::make_unique<Checkpoint>(); copy->settings_ = settings_; return copy; }
Vector3 Checkpoint::GetRespawnWorldPosition() { return Gameplay::WorldPosition(Owner(this)) + settings_.offset; }
int Checkpoint::Advance() {
    int updated = 0;
    auto *owner = Owner(this);
    for (auto *other : Gameplay::QueryContacts(owner, GetOwnerSceneContext())) {
        if (other->GetTagName() != settings_.playerTag) continue;
        auto *respawner = other->GetComponent<Respawner>();
        if (!respawner || !respawner->SetRespawnPoint(GetRespawnWorldPosition(), nullptr, Gameplay::IdOf(owner))) continue;
        ++updated;
        std::string error;
        Gameplay::EmitEvent(GetOwnerSceneContext(), owner, settings_.activatedEvent, GameEvents::Value{{"player", Gameplay::IdOf(other)}}, &error);
    }
    return updated;
}
void Checkpoint::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(); }
JSON Checkpoint::SaveToJson() const { return SaveSettings<Checkpoint>(settings_); }
bool Checkpoint::LoadFromJson(const JSON &json) { return LoadSettings<Checkpoint>(json, settings_); }
#if defined(USE_IMGUI)
void Checkpoint::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    ImGui::InputText("対象のタグ", &next.playerTag);
    ImGui::DragFloat3("復活位置のオフセット", &next.offset.x, 0.01f);
    EventInput("更新イベント", next.activatedEvent);
    ApplyEdit(*this, next, settings_);
    ImGui::TextWrapped("対象に Respawner が必要です（自動では追加しません）。");
}
#endif

//==================================================
// Dialogue
//==================================================
template <class F> bool Dialogue::Bind(Settings &s, F &f) {
    JSON lines = JSON::array();
    if (!f.Reading()) for (const auto &line : s.lines) {
        JSON choices = JSON::array();
        for (const auto &choice : line.choices) choices.push_back({{"label", choice.label}, {"next", choice.next}});
        lines.push_back({{"speaker", line.speaker}, {"text", line.text}, {"choices", choices}, {"next", line.next}});
    }
    if (!f.Raw("lines", lines)) return false;
    if (f.Reading()) {
        if (!lines.is_array() || lines.size() > 256) return false;
        s.lines.clear();
        for (const auto &l : lines) {
            if (!l.is_object()) return false;
            Line line;
            JSON lineJson = l;
            Gameplay::Fields lf(static_cast<const JSON &>(lineJson));
            if (!lf.Text("speaker", line.speaker, 64) || !lf.Text("text", line.text, 1024) || !lf.Int("next", line.next, -2, 255)) return false;
            if (l.contains("choices")) {
                if (!l["choices"].is_array() || l["choices"].size() > 4) return false;
                for (const auto &c : l["choices"]) {
                    Choice choice;
                    Gameplay::Fields cf(c);
                    if (!c.is_object() || !cf.Text("label", choice.label, 128) || !cf.Int("next", choice.next, -1, 255)) return false;
                    line.choices.push_back(choice);
                }
            }
            s.lines.push_back(std::move(line));
        }
    }
    return f.Text("outputObject", s.outputObject, 128) && f.KeyCode("advanceKey", s.advanceKey) && f.Bool("choiceKeys", s.choiceKeys) &&
        f.EventName("startedEvent", s.startedEvent) && f.EventName("lineEvent", s.lineEvent) && f.EventName("choiceEvent", s.choiceEvent) &&
        f.EventName("endedEvent", s.endedEvent) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool Dialogue::IsValid(const Settings &s, std::string *error) {
    const int count = static_cast<int>(s.lines.size());
    if (count > 256) return Fail(error, "行は256行までです。");
    for (int i = 0; i < count; ++i) {
        const auto &line = s.lines[i];
        if (line.next < -2 || line.next >= count) return Fail(error, std::to_string(i + 1) + " 行目の次の行が範囲外です。");
        if (line.choices.size() > 4) return Fail(error, "選択肢は1行に4つまでです。");
        for (const auto &choice : line.choices) {
            if (choice.label.empty()) return Fail(error, std::to_string(i + 1) + " 行目の選択肢の表示名が空です。");
            if (choice.next < -1 || choice.next >= count) return Fail(error, std::to_string(i + 1) + " 行目の選択肢の行き先が範囲外です。");
        }
    }
    for (const auto *name : {&s.startedEvent, &s.lineEvent, &s.choiceEvent, &s.endedEvent}) if (!ValidEventName(*name)) return Fail(error, "イベント名が不正です。");
    return true;
}
bool Dialogue::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; End(); return true; }
std::unique_ptr<IObjectComponent> Dialogue::Clone() const { auto copy = std::make_unique<Dialogue>(); copy->settings_ = settings_; return copy; }
std::string Dialogue::GetDisplayText() const {
    if (!running_ || index_ < 0 || index_ >= static_cast<int>(settings_.lines.size())) return {};
    const auto &line = settings_.lines[index_];
    std::string text = line.speaker.empty() ? line.text : line.speaker + ": " + line.text;
    for (size_t i = 0; i < line.choices.size(); ++i) text += "\n" + std::to_string(i + 1) + ". " + line.choices[i].label;
    return text;
}
void Dialogue::Output(const std::string &text) {
    auto *scene = GetOwnerSceneContext();
    std::string error;
    EmptyObject *target = settings_.outputObject.empty() ? Owner(this) : Gameplay::ResolveObject(scene, settings_.outputObject, &error);
    if (!target) { lastError_ = error.empty() ? "表示先がありません。" : error; return; }
    if (auto *text2 = target->GetComponent<TextRenderer>()) { text2->SetText(text); return; }
    if (auto *bitmap = target->GetComponent<BitmapTextRenderer>()) { bitmap->SetText(text); return; }
    lastError_ = target->GetName() + " に TextRenderer / BitmapTextRenderer がありません（表示先を設定してください）。";
}
bool Dialogue::Show(int index) {
    if (index < 0 || index >= static_cast<int>(settings_.lines.size())) { End(); return true; }
    index_ = index;
    Output(GetDisplayText());
    std::string error;
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.lineEvent, GameEvents::Value{{"line", index_}, {"text", settings_.lines[index_].text}}, &error);
    return true;
}
bool Dialogue::Start(std::string *error) {
    if (settings_.lines.empty()) return Fail(error, "会話の文章が未入力です。Inspector で行を追加してください（文章は自動生成しません）。");
    running_ = true;
    std::string emitError;
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.startedEvent, GameEvents::Value::object(), &emitError);
    return Show(0);
}
bool Dialogue::Advance(std::string *error) {
    if (!running_) return Fail(error, "会話は開始されていません。");
    const auto &line = settings_.lines[index_];
    if (!line.choices.empty()) return Fail(error, "選択肢を選んでください。");
    return Show(line.next == -2 ? index_ + 1 : line.next);
}
bool Dialogue::Choose(int index, std::string *error) {
    if (!running_) return Fail(error, "会話は開始されていません。");
    const auto &line = settings_.lines[index_];
    if (index < 0 || index >= static_cast<int>(line.choices.size())) return Fail(error, "その選択肢はありません。");
    const auto choice = line.choices[index];
    std::string emitError;
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.choiceEvent,
        GameEvents::Value{{"line", index_}, {"choice", index}, {"label", choice.label}}, &emitError);
    return Show(choice.next);
}
void Dialogue::End() {
    const bool wasRunning = running_;
    running_ = false;
    index_ = -1;
    if (!wasRunning) return;
    Output("");
    std::string error;
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.endedEvent, GameEvents::Value::object(), &error);
}
void Dialogue::HandleInput(bool advancePressed, int choicePressed) {
    if (!running_) return;
    std::string error;
    if (choicePressed >= 0 && !settings_.lines[index_].choices.empty()) { if (!Choose(choicePressed, &error)) lastError_ = error; return; }
    if (advancePressed && settings_.lines[index_].choices.empty() && !Advance(&error)) lastError_ = error;
}
void Dialogue::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!Gameplay::IsPlaying(scene) || !running_) return;
    int choice = -1;
    if (settings_.choiceKeys) for (int i = 0; i < 4; ++i) if (KeyTriggered(scene, static_cast<Key>(static_cast<int>(Key::D1) + i))) { choice = i; break; }
    HandleInput(KeyTriggered(scene, settings_.advanceKey), choice);
}
bool Dialogue::ExecuteEventCommand(const std::string &command, const GameEvents::Value &args, const std::string &sender, std::string &error) {
    (void)sender;
    if (command == "start") return Start(&error);
    if (command == "advance") return Advance(&error);
    if (command == "end") { End(); return true; }
    if (command == "choose") {
        if (!args.is_object() || !args.contains("index") || !args["index"].is_number_integer()) { error = "index (整数) が必要です。"; return false; }
        return Choose(args["index"].get<int>(), &error);
    }
    error = "Dialogue に命令 " + command + " はありません。";
    return false;
}
std::vector<std::string> Dialogue::GetEventCommands() const { return {"start()", "advance()", "choose(index: 整数)", "end()"}; }
JSON Dialogue::SaveToJson() const { return SaveSettings<Dialogue>(settings_); }
bool Dialogue::LoadFromJson(const JSON &json) { if (!LoadSettings<Dialogue>(json, settings_)) return false; running_ = false; index_ = -1; return true; }
#if defined(USE_IMGUI)
void Dialogue::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    int removeLine = -1;
    for (size_t i = 0; i < next.lines.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto &line = next.lines[i];
        if (ImGui::TreeNode("line", "%zu: %s", i + 1, line.text.c_str())) {
            ImGui::InputText("話者", &line.speaker);
            ImGui::InputTextMultiline("文章", &line.text, ImVec2(-1, 48));
            ImGui::InputInt("次の行（-2: 次, -1: 終了, 0〜: 行番号-1）", &line.next);
            int removeChoice = -1;
            for (size_t c = 0; c < line.choices.size(); ++c) {
                ImGui::PushID(static_cast<int>(c));
                ImGui::SetNextItemWidth(160); ImGui::InputText("選択肢", &line.choices[c].label);
                ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::InputInt("行き先", &line.choices[c].next);
                ImGui::SameLine(); if (ImGui::SmallButton("削除")) removeChoice = static_cast<int>(c);
                ImGui::PopID();
            }
            if (removeChoice >= 0) line.choices.erase(line.choices.begin() + removeChoice);
            if (line.choices.size() < 4 && ImGui::Button("選択肢を追加")) line.choices.push_back({"選択肢", -1});
            if (ImGui::Button("この行を削除")) removeLine = static_cast<int>(i);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeLine >= 0) next.lines.erase(next.lines.begin() + removeLine);
    if (next.lines.size() < 256 && ImGui::Button("行を追加")) next.lines.push_back({});
    ImGui::InputText("表示先（空欄は自分、名前またはUUID）", &next.outputObject);
    KeyCombo("進めるキー", next.advanceKey);
    ImGui::Checkbox("1〜4キーで選択", &next.choiceKeys);
    EventInput("開始イベント", next.startedEvent);
    EventInput("行表示イベント", next.lineEvent);
    EventInput("選択イベント", next.choiceEvent);
    EventInput("終了イベント", next.endedEvent);
    ApplyEdit(*this, next, settings_);
    if (settings_.lines.empty()) ImGui::TextWrapped("必要な設定: 会話の文章を入力してください。");
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif

//==================================================
// AnimationStateSwitcher
//==================================================
template <class F> bool AnimationStateSwitcher::Bind(Settings &s, F &f) {
    JSON states = JSON::array();
    if (!f.Reading()) for (const auto &st : s.states)
        states.push_back({{"name", st.name}, {"minSpeed", st.minSpeed}, {"maxSpeed", st.maxSpeed}, {"priority", st.priority}, {"clip", st.clip}, {"player", st.player}});
    if (!f.Raw("states", states)) return false;
    if (f.Reading()) {
        if (!states.is_array() || states.size() > 16) return false;
        s.states.clear();
        for (const auto &j : states) {
            if (!j.is_object()) return false;
            State st;
            Gameplay::Fields sf(j);
            if (!sf.Text("name", st.name, 64) || !sf.Float("minSpeed", st.minSpeed, 0.0f, 1.0e6f) || !sf.Float("maxSpeed", st.maxSpeed, -1.0f, 1.0e6f) ||
                !sf.Int("priority", st.priority, -1000, 1000) || !sf.Text("clip", st.clip, 128) || !sf.Int("player", st.player, 0, 1)) return false;
            s.states.push_back(std::move(st));
        }
    }
    return f.EventName("transitionEvent", s.transitionEvent) && f.Bool("shortTextManaged", s.shortTextManaged) && f.Text("sourceText", s.sourceText, 512);
}
bool AnimationStateSwitcher::IsValid(const Settings &s, std::string *error) {
    if (s.states.size() > 16) return Fail(error, "状態は16個までです。");
    for (size_t i = 0; i < s.states.size(); ++i) {
        const auto &st = s.states[i];
        if (st.name.empty()) return Fail(error, "状態名が空です。");
        for (size_t k = 0; k < i; ++k) if (s.states[k].name == st.name) return Fail(error, "状態名「" + st.name + "」が重複しています。");
        if (!std::isfinite(st.minSpeed) || st.minSpeed < 0 || !std::isfinite(st.maxSpeed) || (st.maxSpeed >= 0 && st.maxSpeed < st.minSpeed) || (st.maxSpeed < 0 && st.maxSpeed != -1.0f))
            return Fail(error, "状態「" + st.name + "」の速度範囲が不正です。");
        if (st.player < 0 || st.player > 1) return Fail(error, "再生先が不正です。");
    }
    return ValidEventName(s.transitionEvent) || Fail(error, "イベント名が不正です。");
}
bool AnimationStateSwitcher::SetSettings(const Settings &settings) { if (!IsValid(settings)) return false; settings_ = settings; current_.clear(); return true; }
std::unique_ptr<IObjectComponent> AnimationStateSwitcher::Clone() const { auto copy = std::make_unique<AnimationStateSwitcher>(); copy->settings_ = settings_; return copy; }
bool AnimationStateSwitcher::Play(const State &state, std::string &error) {
    auto *owner = Owner(this);
    if (state.clip.empty()) { error = "状態「" + state.name + "」のクリップが未設定です。"; return false; }
    if (state.player == 0) {
        auto *keyframes = owner ? owner->GetComponent<KeyFrameAnimator>() : nullptr;
        if (!keyframes || !keyframes->HasAnimation(state.clip)) { error = "KeyFrameAnimator のアニメーション「" + state.clip + "」がありません。"; return false; }
        for (const auto &other : settings_.states) if (other.player == 0 && other.clip != state.clip && keyframes->HasAnimation(other.clip)) keyframes->Stop(other.clip);
        return keyframes->Play(state.clip) || (error = "「" + state.clip + "」を再生できませんでした。", false);
    }
    auto *animator = owner ? owner->GetComponent<Animator>() : nullptr;
    if (!animator) { error = "Animator がありません。"; return false; }
    animator->SetClipName(state.clip);
    animator->Play();
    return true;
}
bool AnimationStateSwitcher::Evaluate(float speed) {
    if (!std::isfinite(speed) || speed < 0) return false;
    const State *best = nullptr;
    for (const auto &st : settings_.states) {
        if (speed < st.minSpeed || (st.maxSpeed >= 0 && speed > st.maxSpeed)) continue;
        if (!best || st.priority > best->priority) best = &st;
    }
    if (!best || best->name == current_) return false; // the same state never restarts its clip
    std::string error;
    if (!Play(*best, error)) { lastError_ = error; return false; }
    const std::string from = current_;
    current_ = best->name;
    ++playCount_;
    lastError_.clear();
    Gameplay::EmitEvent(GetOwnerSceneContext(), Owner(this), settings_.transitionEvent,
        GameEvents::Value{{"from", from}, {"to", current_}, {"speed", speed}}, &error);
    return true;
}
bool AnimationStateSwitcher::Advance(double dt) {
    if (!ValidDt(dt)) return false;
    auto *owner = Owner(this);
    if (!owner) return false;
    float speed = 0;
    if (auto *velocity = owner->GetComponent<Velocity>()) {
        const Vector3 v = velocity->GetVelocity();
        speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    } else {
        const Vector3 p = Gameplay::WorldPosition(owner);
        if (hasLast_ && dt > 0) { const Vector3 d = p - last_; speed = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) / static_cast<float>(dt); }
        last_ = p; hasLast_ = true;
    }
    return Evaluate(speed);
}
void AnimationStateSwitcher::Update() { if (Gameplay::IsPlaying(GetOwnerSceneContext())) Advance(GetDeltaTime()); }
JSON AnimationStateSwitcher::SaveToJson() const { return SaveSettings<AnimationStateSwitcher>(settings_); }
bool AnimationStateSwitcher::LoadFromJson(const JSON &json) { if (!LoadSettings<AnimationStateSwitcher>(json, settings_)) return false; current_.clear(); hasLast_ = false; return true; }
#if defined(USE_IMGUI)
void AnimationStateSwitcher::ShowImGui() {
    ManagedHeader(settings_.shortTextManaged, settings_.sourceText);
    auto next = settings_;
    int remove = -1;
    for (size_t i = 0; i < next.states.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto &st = next.states[i];
        if (ImGui::TreeNode("state", "%s（優先度 %d）", st.name.c_str(), st.priority)) {
            ImGui::InputText("状態名", &st.name);
            ImGui::DragFloat("最小速度", &st.minSpeed, 0.01f, 0.0f, 1.0e6f);
            ImGui::DragFloat("最大速度（-1は上限なし）", &st.maxSpeed, 0.01f, -1.0f, 1.0e6f);
            ImGui::InputInt("優先度", &st.priority);
            ImGui::Combo("再生先", &st.player, "KeyFrameAnimator\0Animator\0");
            ImGui::InputText("クリップ / アニメーション名", &st.clip);
            if (ImGui::Button("この状態を削除")) remove = static_cast<int>(i);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (remove >= 0) next.states.erase(next.states.begin() + remove);
    if (next.states.size() < 16 && ImGui::Button("状態を追加")) next.states.push_back({"State" + std::to_string(next.states.size() + 1)});
    EventInput("遷移イベント", next.transitionEvent);
    ApplyEdit(*this, next, settings_);
    for (const auto &st : settings_.states) if (st.clip.empty()) ImGui::TextWrapped("必要な設定: 状態「%s」のクリップ名", st.name.c_str());
    ImGui::Text("現在の状態: %s", current_.empty() ? "（なし）" : current_.c_str());
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif
} // namespace KashipanEngine
