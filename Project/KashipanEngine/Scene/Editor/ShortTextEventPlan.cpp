#include "Scene/Editor/ShortTextEventPlan.h"
#ifdef USE_IMGUI
#include <cstdint>
#include <cstdio>
#include <set>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/Components/Animator.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/GameEventListener.h"
#include "Objects/Components/GameplayComponents.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Render/BitmapTextRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/Components/GameEventRouter.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Utilities/ShortTextProgram.h"

namespace KashipanEngine {

//==================================================
// GameEventDefinitionsCommand
//==================================================
bool GameEventDefinitionsCommand::Execute(SceneEditorContext *context) {
    SetAppliedChangesAfterFailure(false);
    if (!context) return false;
    std::vector<GameEvents::Definition> definitions;
    if (!GameEvents::DecodeDefinitions(after_, definitions)) return false;
    auto *router = context->GetComponent<GameEventRouter>();
    added_ = false;
    if (!router) {
        if (!addRouter_) return false;
        router = context->AddComponent<GameEventRouter>();
        if (!router) return false;
        added_ = true;
    }
    if (!router->SetDefinitions(definitions)) {
        if (added_ && !context->RemoveComponent(router)) SetAppliedChangesAfterFailure(true);
        added_ = false;
        return false;
    }
    return true;
}

bool GameEventDefinitionsCommand::Undo(SceneEditorContext *context) {
    auto *router = context ? context->GetComponent<GameEventRouter>() : nullptr;
    if (!router) return false;
    if (added_) {
        if (!context->RemoveComponent(router)) return false;
        added_ = false;
        return true;
    }
    std::vector<GameEvents::Definition> definitions;
    return GameEvents::DecodeDefinitions(before_, definitions) && router->SetDefinitions(definitions);
}

namespace {
using ShortText::EventRequestKind;
using ShortText::EventActionKind;
using ShortText::EventTrigger;

std::string Hash(const std::string &text) {
    std::uint32_t h = 2166136261u; // FNV-1a: stable across runs, so a re-application finds its handler
    for (unsigned char c : text) { h ^= c; h *= 16777619u; }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08x", h);
    return buffer;
}
std::string Number(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}
IObjectComponent *FindOfType(const EmptyObject *object, const std::string &type, bool managedOnly = false) {
    if (!object) return nullptr;
    for (const auto &[component, addedID] : object->GetAllComponents()) {
        (void)addedID;
        if (!component || component->GetComponentType() != type) continue;
        if (managedOnly) {
            const bool managed = IsShortTextManagedGameplay(component);
            if (!managed) continue;
        }
        return component;
    }
    return nullptr;
}
bool HasCollider(const EmptyObject *object) {
    if (!object) return false;
    for (const auto &[component, addedID] : object->GetAllComponents()) { (void)addedID; if (dynamic_cast<ICollider *>(component)) return true; }
    return false;
}

struct Builder {
    SceneEditorContext *context = nullptr;
    EmptyObject *target = nullptr;
    std::string source;
    ShortTextComponentPlan &plan;
    std::unique_ptr<CompositeCommand> group = std::make_unique<CompositeCommand>("短文からイベント・部品を設定");
    std::vector<GameEvents::Definition> definitionsBefore;
    std::vector<GameEvents::Definition> definitionsAfter;
    bool hasRouter = false;
    bool needRouter = false;
    std::string error;

    bool Fail(const std::string &message) { error = message; return false; }
    void Step(const std::string &text) { plan.steps.push_back(text); }
    void Require(const std::string &text) {
        for (const auto &existing : plan.requirements) if (existing == text) return;
        plan.requirements.push_back(text);
    }
    const GameEvents::Definition *Definition(const std::string &name) const { return GameEvents::Find(definitionsAfter, name); }

    std::vector<EmptyObject *> ByTag(const std::string &tag) const {
        std::vector<EmptyObject *> list;
        for (auto *object : context->GetSceneObjects()) if (object && object->GetTagName() == tag) list.push_back(object);
        return list;
    }
    EmptyObject *UniqueByName(const std::string &name, std::string &why) const {
        const auto list = context->GetSceneObjects(name);
        if (list.size() == 1) return list.front();
        why = list.empty() ? "「" + name + "」という名前のオブジェクトがシーンにありません。" :
            "「" + name + "」という名前のオブジェクトが " + std::to_string(list.size()) + " 個あります。名前を一意にするか、タグで指定してください。";
        return nullptr;
    }
    // Missing contact prerequisites are reported, never guessed (collider shapes are unknown).
    void NoteContact(EmptyObject *self, const std::vector<EmptyObject *> &others, const std::string &othersText) {
        if (!context->GetComponent<SceneObjectCollider>()) Require("接触の判定にはシーンに SceneObjectCollider が必要です（自動では追加しません）。");
        if (!HasCollider(self)) Require(self->GetName() + " に Collider が必要です（形状は推測しないため Inspector で追加してください）。");
        if (others.empty()) Require(othersText + " に当たるオブジェクトがシーンにまだありません。");
        for (auto *other : others) if (!HasCollider(other)) Require(other->GetName() + " に Collider が必要です。");
    }

    static JSON NewState(const std::string &type, const JSON &customData) {
        const auto detached = CreateObjectComponentByType(type);
        return JSON{{"priority", detached ? detached->GetUpdatePriority() : 1}, {"isActive", true}, {"tag", ""}, {"customData", customData}};
    }
    // Adds the component, or merges the settings into this object's short-text component of the type.
    // A hand-made component of a single-instance type is never overwritten.
    bool Upsert(EmptyObject *object, const std::string &type, JSON settings, const std::string &describe, bool singleInstance = true) {
        settings["shortTextManaged"] = true;
        settings["sourceText"] = source;
        if (auto *managed = FindOfType(object, type, true)) {
            const JSON before = object->SaveComponentToJson(managed);
            JSON after = before;
            after["data"]["customData"].update(settings);
            group->AddCommand(std::make_unique<ComponentEditCommand>(object, managed, before, after));
            Step(object->GetName() + " の短文由来の " + type + " を更新: " + describe);
            return true;
        }
        if (singleInstance && FindOfType(object, type))
            return Fail(object->GetName() + " には手作りの " + type + " があります。Inspector で編集するか、削除してから適用してください（手作りの部品は上書きしません）。");
        group->AddCommand(std::make_unique<AddComponentCommand>(object, type, NewState(type, settings)));
        Step(object->GetName() + " に " + type + " を追加: " + describe);
        return true;
    }
    // Ensures a component exists (any existing one is used as is; a new one is marked as short-text managed).
    void Ensure(EmptyObject *object, const std::string &type, JSON settings, const std::string &why) {
        if (FindOfType(object, type)) { Step(object->GetName() + " の既存の " + type + " を使用（" + why + "）"); return; }
        settings["shortTextManaged"] = true;
        settings["sourceText"] = source;
        group->AddCommand(std::make_unique<AddComponentCommand>(object, type, NewState(type, settings)));
        Step(object->GetName() + " に " + type + " を追加（" + why + "）");
    }
    bool UpsertHandler(EmptyObject *object, GameEventListener::Handler handler) {
        handler.shortTextManaged = true;
        handler.sourceText = source;
        const JSON encoded = GameEventListener::EncodeHandler(handler);
        if (auto *listener = object->GetComponent<GameEventListener>()) {
            const JSON before = object->SaveComponentToJson(listener);
            JSON after = before;
            auto &handlers = after["data"]["customData"]["handlers"];
            if (!handlers.is_array()) handlers = JSON::array();
            bool replaced = false;
            for (auto &existing : handlers) {
                if (existing.value("id", std::string{}) != handler.id) continue;
                if (!existing.value("shortTextManaged", false)) return Fail("同じIDの手作りの受信処理があります: " + handler.id);
                existing = encoded;
                replaced = true;
            }
            if (!replaced) {
                if (handlers.size() >= GameEventListener::kMaxHandlers) return Fail("GameEventListener の受信処理が上限（32）です。");
                handlers.push_back(encoded);
            }
            GameEventListener::Settings check;
            std::string reason;
            if (!GameEventListener::DecodeSettings(after["data"]["customData"], check, &reason)) return Fail("受信処理を作れません: " + reason);
            group->AddCommand(std::make_unique<ComponentEditCommand>(object, listener, before, after));
            Step(object->GetName() + " の GameEventListener の受信処理を" + (replaced ? "更新" : "追加（既存の受信処理はそのまま）") + ": " + GameEventListener::DescribeHandler(handler));
            return true;
        }
        GameEventListener::Settings settings;
        settings.handlers.push_back(handler);
        std::string reason;
        if (!GameEventListener::IsValid(settings, &reason)) return Fail("受信処理を作れません: " + reason);
        group->AddCommand(std::make_unique<AddComponentCommand>(object, "GameEventListener", NewState("GameEventListener", GameEventListener::EncodeSettings(settings))));
        Step(object->GetName() + " に GameEventListener を追加: " + GameEventListener::DescribeHandler(handler));
        return true;
    }
};

// Converts a sentence value to an action argument source, checked against the argument type it feeds.
bool ResolveValue(Builder &b, const ShortText::EventValueText &value, GameEvents::ArgType type, const GameEvents::Definition *received,
    GameEventListener::ValueSource &out, const std::string &what) {
    if (value.fromPayload) {
        const GameEvents::ArgDef *arg = nullptr;
        if (received) for (const auto &candidate : received->args) if (candidate.name == value.text) arg = &candidate;
        if (!arg) return b.Fail(what + ": 受け取るイベントに引数「" + value.text + "」がありません。");
        const bool numeric = (type == GameEvents::ArgType::Number || type == GameEvents::ArgType::Int) &&
            (arg->type == GameEvents::ArgType::Number || arg->type == GameEvents::ArgType::Int);
        if (arg->type != type && !(numeric && !(type == GameEvents::ArgType::Int && arg->type == GameEvents::ArgType::Number)))
            return b.Fail(what + ": 受信引数「" + value.text + "」の型（" + GameEvents::ArgTypeName(arg->type) + "）が " + GameEvents::ArgTypeName(type) + " と合いません。");
        if (!arg->required && arg->defaultValue.is_null()) b.Require("受信引数「" + value.text + "」は省略可能です。省略された場合はこの動作を実行しません（診断に表示）。");
        out = {GameEventListener::SourceKind::Payload, nullptr, value.text};
        return true;
    }
    GameEvents::Value literal;
    std::string reason;
    if (!ShortText::ParseEventLiteral(type, value.text, literal, &reason)) return b.Fail(what + ": " + reason);
    if (type == GameEvents::ArgType::Object) {
        auto *object = b.UniqueByName(literal.get<std::string>(), reason);
        if (!object) return b.Fail(what + ": " + reason);
        literal = object->GetObjectID().ToString();
    }
    out = {GameEventListener::SourceKind::Fixed, literal, ""};
    return true;
}

//--------------------------------------------------
// Handler sentences
//--------------------------------------------------
bool PlanHandler(Builder &b, const ShortText::EventRequest &r) {
    EmptyObject *owner = b.target;
    GameEventListener::Handler handler;
    std::string identity;
    const GameEvents::Definition *received = nullptr;
    std::string why;
    switch (r.trigger) {
    case EventTrigger::Start:
        handler.trigger = GameEventListener::Trigger::Start;
        handler.once = true;
        identity = "start";
        b.Require("きっかけが無いため、再生開始時に1回実行します。");
        break;
    case EventTrigger::Receive:
        received = b.Definition(r.triggerEvent);
        if (!received) return b.Fail("イベント「" + r.triggerEvent + "」はシーンに定義されていません。先に「" + r.triggerEvent + "イベントを作成」などで定義してください。");
        handler.trigger = GameEventListener::Trigger::Event;
        handler.event = received->name;
        identity = "receive:" + GameEvents::FoldName(received->name);
        break;
    case EventTrigger::Key:
        handler.trigger = GameEventListener::Trigger::Key;
        handler.key = ShortText::ParseKey(r.key);
        if (handler.key == Key::Unknown) return b.Fail("キー「" + r.key + "」は使えません。");
        identity = "key:" + r.key;
        break;
    case EventTrigger::Contact: {
        if (!r.contactSource.empty() && !(b.target && b.target->GetName() == r.contactSource)) {
            owner = b.UniqueByName(r.contactSource, why);
            if (!owner) return b.Fail(why);
        }
        const auto named = b.context->GetSceneObjects(r.contactOther);
        const auto tagged = b.ByTag(r.contactOther);
        handler.trigger = GameEventListener::Trigger::Contact;
        if (named.size() == 1 && (tagged.empty() || (tagged.size() == 1 && tagged.front() == named.front()))) handler.contactName = r.contactOther;
        else if (named.empty() && !tagged.empty()) handler.contactTag = r.contactOther;
        else if (named.size() > 1) return b.Fail("「" + r.contactOther + "」という名前のオブジェクトが複数あります。名前を一意にするか、タグを使ってください。");
        else if (!named.empty()) return b.Fail("「" + r.contactOther + "」は名前とタグの両方に一致します（別のオブジェクト）。どちらかに揃えてください。");
        else return b.Fail("「" + r.contactOther + "」という名前・タグのオブジェクトがシーンにありません。");
        b.NoteContact(owner, handler.contactName.empty() ? tagged : named, r.contactOther);
        identity = "contact:" + r.contactOther;
        break;
    }
    }
    if (!owner) return b.Fail("対象のオブジェクトをひとつ選択してください。");

    GameEventListener::Action action;
    const auto &a = r.action;
    switch (a.kind) {
    case EventActionKind::Emit: {
        const auto *definition = b.Definition(a.event);
        if (!definition) return b.Fail("イベント「" + a.event + "」はシーンに定義されていません。先に「" + a.event + "イベントを作成」などで定義してください。");
        action.kind = GameEventListener::ActionKind::Emit;
        action.name = definition->name;
        if (a.targetMode == 1) {
            auto *object = b.UniqueByName(a.target, why);
            if (!object) return b.Fail("送信先: " + why);
            action.emitTarget = GameEvents::TargetMode::Object;
            action.emitTargetValue = object->GetObjectID().ToString(); // stable UUID, resolved for liveness at send time
            if (!object->GetComponent<GameEventListener>()) b.Require(a.target + " が受け取るには GameEventListener（「" + definition->name + "を受け取ったら…」）が必要です。");
        } else if (a.targetMode == 2) {
            action.emitTarget = GameEvents::TargetMode::Tag;
            action.emitTargetValue = a.target;
            if (b.ByTag(a.target).empty()) b.Require("タグ「" + a.target + "」のオブジェクトがまだありません。");
        } else if (a.targetMode == 3) {
            action.emitTarget = GameEvents::TargetMode::Object;
            action.emitTargetValue = "$sender";
        }
        std::set<std::string> given;
        for (const auto &[name, value] : a.args) {
            const GameEvents::ArgDef *arg = nullptr;
            if (name.empty()) {
                if (definition->args.size() != 1) return b.Fail("イベント「" + definition->name + "」の引数は " + std::to_string(definition->args.size()) + " 個です。「引数名を値として」で指定してください。");
                arg = &definition->args.front();
            } else for (const auto &candidate : definition->args) if (candidate.name == name) arg = &candidate;
            if (!arg) return b.Fail("イベント「" + definition->name + "」に引数「" + name + "」はありません（定義: " + GameEvents::DescribeDefinition(*definition) + "）。");
            GameEventListener::ValueSource source;
            if (!ResolveValue(b, value, arg->type, received, source, "引数「" + arg->name + "」")) return false;
            action.args.emplace_back(arg->name, std::move(source));
            given.insert(arg->name);
        }
        for (const auto &arg : definition->args) if (arg.required && !given.contains(arg.name)) return b.Fail("イベント「" + definition->name + "」の必須の引数「" + arg.name + "」がありません。");
        // Validate the fixed part of the payload exactly as the router will (payload-sourced values are typed above).
        GameEvents::Value fixed = GameEvents::Value::object();
        for (const auto &[name, source] : action.args) if (source.kind == GameEventListener::SourceKind::Fixed) fixed[name] = source.value;
        GameEvents::Definition relaxed = *definition;
        for (auto &arg : relaxed.args) arg.required = false;
        GameEvents::Value payload;
        std::string reason;
        if (!GameEvents::BuildPayload(relaxed, fixed, payload, &reason)) return b.Fail("引数が不正です: " + reason);
        identity += "|emit:" + GameEvents::FoldName(definition->name) + ":" + std::to_string(a.targetMode) + ":" + a.target;
        b.needRouter = true;
        break;
    }
    case EventActionKind::CallScript: {
        action.kind = GameEventListener::ActionKind::CallScript;
        action.name = a.function;
        action.passEvent = a.passEvent;
        for (const auto &argName : a.functionArgs) {
            GameEventListener::ValueSource source;
            const GameEvents::ArgDef *arg = nullptr;
            if (received) for (const auto &candidate : received->args) if (candidate.name == argName) arg = &candidate;
            if (!arg) return b.Fail("受け取るイベントに引数「" + argName + "」がありません。");
            if (!ResolveValue(b, {argName, true}, arg->type, received, source, "関数の引数")) return false;
            action.args.emplace_back(argName, std::move(source));
        }
        if (!owner->GetComponent<ScriptComponent>())
            b.Require(owner->GetName() + " に ScriptComponent（関数 " + a.function + " を持つスクリプト）が必要です。スクリプトは自動では作りません。");
        b.Require(a.passEvent ? "スクリプトに `void " + a.function + "(GameEvent@ e)` を用意してください（e.GetFloat(\"amount\") などで引数を読めます）。" :
            "スクリプトに関数 " + a.function + " を用意してください（引数 " + std::to_string(a.functionArgs.size()) + " 個。型は受信引数の型に合わせます）。");
        identity += "|call:" + a.function;
        break;
    }
    case EventActionKind::ComponentCommand: {
        action.kind = GameEventListener::ActionKind::ComponentCommand;
        action.name = a.command;
        const auto type = a.command.substr(0, a.command.find('.'));
        for (const auto &[name, value] : a.commandArgs) {
            GameEventListener::ValueSource source;
            const auto argType = name == "item" ? GameEvents::ArgType::String : name == "amount" && type == "Inventory" ? GameEvents::ArgType::Int : GameEvents::ArgType::Number;
            if (!ResolveValue(b, value, argType, received, source, a.command + " の " + name)) return false;
            if (source.kind == GameEventListener::SourceKind::Fixed && source.value.is_number() && source.value.get<double>() <= 0) return b.Fail(a.command + " の " + name + " は 0 より大きい値です。");
            action.args.emplace_back(name, std::move(source));
        }
        if (type == "Health") b.Ensure(owner, "Health", JSON::object(), a.command + " の実行先");
        else if (type == "Inventory") b.Ensure(owner, "Inventory", JSON::object(), a.command + " の実行先");
        else if (type == "Respawner") b.Ensure(owner, "Respawner", JSON::object(), a.command + " の実行先");
        else if (type == "Dialogue") { b.Ensure(owner, "Dialogue", JSON::object(), a.command + " の実行先"); b.Require("会話の文章は Dialogue に入力してください（自動では作りません）。"); }
        else if (!FindOfType(owner, type)) b.Require(owner->GetName() + " に " + type + " が必要です（テンプレートなどの設定が必要なため自動では追加しません）。");
        identity += "|command:" + a.command;
        break;
    }
    case EventActionKind::Tween: {
        const auto parsed = ParseShortTextPropertyRequest(a.tweenText, owner);
        if (!parsed.recognized || !parsed.success) return b.Fail("「" + a.tweenText + "」を解釈できません: " + parsed.message);
        if (parsed.request.mode != ShortText::PropertyMode::Tween) return b.Fail("受け取ったときの動きは「N秒かけて AからBまで イージング」で書いてください。");
        ShortTextComponentPlan sub;
        std::string applied;
        ShortTextTweenOptions options;
        options.playOnStart = false;
        options.entryName = "ShortText.On" + received->name + "." + parsed.request.component + "." + parsed.request.member + (parsed.request.axis >= 0 ? "." + std::string(1, "xyzw"[parsed.request.axis & 3]) : "");
        options.appliedName = &applied;
        auto command = MakeShortTextPropertyCommand(owner, parsed.request, b.source, &sub, &options);
        if (!command || !sub.success) return b.Fail(sub.message);
        b.group->AddCommand(std::move(command));
        for (const auto &step : sub.steps) b.Step(step);
        for (const auto &requirement : sub.requirements) b.Require(requirement);
        action.kind = GameEventListener::ActionKind::EngineCommand;
        action.name = "keyframe.play";
        action.args.emplace_back("name", GameEventListener::ValueSource{GameEventListener::SourceKind::Fixed, applied, ""});
        identity += "|tween:" + parsed.request.component + "." + parsed.request.member + "." + std::to_string(parsed.request.axis);
        break;
    }
    }
    if (handler.trigger == GameEventListener::Trigger::Event) b.needRouter = true;
    handler.id = "st-" + Hash(identity);
    handler.actions.push_back(std::move(action));
    return b.UpsertHandler(owner, std::move(handler));
}

//--------------------------------------------------
// Gameplay component sentences
//--------------------------------------------------
void NoteRecipientInventory(Builder &b, const std::string &tag) {
    const auto recipients = b.ByTag(tag);
    if (recipients.empty()) { b.Require("タグ「" + tag + "」のオブジェクト（受け取り手）がシーンにありません。受け取り手は自動では作りません。"); return; }
    std::vector<EmptyObject *> missing;
    for (auto *object : recipients) if (!object->GetComponent<Inventory>()) missing.push_back(object);
    if (recipients.size() == 1 && missing.size() == 1) b.Ensure(missing.front(), "Inventory", JSON::object(), "アイテムの受け取り先");
    else for (auto *object : missing) b.Require(object->GetName() + "（タグ " + tag + "）に Inventory が必要です。");
}

bool PlanComponent(Builder &b, const ShortText::EventRequest &r) {
    auto *t = b.target;
    if (!t) return b.Fail("対象のオブジェクトをひとつ選択してください。");
    const auto &p = r.params;
    std::string why;
    switch (r.kind) {
    case EventRequestKind::Health: {
        const double max = p["max"].get<double>();
        if (!b.Upsert(t, "Health", JSON{{"max", max}, {"current", max}}, "最大体力 " + Number(max) + "・現在 " + Number(max))) return false;
        if (p.contains("otherTag")) {
            const auto tag = p["otherTag"].get<std::string>();
            const double amount = p["amount"].get<double>();
            if (!b.Upsert(t, "ContactDamage", JSON{{"otherTag", tag}, {"amount", amount}, {"mode", 0}}, "タグ " + tag + " に触れたら " + Number(amount) + " ダメージ（同じ相手は 0.5 秒に1回）", false)) return false;
            b.NoteContact(t, b.ByTag(tag), "タグ「" + tag + "」");
        }
        return true;
    }
    case EventRequestKind::ContactDamage: {
        const auto tag = p["otherTag"].get<std::string>();
        if (!b.Upsert(t, "ContactDamage", JSON{{"otherTag", tag}, {"amount", p["amount"]}, {"mode", 1}}, "タグ " + tag + " の相手へ " + Number(p["amount"].get<double>()) + " ダメージ", false)) return false;
        b.NoteContact(t, b.ByTag(tag), "タグ「" + tag + "」");
        for (auto *other : b.ByTag(tag)) if (!other->GetComponent<Health>()) b.Require(other->GetName() + " に Health が必要です。");
        return true;
    }
    case EventRequestKind::Pickup: {
        const auto tag = p["recipientTag"].get<std::string>();
        const auto item = p["item"].get<std::string>();
        if (!Inventory::IsValidItemId(item)) return b.Fail("アイテムID「" + item + "」は使えません。");
        if (!b.Upsert(t, "Pickup", JSON{{"recipientTag", tag}, {"itemId", item}, {"amount", p["amount"]}, {"after", p["after"]}, {"once", true}},
            tag + " が触れたら「" + item + "」を " + p["amount"].dump() + " 個渡し、" + std::string(p["after"] == 0 ? "残る" : p["after"] == 1 ? "無効化" : "削除") + "（一度だけ）")) return false;
        NoteRecipientInventory(b, tag);
        b.NoteContact(t, b.ByTag(tag), "タグ「" + tag + "」");
        return true;
    }
    case EventRequestKind::Lifetime:
        return b.Upsert(t, "Lifetime", JSON{{"duration", p["duration"]}, {"action", p["action"]}},
            "再生中に " + Number(p["duration"].get<double>()) + " 秒経ったら" + (p["action"] == 1 ? "無効化（再有効化で 0 秒から）" : "削除") + "。編集中は進みません");
    case EventRequestKind::Spawner: {
        auto *source = b.UniqueByName(p["template"].get<std::string>(), why);
        if (!source) return b.Fail("テンプレート: " + why + " 既存のオブジェクトを指定してください（自動では作りません）。");
        for (auto *ancestor = t; ancestor; ancestor = ancestor->GetComponent<Transform>() ? ancestor->GetComponent<Transform>()->GetParentObject() : nullptr)
            if (ancestor == source) return b.Fail("自分自身や親はテンプレートにできません。");
        if (source->GetComponent<Spawner>()) return b.Fail("Spawner を持つオブジェクトはテンプレートにできません（自己複製を防ぐため）。");
        if (source->IsActive()) b.Require("テンプレート「" + source->GetName() + "」は通常、無効化しておきます（複製は有効化して生成します）。");
        return b.Upsert(t, "Spawner", JSON{{"templateObject", source->GetObjectID().ToString()}, {"interval", p["interval"]}, {"maxAlive", p["maxAlive"]}},
            Number(p["interval"].get<double>()) + " 秒ごとに「" + source->GetName() + "」（UUID参照）を複製、同時に最大 " + p["maxAlive"].dump() + " 体", false);
    }
    case EventRequestKind::Interact: {
        JSON settings{{"key", r.key}, {"interactorTag", "Player"}};
        std::string describe = "タグ Player が 2 以内で " + r.key + " キー → ";
        if (p.contains("open") || p.contains("clip")) {
            const std::string clip = p.contains("clip") ? p["clip"].get<std::string>() : "Open";
            auto *animator = t->GetComponent<KeyFrameAnimator>();
            if (animator && animator->HasAnimation(clip)) { settings["keyframeClip"] = clip; describe += "KeyFrameAnimator の「" + clip + "」を再生"; }
            else {
                settings["keyframeClip"] = "";
                describe += "（開く処理は未設定）";
                b.Require("開く方法が未設定です。" + t->GetName() + " の KeyFrameAnimator にアニメーション「" + clip + "」を用意するか、Interactable の「再生するアニメーション」「実行イベント」を設定してください（推測しません）。");
            }
        } else if (p.contains("dialogue")) {
            settings["startDialogue"] = true;
            describe += "Dialogue を開始／進める";
            b.Ensure(t, "Dialogue", JSON::object(), "話しかけた時の会話");
            b.Require("会話の文章は Dialogue に入力してください（自動では作りません）。");
        } else {
            const auto *definition = b.Definition(p["event"].get<std::string>());
            if (!definition) return b.Fail("イベント「" + p["event"].get<std::string>() + "」はシーンに定義されていません。");
            settings["interactEvent"] = definition->name;
            describe += "イベント「" + definition->name + "」を送る";
            b.needRouter = true;
        }
        if (b.ByTag("Player").empty()) b.Require("操作する相手（タグ Player）のオブジェクトがシーンにありません。");
        return b.Upsert(t, "Interactable", settings, describe, false);
    }
    case EventRequestKind::Respawn: {
        if (!b.Upsert(t, "Respawner", JSON{{"checkpointTag", "Checkpoint"}, {"useFall", p["useFall"]}, {"fallY", p["fallY"]}, {"onDeath", p["onDeath"]}, {"resetVelocity", true}},
            std::string("タグ Checkpoint に触れた場所（その Checkpoint の位置＋オフセット）を復活地点にし、") +
            (p["useFall"].get<bool>() ? "ワールドY が " + Number(p["fallY"].get<double>()) + " 未満で" : "Health が 0 で") + " 戻る（Velocity を 0 に）")) return false;
        if (b.ByTag("Checkpoint").empty()) b.Require("タグ Checkpoint のオブジェクト（Collider 付き）を置くと、触れた場所が復活地点になります。それまでは再生開始位置に戻ります。");
        b.NoteContact(t, b.ByTag("Checkpoint"), "タグ「Checkpoint」");
        return true;
    }
    case EventRequestKind::Inventory:
        return b.Upsert(t, "Inventory", JSON{{"capacity", p["capacity"]}}, "アイテムID→個数（種類数の上限 " + p["capacity"].dump() + "）");
    case EventRequestKind::KeyDoor: {
        const auto item = p["item"].get<std::string>();
        if (!Inventory::IsValidItemId(item)) return b.Fail("アイテムID「" + item + "」は使えません。");
        JSON settings{{"key", "E"}, {"interactorTag", "Player"}, {"requiredItem", item}, {"requiredAmount", 1}};
        auto *animator = t->GetComponent<KeyFrameAnimator>();
        std::string describe = "タグ Player が「" + item + "」を持っていれば E キーで開く";
        if (animator && animator->HasAnimation("Open")) { settings["keyframeClip"] = "Open"; describe += "（KeyFrameAnimator の「Open」を再生）"; }
        else b.Require("開く方法が未設定です。" + t->GetName() + " の KeyFrameAnimator にアニメーション「Open」を用意するか、Interactable の実行イベントを設定してください（推測しません）。");
        if (!b.Upsert(t, "Interactable", settings, describe, false)) return false;
        const auto keys = b.context->GetSceneObjects(item);
        if (keys.size() == 1 && keys.front() != t) {
            if (!b.Upsert(keys.front(), "Pickup", JSON{{"recipientTag", "Player"}, {"itemId", item}, {"amount", 1}, {"after", 2}, {"once", true}}, "Player が触れたら「" + item + "」を 1 個渡して削除")) return false;
            b.NoteContact(keys.front(), b.ByTag("Player"), "タグ「Player」");
        } else if (keys.size() > 1) b.Require("「" + item + "」という名前のオブジェクトが複数あります。取得させるオブジェクトに Pickup（アイテムID " + item + "）を付けてください。");
        else b.Require("取得させるオブジェクト（名前「" + item + "」など）に Pickup（アイテムID " + item + "）を付けてください。");
        NoteRecipientInventory(b, "Player");
        return true;
    }
    case EventRequestKind::Dialogue: {
        auto *dialogue = t->GetComponent<Dialogue>();
        if (!dialogue) b.Ensure(t, "Dialogue", JSON::object(), "会話");
        if (!dialogue || dialogue->GetSettings().lines.empty()) b.Require("会話の文章が未入力です。Dialogue の Inspector で行・選択肢を入力してください（文章は自動生成しません）。");
        if (!t->GetComponent<TextRenderer>() && !t->GetComponent<BitmapTextRenderer>())
            b.Require("表示先（TextRenderer / BitmapTextRenderer を持つオブジェクト）を Dialogue の「表示先」に設定してください。");
        if (b.ByTag("Player").empty()) b.Require("話しかける相手（タグ Player）のオブジェクトがシーンにありません。");
        return b.Upsert(t, "Interactable", JSON{{"key", r.key}, {"interactorTag", "Player"}, {"startDialogue", true}}, "タグ Player が近くで " + r.key + " キー → 会話を開始／次の行へ（選択肢は 1〜4 キー）", false);
    }
    case EventRequestKind::AnimationState: {
        const auto moving = p["moving"].get<std::string>(), idle = p["idle"].get<std::string>();
        const double threshold = p["threshold"].get<double>();
        int player = 0;
        auto *keyframes = t->GetComponent<KeyFrameAnimator>();
        if (keyframes && keyframes->HasAnimation(moving) && keyframes->HasAnimation(idle)) player = 0;
        else if (t->GetComponent<Animator>()) { player = 1; b.Require("Animator のクリップ名「" + moving + "」「" + idle + "」が正しいか確認してください。"); }
        else b.Require("再生先が未設定です。" + t->GetName() + " の KeyFrameAnimator にアニメーション「" + moving + "」「" + idle + "」を用意するか、Animator を追加して再生先を変更してください（推測しません）。");
        JSON states = JSON::array({
            {{"name", idle}, {"minSpeed", 0.0}, {"maxSpeed", threshold}, {"priority", 0}, {"clip", idle}, {"player", player}},
            {{"name", moving}, {"minSpeed", threshold}, {"maxSpeed", -1.0}, {"priority", 1}, {"clip", moving}, {"player", player}}});
        return b.Upsert(t, "AnimationStateSwitcher", JSON{{"states", states}},
            "速度 " + Number(threshold) + " 以上で「" + moving + "」、未満で「" + idle + "」（状態が変わった時だけ再生、" + (player == 0 ? "KeyFrameAnimator" : "Animator") + "）");
    }
    default: return b.Fail("未対応の要求です。");
    }
}

bool PlanDefine(Builder &b, const ShortText::EventRequest &r) {
    auto definition = r.definition;
    definition.shortTextManaged = true;
    definition.sourceText = b.source;
    std::string reason;
    if (!GameEvents::ValidateDefinition(definition, &reason)) return b.Fail(reason);
    for (auto &existing : b.definitionsAfter) {
        if (!GameEvents::SameName(existing.name, definition.name)) continue;
        if (GameEvents::EncodeDefinition(existing)["args"] == GameEvents::EncodeDefinition(definition)["args"]) {
            b.Step("イベント「" + existing.name + "」は同じ引数で定義済みです（変更なし）。");
            b.needRouter = true;
            return true;
        }
        if (!existing.shortTextManaged) return b.Fail("手作りのイベント「" + existing.name + "」が別の引数で定義されています（" + GameEvents::DescribeDefinition(existing) + "）。Inspector で編集してください。");
        definition.name = existing.name; // keep the stored spelling
        definition.description = existing.description;
        existing = definition;
        b.Step("短文由来のイベント「" + definition.name + "」の引数を更新: " + GameEvents::DescribeDefinition(definition));
        b.needRouter = true;
        return true;
    }
    if (b.definitionsAfter.size() >= GameEvents::MaxDefinitions) return b.Fail("イベント定義は256個までです。");
    b.definitionsAfter.push_back(definition);
    b.Step("イベント「" + definition.name + "」を定義: " + GameEvents::DescribeDefinition(definition));
    b.needRouter = true;
    return true;
}
} // namespace

std::unique_ptr<IEditorCommand> MakeShortTextEventCommand(SceneEditorContext *context, EmptyObject *target,
    const ShortText::EventRequest &request, const std::string &sourceText, ShortTextComponentPlan *plan) {
    ShortTextComponentPlan local;
    auto &result = plan ? *plan : local;
    result = {};
    auto fail = [&](const std::string &message) -> std::unique_ptr<IEditorCommand> { result = {}; result.success = false; result.message = message; return nullptr; };
    if (!context) return fail("シーンがありません。");
    if (context->IsPlaying()) return fail("編集は再生を停止してから行ってください。");
    if (sourceText.size() > 512) return fail("文章は512バイト以下にしてください。");
    if (request.kind != EventRequestKind::DefineEvent && (!target || !context->GetSceneObject(target->GetObjectID()))) return fail("対象のオブジェクトをひとつ選択してください。");
    Builder b{context, target, sourceText, result};
    auto *router = context->GetComponent<GameEventRouter>();
    b.hasRouter = router != nullptr;
    if (router) b.definitionsBefore = b.definitionsAfter = router->GetDefinitions();
    bool ok = false;
    if (request.kind == EventRequestKind::DefineEvent) ok = PlanDefine(b, request);
    else if (request.kind == EventRequestKind::Handler) ok = PlanHandler(b, request);
    else ok = PlanComponent(b, request);
    if (!ok) return fail(b.error);

    const auto beforeJson = GameEvents::EncodeDefinitions(b.definitionsBefore), afterJson = GameEvents::EncodeDefinitions(b.definitionsAfter);
    std::unique_ptr<IEditorCommand> definitions;
    if (!b.hasRouter && b.needRouter) {
        definitions = std::make_unique<GameEventDefinitionsCommand>(beforeJson, afterJson, true);
        result.steps.insert(result.steps.begin(), "シーンに GameEventRouter を追加（イベントの定義・配送）");
    } else if (beforeJson != afterJson) definitions = std::make_unique<GameEventDefinitionsCommand>(beforeJson, afterJson, false);
    // Definitions come first so listeners added in the same step subscribe to an existing event.
    auto group = std::make_unique<CompositeCommand>("短文からイベント・部品を設定");
    if (definitions) group->AddCommand(std::move(definitions));
    group->AddCommand(std::move(b.group));
    result.success = true;
    result.message = "解釈: " + ShortText::DescribeEventRequest(request);
    return group;
}
} // namespace KashipanEngine
#endif
