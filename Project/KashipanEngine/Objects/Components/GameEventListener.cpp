#include "Objects/Components/GameEventListener.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include "Input/Input.h"
#include "Input/Keyboard.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/GameplayComponents.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Scene/Components/GameEventRouter.h"
#include "Scene/SceneContext.h"
#include "Utilities/ShortTextProgram.h"
#include "Utilities/TimeUtils.h"
#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_stdlib.h>
#endif

namespace KashipanEngine {
namespace {
constexpr const char *kTriggerNames[] = {"event", "start", "contact", "key"};
constexpr const char *kActionNames[] = {"setProperty", "engineCommand", "emit", "callScript", "componentCommand"};
constexpr const char *kSourceNames[] = {"fixed", "payload", "sender", "event"};
constexpr const char *kOps[] = {"==", "!=", "<", "<=", ">", ">="};
bool Fail(std::string *error, const std::string &message) { if (error) *error = message; return false; }
template <size_t N> int IndexOf(const char *const (&names)[N], const std::string &text) {
    for (size_t i = 0; i < N; ++i) if (text == names[i]) return static_cast<int>(i);
    return -1;
}
bool ValidField(const std::string &text, size_t limit) {
    if (text.size() > limit) return false;
    for (unsigned char c : text) if (c < 32 || c == 127) return false;
    return true;
}
bool Compare(const GameEvents::Value &left, const std::string &op, const GameEvents::Value &right) {
    if (left.is_number() && right.is_number()) {
        const double a = left.get<double>(), b = right.get<double>();
        if (op == "==") return a == b;
        if (op == "!=") return a != b;
        if (op == "<") return a < b;
        if (op == "<=") return a <= b;
        if (op == ">") return a > b;
        if (op == ">=") return a >= b;
        return false;
    }
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    return false; // ordering needs numbers
}
std::string MessageText(const GameEvents::Message &m) { return m.name.empty() ? std::string("(trigger)") : m.name; }
}

//--------------------------------------------------
// Serialization
//--------------------------------------------------
JSON GameEventListener::EncodeHandler(const Handler &h) {
    JSON conditions = JSON::array();
    for (const auto &c : h.conditions) conditions.push_back({{"arg", c.arg}, {"op", c.op}, {"value", c.value}});
    JSON actions = JSON::array();
    for (const auto &a : h.actions) {
        JSON args = JSON::array();
        for (const auto &[name, source] : a.args) {
            JSON s{{"name", name}, {"from", kSourceNames[static_cast<int>(source.kind)]}};
            if (source.kind == SourceKind::Fixed) s["value"] = source.value;
            if (source.kind == SourceKind::Payload) s["arg"] = source.arg;
            args.push_back(std::move(s));
        }
        JSON action{{"kind", kActionNames[static_cast<int>(a.kind)]}, {"target", a.target}, {"name", a.name}, {"args", std::move(args)}};
        if (a.kind == ActionKind::Emit) { action["emitTarget"] = GameEvents::TargetModeNames[static_cast<int>(a.emitTarget)]; action["emitTargetValue"] = a.emitTargetValue; }
        if (a.kind == ActionKind::CallScript) action["passEvent"] = a.passEvent;
        actions.push_back(std::move(action));
    }
    return JSON{{"id", h.id}, {"trigger", kTriggerNames[static_cast<int>(h.trigger)]}, {"event", h.event}, {"contactTag", h.contactTag},
        {"contactName", h.contactName}, {"key", ShortText::KeyName(h.key)}, {"senderTag", h.senderTag}, {"conditions", std::move(conditions)},
        {"once", h.once}, {"cooldown", h.cooldown}, {"actions", std::move(actions)}, {"shortTextManaged", h.shortTextManaged}, {"sourceText", h.sourceText}};
}

bool GameEventListener::DecodeHandler(const JSON &j, Handler &out, std::string *error) {
    if (!j.is_object()) return Fail(error, "受信処理はオブジェクトです。");
    Handler h;
    auto text = [&](const JSON &o, const char *key, std::string &value, size_t limit) {
        if (!o.contains(key)) return true;
        if (!o[key].is_string() || !ValidField(o[key].get<std::string>(), limit)) return false;
        value = o[key].get<std::string>(); return true;
    };
    std::string trigger = "event", key = ShortText::KeyName(h.key);
    if (!text(j, "id", h.id, 64) || !text(j, "trigger", trigger, 16) || !text(j, "event", h.event, 64) || !text(j, "contactTag", h.contactTag, 64) ||
        !text(j, "contactName", h.contactName, 128) || !text(j, "key", key, 16) || !text(j, "senderTag", h.senderTag, 64) || !text(j, "sourceText", h.sourceText, 512))
        return Fail(error, "受信処理の文字列項目が不正です。");
    const int t = IndexOf(kTriggerNames, trigger);
    if (t < 0) return Fail(error, "未知のきっかけ: " + trigger);
    h.trigger = static_cast<Trigger>(t);
    h.key = ShortText::ParseKey(key);
    if (j.contains("once")) { if (!j["once"].is_boolean()) return Fail(error, "once は真偽値です。"); h.once = j["once"].get<bool>(); }
    if (j.contains("shortTextManaged")) { if (!j["shortTextManaged"].is_boolean()) return Fail(error, "shortTextManaged は真偽値です。"); h.shortTextManaged = j["shortTextManaged"].get<bool>(); }
    if (j.contains("cooldown")) {
        if (!j["cooldown"].is_number() || !std::isfinite(j["cooldown"].get<double>()) || j["cooldown"].get<double>() < 0 || j["cooldown"].get<double>() > 3600) return Fail(error, "クールダウンは 0〜3600 秒です。");
        h.cooldown = j["cooldown"].get<float>();
    }
    if (j.contains("conditions")) {
        if (!j["conditions"].is_array() || j["conditions"].size() > 4) return Fail(error, "条件は4つまでです。");
        for (const auto &c : j["conditions"]) {
            Condition condition;
            if (!c.is_object() || !text(c, "arg", condition.arg, 64) || !text(c, "op", condition.op, 2) || IndexOf(kOps, condition.op) < 0) return Fail(error, "条件が不正です。");
            if (c.contains("value")) condition.value = c["value"];
            h.conditions.push_back(std::move(condition));
        }
    }
    if (j.contains("actions")) {
        if (!j["actions"].is_array() || j["actions"].size() > kMaxActions) return Fail(error, "動作は8個までです。");
        for (const auto &a : j["actions"]) {
            Action action;
            std::string kind = "engineCommand", emitTarget = "broadcast";
            if (!a.is_object() || !text(a, "kind", kind, 32) || !text(a, "target", action.target, 128) || !text(a, "name", action.name, 128) ||
                !text(a, "emitTarget", emitTarget, 16) || !text(a, "emitTargetValue", action.emitTargetValue, 128)) return Fail(error, "動作の文字列項目が不正です。");
            const int k = IndexOf(kActionNames, kind);
            if (k < 0) return Fail(error, "未知の動作: " + kind);
            action.kind = static_cast<ActionKind>(k);
            if (!GameEvents::ParseTargetMode(emitTarget, action.emitTarget)) return Fail(error, "未知の送信先: " + emitTarget);
            if (a.contains("passEvent")) { if (!a["passEvent"].is_boolean()) return Fail(error, "passEvent は真偽値です。"); action.passEvent = a["passEvent"].get<bool>(); }
            if (a.contains("args")) {
                if (!a["args"].is_array() || a["args"].size() > 16) return Fail(error, "引数は16個までです。");
                for (const auto &s : a["args"]) {
                    std::string name, from = "fixed";
                    ValueSource source;
                    if (!s.is_object() || !text(s, "name", name, 64) || name.empty() || !text(s, "from", from, 16) || !text(s, "arg", source.arg, 64)) return Fail(error, "引数の指定が不正です。");
                    const int f = IndexOf(kSourceNames, from);
                    if (f < 0) return Fail(error, "未知の値の取得元: " + from);
                    source.kind = static_cast<SourceKind>(f);
                    if (s.contains("value")) source.value = s["value"];
                    if (source.kind == SourceKind::Fixed && (source.value.is_null() || source.value.dump().size() > GameEvents::MaxPayloadBytes)) return Fail(error, "固定値「" + name + "」がありません。");
                    if (source.kind == SourceKind::Payload && source.arg.empty()) return Fail(error, "引数「" + name + "」の受信引数名がありません。");
                    action.args.emplace_back(name, std::move(source));
                }
            }
            h.actions.push_back(std::move(action));
        }
    }
    out = std::move(h);
    return true;
}

JSON GameEventListener::EncodeSettings(const Settings &s) {
    JSON handlers = JSON::array();
    for (const auto &h : s.handlers) handlers.push_back(EncodeHandler(h));
    return JSON{{"handlers", std::move(handlers)}};
}

bool GameEventListener::DecodeSettings(const JSON &json, Settings &out, std::string *error) {
    if (!json.is_object()) return Fail(error, "設定はオブジェクトです。");
    Settings s;
    if (json.contains("handlers")) {
        if (!json["handlers"].is_array() || json["handlers"].size() > kMaxHandlers) return Fail(error, "受信処理は32個までです。");
        for (const auto &h : json["handlers"]) {
            Handler handler;
            if (!DecodeHandler(h, handler, error)) return false;
            s.handlers.push_back(std::move(handler));
        }
    }
    if (!IsValid(s, error)) return false;
    out = std::move(s);
    return true;
}

bool GameEventListener::IsValid(const Settings &s, std::string *error) {
    if (s.handlers.size() > kMaxHandlers) return Fail(error, "受信処理は32個までです。");
    std::set<std::string> ids;
    for (const auto &h : s.handlers) {
        if (h.id.empty() || !ids.insert(h.id).second) return Fail(error, "受信処理のIDが空か重複しています: " + h.id);
        if (h.trigger == Trigger::Event && !GameEvents::IsValidName(h.event, error)) return false;
        if (h.trigger == Trigger::Contact && h.contactTag.empty() && h.contactName.empty()) return Fail(error, "接触の相手（タグか名前）を指定してください。");
        if (h.trigger == Trigger::Key && ShortText::KeyName(h.key).empty()) return Fail(error, "キーを指定してください。");
        if (!std::isfinite(h.cooldown) || h.cooldown < 0 || h.cooldown > 3600) return Fail(error, "クールダウンは 0〜3600 秒です。");
        if (h.actions.empty() || h.actions.size() > kMaxActions) return Fail(error, "受信処理「" + h.id + "」の動作は1〜8個です。");
        for (const auto &a : h.actions) {
            if (a.name.empty()) return Fail(error, "動作の名前（プロパティ・命令・イベント・関数）がありません。");
            if (a.kind == ActionKind::Emit) {
                if (!GameEvents::IsValidName(a.name, error)) return false;
                if (a.emitTarget != GameEvents::TargetMode::Broadcast && a.emitTargetValue.empty()) return Fail(error, "送信先を指定してください。");
            }
            if (a.kind == ActionKind::SetProperty && (a.args.size() != 1 || a.args.front().first != "value")) return Fail(error, "プロパティ設定には引数 value がひとつ必要です。");
            if (a.kind == ActionKind::CallScript && !a.passEvent && a.args.size() > 4) return Fail(error, "関数の引数は4個までです。");
            if (a.kind == ActionKind::ComponentCommand && a.name.find('.') == std::string::npos) return Fail(error, "部品の命令は「型.命令」で指定します（例: Health.damage）。");
            if (a.kind == ActionKind::EngineCommand && IsContinuousCommand(a.name))
                return Fail(error, "実行部品「" + a.name + "」は毎フレームの継続実行が前提のため、イベントの受信処理では使えません（短文の挙動やスクリプトで使ってください）。");
        }
    }
    return true;
}

bool GameEventListener::IsContinuousCommand(const std::string &name) {
    for (const char *command : {"accelerated_move", "waypoint_move", "property.sync", "property.unsync"})
        if (name == command || name.ends_with(std::string(".") + command)) return true;
    return false;
}

//--------------------------------------------------
// Component
//--------------------------------------------------
std::unique_ptr<IObjectComponent> GameEventListener::Clone() const {
    auto copy = std::make_unique<GameEventListener>();
    copy->settings_ = settings_;
    return copy;
}
bool GameEventListener::SetSettings(const Settings &settings) {
    if (!IsValid(settings)) return false;
    settings_ = settings;
    ++revision_;
    ResetRuntime();
    return true;
}
const GameEventListener::Handler *GameEventListener::FindHandler(const std::string &id) const {
    for (const auto &h : settings_.handlers) if (h.id == id) return &h;
    return nullptr;
}
int GameEventListener::GetFireCount(size_t index) const { return index < fired_.size() ? fired_[index] : 0; }
void GameEventListener::ResetRuntime() {
    started_ = false;
    fired_.assign(settings_.handlers.size(), 0);
    cooldowns_.assign(settings_.handlers.size(), 0.0f);
    contacts_.assign(settings_.handlers.size(), {});
    memories_.assign(settings_.handlers.size(), {});
    subscribedRevision_ = 0;
}
void GameEventListener::Initialize() { ResetRuntime(); lastError_.clear(); subscribedGeneration_ = 0; }
void GameEventListener::Finalize() {
    if (auto *router = GameEventRouter::Find(GetOwnerSceneContext())) router->UnsubscribeOwner(GetComponentRef());
    subscribedGeneration_ = 0;
    subscribedRevision_ = 0;
}

void GameEventListener::Resubscribe() {
    auto *router = GameEventRouter::Find(GetOwnerSceneContext());
    subscribedGeneration_ = router ? router->GetGeneration() : 0;
    subscribedRevision_ = revision_;
    if (!router) {
        for (const auto &h : settings_.handlers) if (h.trigger == Trigger::Event) { lastError_ = "シーンに GameEventRouter がありません（イベントを受け取れません）。"; break; }
        return;
    }
    const auto self = GetComponentRef();
    router->UnsubscribeOwner(self);
    const auto revision = revision_;
    for (size_t i = 0; i < settings_.handlers.size(); ++i) {
        const auto &h = settings_.handlers[i];
        if (h.trigger != Trigger::Event) continue;
        if (!router->FindDefinition(h.event)) lastError_ = "イベント「" + h.event + "」はシーンに定義されていません。";
        router->Subscribe(h.event, self, [i, revision](IObjectComponent *receiver, const GameEvents::Message &message) {
            auto *listener = dynamic_cast<GameEventListener *>(receiver);
            if (listener && listener->revision_ == revision) listener->Fire(i, message);
        }, "listener:" + h.id);
    }
}

bool GameEventListener::Resolve(const ValueSource &source, const GameEvents::Message &message, GameEvents::Value &out, std::string &error) const {
    switch (source.kind) {
    case SourceKind::Fixed: out = source.value; return true;
    case SourceKind::Sender: out = message.sender; return true;
    case SourceKind::EventName: out = message.name; return true;
    case SourceKind::Payload:
        if (!message.Has(source.arg)) { error = "受信した引数に「" + source.arg + "」がありません。"; return false; }
        out = message.payload[source.arg];
        return true;
    }
    return false;
}

bool GameEventListener::RunAction(size_t handlerIndex, const Action &action, const GameEvents::Message &message, std::string &error) {
    auto *scene = GetOwnerSceneContext();
    auto *owner = const_cast<EmptyObject *>(GetOwnerObject());
    EmptyObject *target = owner;
    if (action.target == "$sender") {
        target = scene && !message.sender.empty() ? scene->GetSceneObject(UUID128(message.sender)) : nullptr;
        if (!target) { error = "送信元のオブジェクトがありません。"; return false; }
    } else if (!action.target.empty()) {
        target = Gameplay::ResolveObject(scene, action.target, &error);
        if (!target) return false;
    }
    GameEvents::Value args = GameEvents::Value::object();
    std::vector<GameEvents::Value> ordered;
    for (const auto &[name, source] : action.args) {
        GameEvents::Value value;
        if (!Resolve(source, message, value, error)) return false;
        args[name] = value;
        ordered.push_back(std::move(value));
    }
    switch (action.kind) {
    case ActionKind::SetProperty:
        if (!ShortText::WriteExtendedProperty(target, action.name, args["value"])) { error = action.name + " へ書き込めませんでした（型・範囲を確認してください）。"; return false; }
        return true;
    case ActionKind::EngineCommand: {
        ShortText::Action command;
        command.operation = ShortText::Operation::EngineCommand;
        command.name = action.name;
        command.data = args;
        if (IsContinuousCommand(action.name)) { error = action.name + " はイベントの受信処理では使えません。"; return false; }
        ShortText::CommandContext context;
        context.owner = owner; context.target = target; context.scene = scene;
        context.memory = handlerIndex < memories_.size() ? &memories_[handlerIndex] : nullptr;
        context.dt = lastDt_;
        return ShortText::ApplyExtendedCommand(command, context, error);
    }
    case ActionKind::Emit: {
        auto *router = GameEventRouter::Find(scene);
        if (!router) { error = "シーンに GameEventRouter がありません。"; return false; }
        std::string targetValue = action.emitTargetValue == "$sender" ? message.sender : action.emitTargetValue;
        if (action.emitTarget == GameEvents::TargetMode::Object && targetValue != message.sender) {
            // Names written by hand resolve to a stable UUID at send time (duplicates are rejected).
            if (!UUID128(targetValue).IsValid()) {
                auto *object = Gameplay::ResolveObject(scene, targetValue, &error);
                if (!object) return false;
                targetValue = Gameplay::IdOf(object);
            }
        }
        return router->Emit(action.name, args, Gameplay::IdOf(owner), action.emitTarget, targetValue, &error);
    }
    case ActionKind::CallScript: {
        auto *script = target ? target->GetComponent<ScriptComponent>() : nullptr;
        if (!script) { error = "ScriptComponent がありません。"; return false; }
        // The script may delete itself, its object or this listener: re-resolve before reading its error.
        const auto scriptRef = script->GetComponentRef();
        const std::weak_ptr<void> sceneAlive = scene ? scene->GetLifetimeToken() : std::weak_ptr<void>();
        const bool ok = action.passEvent ? script->InvokeEventHandler(action.name, message) : script->InvokeMethodWithValues(action.name, ordered);
        if (!scene || sceneAlive.expired() || scene->ResolveComponent(scriptRef) != script) return ok;
        if (!ok) { error = "関数「" + action.name + "」を呼べませんでした（名前・引数の型と数を確認してください）。"; return false; }
        if (!script->GetLastError().empty()) { error = "スクリプトのエラー: " + script->GetLastError(); return false; }
        return true;
    }
    case ActionKind::ComponentCommand: {
        const auto dot = action.name.find('.');
        const auto type = action.name.substr(0, dot), command = action.name.substr(dot + 1);
        for (const auto &[component, addedID] : target->GetAllComponents()) {
            (void)addedID;
            if (!component || component->GetComponentType() != type) continue;
            auto *commandTarget = dynamic_cast<IGameEventCommandTarget *>(component);
            if (!commandTarget) { error = type + " は命令を受け付けません。"; return false; }
            return commandTarget->ExecuteEventCommand(command, args, message.sender, error);
        }
        error = target->GetName() + " に " + type + " がありません。";
        return false;
    }
    }
    return false;
}

bool GameEventListener::Fire(size_t index, const GameEvents::Message &message) {
    if (index >= settings_.handlers.size()) return false;
    if (fired_.size() != settings_.handlers.size()) ResetRuntime();
    // Copy: an action may edit this listener's settings (via SetProperty / script) while it runs.
    const Handler handler = settings_.handlers[index];
    const auto revision = revision_;
    if (handler.once && fired_[index] > 0) return false;
    if (cooldowns_[index] > 0) return false;
    auto *scene = GetOwnerSceneContext();
    if (!handler.senderTag.empty()) {
        auto *sender = scene && !message.sender.empty() ? scene->GetSceneObject(UUID128(message.sender)) : nullptr;
        if (!sender || sender->GetTagName() != handler.senderTag) return false;
    }
    for (const auto &condition : handler.conditions) {
        if (!message.Has(condition.arg) || !Compare(message.payload[condition.arg], condition.op, condition.value)) return false;
    }
    ++fired_[index];
    cooldowns_[index] = handler.cooldown;
    const std::weak_ptr<void> sceneAlive = scene ? scene->GetLifetimeToken() : std::weak_ptr<void>();
    const auto self = GetComponentRef();
    // Deletions made by actions (scripts deleting themselves, Pickup-like removals) are destroyed after the handler.
    struct DeferScope {
        SceneContext *scene; std::weak_ptr<void> alive; bool previous;
        ~DeferScope() { if (scene && !alive.expired()) scene->EndDeferredDestroy(previous); }
    } defer{scene, sceneAlive, scene ? scene->BeginDeferredDestroy() : true};
    for (const auto &action : handler.actions) {
        std::string error;
        bool ok = false;
        try { ok = RunAction(index, action, message, error); }
        catch (const std::exception &e) { error = e.what(); }
        // The action may have deleted this object: re-resolve before touching members again.
        if (!scene || sceneAlive.expired() || scene->ResolveComponent(self) != this) return true;
        if (!ok) {
            lastError_ = "「" + handler.id + "」(" + MessageText(message) + ") の動作 " + action.name + ": " + error;
            if (auto *router = GameEventRouter::Find(scene)) router->AddDiagnostic(lastError_);
            break;
        }
        if (revision != revision_) break; // settings were replaced by the action
    }
    return true;
}

void GameEventListener::NotifyKey(Key key) {
    for (size_t i = 0; i < settings_.handlers.size(); ++i) {
        if (settings_.handlers[i].trigger != Trigger::Key || settings_.handlers[i].key != key) continue;
        GameEvents::Message message;
        message.payload = GameEvents::Value{{"key", ShortText::KeyName(key)}};
        Fire(i, message);
    }
}

void GameEventListener::Advance(double dt) {
    auto *scene = GetOwnerSceneContext();
    if (!scene) return;
    if (fired_.size() != settings_.handlers.size()) ResetRuntime();
    auto *router = GameEventRouter::Find(scene);
    if (subscribedRevision_ != revision_ || subscribedGeneration_ != (router ? router->GetGeneration() : 0)) Resubscribe();
    lastDt_ = std::isfinite(dt) && dt > 0 && dt <= 10 ? dt : 0;
    if (lastDt_ > 0) for (auto &cooldown : cooldowns_) cooldown = std::max(0.0f, cooldown - static_cast<float>(lastDt_));
    const auto revision = revision_;
    const auto self = GetComponentRef();
    auto alive = [&]() { return scene->ResolveComponent(self) == this && revision == revision_; };
    if (!started_) {
        started_ = true;
        for (size_t i = 0; i < settings_.handlers.size(); ++i) {
            if (settings_.handlers[i].trigger != Trigger::Start) continue;
            Fire(i, GameEvents::Message{});
            if (!alive()) return;
        }
    }
    bool hasContact = false;
    for (const auto &h : settings_.handlers) hasContact |= h.trigger == Trigger::Contact;
    if (!hasContact) return;
    const auto partners = Gameplay::QueryContacts(GetOwnerObject(), scene);
    std::vector<std::pair<size_t, std::string>> entered;
    for (size_t i = 0; i < settings_.handlers.size(); ++i) {
        const auto &h = settings_.handlers[i];
        if (h.trigger != Trigger::Contact) continue;
        std::vector<std::string> now;
        for (auto *other : partners) {
            if ((!h.contactTag.empty() && other->GetTagName() != h.contactTag) || (!h.contactName.empty() && other->GetName() != h.contactName)) continue;
            const auto id = Gameplay::IdOf(other);
            now.push_back(id);
            if (std::find(contacts_[i].begin(), contacts_[i].end(), id) == contacts_[i].end()) entered.emplace_back(i, id);
        }
        contacts_[i] = std::move(now);
    }
    for (const auto &[index, id] : entered) {
        GameEvents::Message message;
        message.sender = id;
        message.payload = GameEvents::Value{{"other", id}};
        Fire(index, message);
        if (!alive()) return;
    }
}

void GameEventListener::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!Gameplay::IsPlaying(scene)) { started_ = false; return; }
    Advance(GetDeltaTime());
    if (auto *input = scene->GetInput()) {
        std::set<Key> keys;
        for (const auto &h : settings_.handlers) if (h.trigger == Trigger::Key) keys.insert(h.key);
        for (auto key : keys) if (input->GetKeyboard().IsTrigger(key)) NotifyKey(key);
    }
}

JSON GameEventListener::SaveToJson() const { return EncodeSettings(settings_); }
bool GameEventListener::LoadFromJson(const JSON &json) {
    Settings next;
    if (!DecodeSettings(json, next)) return false;
    settings_ = std::move(next);
    ++revision_;
    ResetRuntime();
    return true;
}

std::string GameEventListener::DescribeHandler(const Handler &h) {
    std::ostringstream out;
    switch (h.trigger) {
    case Trigger::Event: out << "「" << h.event << "」を受け取ったら"; break;
    case Trigger::Start: out << "再生開始時に"; break;
    case Trigger::Contact: out << (h.contactName.empty() ? "タグ " + h.contactTag : h.contactName) << " に触れたら"; break;
    case Trigger::Key: out << ShortText::KeyName(h.key) << " キーで"; break;
    }
    for (size_t i = 0; i < h.actions.size(); ++i) {
        const auto &a = h.actions[i];
        out << (i ? "、" : " ") << kActionNames[static_cast<int>(a.kind)] << " " << a.name;
        if (!a.target.empty()) out << " @" << a.target;
        if (a.kind == ActionKind::Emit && a.emitTarget != GameEvents::TargetMode::Broadcast) out << " → " << GameEvents::TargetModeNames[static_cast<int>(a.emitTarget)] << ":" << a.emitTargetValue;
        if (!a.args.empty()) {
            out << "(";
            for (size_t k = 0; k < a.args.size(); ++k) {
                const auto &[name, source] = a.args[k];
                out << (k ? ", " : "") << name << "=";
                if (source.kind == SourceKind::Fixed) out << GameEvents::DescribeValue(source.value);
                else if (source.kind == SourceKind::Payload) out << "受信." << source.arg;
                else out << kSourceNames[static_cast<int>(source.kind)];
            }
            out << ")";
        }
    }
    return out.str();
}

#if defined(USE_IMGUI)
void GameEventListener::ShowImGui() {
    auto next = settings_;
    auto *router = GameEventRouter::Find(GetOwnerSceneContext());
    if (!router) ImGui::TextWrapped("シーンに GameEventRouter がありません。イベントの受信には Scene のコンポーネントとして追加してください。");
    int removeHandler = -1;
    for (size_t i = 0; i < next.handlers.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto &h = next.handlers[i];
        if (ImGui::TreeNode("handler", "%s%s", DescribeHandler(h).c_str(), h.shortTextManaged ? "（短文）" : "")) {
            if (h.shortTextManaged) ImGui::TextWrapped("短文から作成: %s", h.sourceText.c_str());
            ImGui::InputText("ID", &h.id);
            int trigger = static_cast<int>(h.trigger);
            if (ImGui::Combo("きっかけ", &trigger, "イベント受信\0再生開始\0接触\0キー\0")) h.trigger = static_cast<Trigger>(trigger);
            if (h.trigger == Trigger::Event) {
                if (ImGui::BeginCombo("イベント", h.event.c_str())) {
                    if (router) for (const auto &d : router->GetDefinitions()) if (ImGui::Selectable(d.name.c_str(), d.name == h.event)) h.event = d.name;
                    ImGui::EndCombo();
                }
                ImGui::InputText("送信元タグの条件（空欄は全て）", &h.senderTag);
                if (router && !h.event.empty()) {
                    if (const auto *d = router->FindDefinition(h.event)) ImGui::TextDisabled("引数: %s", GameEvents::DescribeDefinition(*d).c_str());
                    else ImGui::TextWrapped("このイベントはシーンに定義されていません。");
                }
            } else if (h.trigger == Trigger::Contact) {
                ImGui::InputText("相手のタグ", &h.contactTag);
                ImGui::InputText("相手の名前", &h.contactName);
            } else if (h.trigger == Trigger::Key) {
                std::string key = ShortText::KeyName(h.key);
                if (ImGui::InputText("キー（A〜Z, 0〜9, Space, Enter など）", &key, ImGuiInputTextFlags_EnterReturnsTrue) && ShortText::ParseKey(key) != Key::Unknown) h.key = ShortText::ParseKey(key);
            }
            ImGui::Checkbox("一度だけ", &h.once);
            ImGui::DragFloat("クールダウン (秒)", &h.cooldown, 0.01f, 0.0f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            int removeCondition = -1;
            for (size_t c = 0; c < h.conditions.size(); ++c) {
                ImGui::PushID(static_cast<int>(c) + 1000);
                auto &condition = h.conditions[c];
                ImGui::SetNextItemWidth(100); ImGui::InputText("##arg", &condition.arg);
                ImGui::SameLine(); ImGui::SetNextItemWidth(60);
                int op = std::max(0, IndexOf(kOps, condition.op));
                if (ImGui::Combo("##op", &op, kOps, 6)) condition.op = kOps[op];
                ImGui::SameLine(); ImGui::SetNextItemWidth(100);
                std::string value = condition.value.dump();
                if (ImGui::InputText("##value", &value, ImGuiInputTextFlags_EnterReturnsTrue)) { try { condition.value = GameEvents::Value::parse(value); } catch (const std::exception &) {} }
                ImGui::SameLine(); if (ImGui::SmallButton("条件を削除")) removeCondition = static_cast<int>(c);
                ImGui::PopID();
            }
            if (removeCondition >= 0) h.conditions.erase(h.conditions.begin() + removeCondition);
            if (h.conditions.size() < 4 && ImGui::Button("受信引数の条件を追加")) h.conditions.push_back({"amount", ">", 0});
            int removeAction = -1;
            for (size_t a = 0; a < h.actions.size(); ++a) {
                ImGui::PushID(static_cast<int>(a) + 2000);
                auto &action = h.actions[a];
                ImGui::SeparatorText(("動作 " + std::to_string(a + 1)).c_str());
                int kind = static_cast<int>(action.kind);
                if (ImGui::Combo("種類", &kind, "プロパティを設定\0実行部品（keyframe.play など）\0イベントを送信\0スクリプト関数を呼ぶ\0部品の命令（Health.damage など）\0")) action.kind = static_cast<ActionKind>(kind);
                ImGui::InputText("対象（空欄は自分 / 名前 / UUID / $sender）", &action.target);
                ImGui::InputText("名前（パス / 命令 / イベント / 関数）", &action.name);
                if (action.kind == ActionKind::Emit) {
                    int mode = static_cast<int>(action.emitTarget);
                    if (ImGui::Combo("送信先", &mode, "シーン全体\0オブジェクト\0名前\0タグ\0")) action.emitTarget = static_cast<GameEvents::TargetMode>(mode);
                    if (action.emitTarget != GameEvents::TargetMode::Broadcast) ImGui::InputText("送信先の値（$sender 可）", &action.emitTargetValue);
                }
                if (action.kind == ActionKind::CallScript) ImGui::Checkbox("void 関数(GameEvent@) として呼ぶ", &action.passEvent);
                int removeArg = -1;
                for (size_t k = 0; k < action.args.size(); ++k) {
                    ImGui::PushID(static_cast<int>(k) + 3000);
                    auto &[name, source] = action.args[k];
                    ImGui::SetNextItemWidth(90); ImGui::InputText("##name", &name);
                    ImGui::SameLine(); ImGui::SetNextItemWidth(110);
                    int from = static_cast<int>(source.kind);
                    if (ImGui::Combo("##from", &from, "固定値\0受信した引数\0送信元UUID\0イベント名\0")) source.kind = static_cast<SourceKind>(from);
                    ImGui::SameLine(); ImGui::SetNextItemWidth(120);
                    if (source.kind == SourceKind::Fixed) {
                        std::string value = source.value.dump();
                        if (ImGui::InputText("##value", &value, ImGuiInputTextFlags_EnterReturnsTrue)) { try { source.value = GameEvents::Value::parse(value); } catch (const std::exception &) {} }
                    } else if (source.kind == SourceKind::Payload) ImGui::InputText("##arg", &source.arg);
                    ImGui::SameLine(); if (ImGui::SmallButton("削除")) removeArg = static_cast<int>(k);
                    ImGui::PopID();
                }
                if (removeArg >= 0) action.args.erase(action.args.begin() + removeArg);
                if (action.args.size() < 16 && ImGui::Button("引数を追加")) action.args.emplace_back(action.kind == ActionKind::SetProperty ? "value" : "arg", ValueSource{SourceKind::Fixed, 0, ""});
                if (h.actions.size() > 1 && ImGui::Button("この動作を削除")) removeAction = static_cast<int>(a);
                ImGui::PopID();
            }
            if (removeAction >= 0) h.actions.erase(h.actions.begin() + removeAction);
            if (h.actions.size() < kMaxActions && ImGui::Button("動作を追加")) h.actions.push_back({ActionKind::EngineCommand, "", "keyframe.play"});
            if (ImGui::Button("この受信処理を削除")) removeHandler = static_cast<int>(i);
            ImGui::Text("実行回数: %d", GetFireCount(i));
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeHandler >= 0) next.handlers.erase(next.handlers.begin() + removeHandler);
    if (next.handlers.size() < kMaxHandlers && ImGui::Button("受信処理を追加")) {
        Handler h;
        for (int n = 1;; ++n) { h.id = "handler" + std::to_string(n); bool used = false; for (const auto &e : next.handlers) used |= e.id == h.id; if (!used) break; }
        if (router && !router->GetDefinitions().empty()) h.event = router->GetDefinitions().front().name; else h.event = "Event1";
        h.actions.push_back({ActionKind::EngineCommand, "", "keyframe.play", {{"name", ValueSource{SourceKind::Fixed, "", ""}}}});
        next.handlers.push_back(std::move(h));
    }
    if (EncodeSettings(next) != EncodeSettings(settings_)) {
        std::string error;
        if (!IsValid(next, &error) || !SetSettings(next)) ImGui::TextWrapped("反映していません: %s", error.c_str());
    }
    ImGui::TextWrapped("部品の命令の例: Health.damage(amount) / Inventory.add(item, amount) / Dialogue.start() / Spawner.spawn() / Respawner.respawn()");
    if (!lastError_.empty()) ImGui::TextWrapped("直近のエラー: %s", lastError_.c_str());
}
#endif
} // namespace KashipanEngine
