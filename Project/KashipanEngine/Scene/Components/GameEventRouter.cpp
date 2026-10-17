#include "Scene/Components/GameEventRouter.h"
#include <algorithm>
#include <atomic>
#include <exception>
#include "Objects/EmptyObject.h"
#include "Objects/IObjectComponent.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_stdlib.h>
#endif

namespace KashipanEngine {
namespace {
bool Fail(std::string *error, const std::string &message) { if (error) *error = message; return false; }
}

std::uint64_t GameEventRouter::NextGeneration() {
    static std::atomic<std::uint64_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

GameEventRouter *GameEventRouter::Find(SceneContext *scene) {
    return scene ? scene->GetComponent<GameEventRouter>() : nullptr;
}

std::unique_ptr<ISceneComponent> GameEventRouter::Clone() const {
    auto copy = std::make_unique<GameEventRouter>();
    copy->definitions_ = definitions_;
    copy->limits_ = limits_;
    return copy;
}

bool GameEventRouter::AddDefinition(const GameEvents::Definition &definition, std::string *error) {
    if (definitions_.size() >= GameEvents::MaxDefinitions) return Fail(error, "イベント定義は256個までです。");
    if (!GameEvents::ValidateDefinition(definition, error)) return false;
    if (FindDefinition(definition.name)) return Fail(error, "イベント「" + definition.name + "」は既にあります（大文字小文字は区別しません）。");
    definitions_.push_back(definition);
    return true;
}

bool GameEventRouter::ReplaceDefinition(const std::string &name, const GameEvents::Definition &definition, std::string *error) {
    if (!GameEvents::ValidateDefinition(definition, error)) return false;
    auto it = std::find_if(definitions_.begin(), definitions_.end(), [&](const auto &d) { return GameEvents::SameName(d.name, name); });
    if (it == definitions_.end()) return Fail(error, "イベント「" + name + "」がありません。");
    for (const auto &d : definitions_) if (&d != &*it && GameEvents::SameName(d.name, definition.name)) return Fail(error, "イベント「" + definition.name + "」は既にあります。");
    *it = definition;
    return true;
}

bool GameEventRouter::RemoveDefinition(const std::string &name) {
    const auto before = definitions_.size();
    std::erase_if(definitions_, [&](const auto &d) { return GameEvents::SameName(d.name, name); });
    return definitions_.size() != before;
}

bool GameEventRouter::SetDefinitions(const std::vector<GameEvents::Definition> &definitions, std::string *error) {
    std::vector<GameEvents::Definition> checked;
    if (!GameEvents::DecodeDefinitions(GameEvents::EncodeDefinitions(definitions), checked, error)) return false;
    definitions_ = std::move(checked);
    return true;
}

bool GameEventRouter::ImportDefinitions(const GameEvents::Value &json, std::string *error) {
    std::vector<GameEvents::Definition> incoming;
    if (!GameEvents::DecodeDefinitions(json, incoming, error)) return false;
    auto merged = definitions_;
    for (const auto &d : incoming) {
        if (const auto *existing = GameEvents::Find(merged, d.name)) {
            if (GameEvents::EncodeDefinition(*existing)["args"] != GameEvents::EncodeDefinition(d)["args"])
                return Fail(error, "イベント「" + d.name + "」は引数の異なる定義が既にあります。");
            continue;
        }
        merged.push_back(d);
    }
    return SetDefinitions(merged, error);
}

bool GameEventRouter::SetLimits(const Limits &limits) {
    if (limits.maxQueue < 1 || limits.maxQueue > 65536 || limits.maxPerUpdate < 1 || limits.maxPerUpdate > 4096 ||
        limits.maxDepth < 1 || limits.maxDepth > 64) return false;
    limits_ = limits;
    return true;
}

void GameEventRouter::AddDiagnostic(const std::string &message) {
    if (diagnostics_.size() >= 32) diagnostics_.pop_front();
    diagnostics_.push_back(message);
}

bool GameEventRouter::Emit(const std::string &name, const GameEvents::Value &payload, const std::string &sender,
    GameEvents::TargetMode target, const std::string &targetValue, std::string *error, bool dropUndeclared) {
    std::string reason;
    auto reject = [&](const std::string &message) {
        ++dropped_;
        AddDiagnostic("送信を拒否: " + name + " / " + message);
        return Fail(error, message);
    };
    const auto *definition = FindDefinition(name);
    if (!definition) return reject("イベント「" + name + "」は定義されていません。");
    GameEvents::Message message;
    message.name = definition->name;
    if (!GameEvents::BuildPayload(*definition, payload, message.payload, &reason, dropUndeclared)) return reject(reason);
    message.sender = sender;
    message.target = target;
    message.targetValue = targetValue;
    message.depth = current_ ? current_->depth + 1 : 0;
    if (message.depth > limits_.maxDepth) return reject("イベントの連鎖が深すぎるため打ち切りました（上限 " + std::to_string(limits_.maxDepth) + "）。");
    if (static_cast<int>(queue_.size()) >= limits_.maxQueue) return reject("イベントキューが一杯です。");
    if (static_cast<int>(target) < 0 || static_cast<int>(target) > static_cast<int>(GameEvents::TargetMode::Tag)) return reject("送信先の種類が不正です。");
    if (target != GameEvents::TargetMode::Broadcast) {
        if (targetValue.empty()) return reject("送信先が指定されていません。");
        auto *scene = GetOwnerSceneContext();
        if (target == GameEvents::TargetMode::Object) {
            const UUID128 id(targetValue);
            if (!id.IsValid() || (scene && !scene->GetSceneObject(id))) return reject("送信先のオブジェクトがありません。");
        } else if (target == GameEvents::TargetMode::Name) {
            // A name selects exactly one object; it is fixed to that object's stable UUID when queued (tags select many).
            const auto matches = scene ? scene->GetSceneObjects(targetValue) : std::vector<EmptyObject *>{};
            if (matches.size() != 1) return reject(matches.empty() ? "送信先「" + targetValue + "」という名前のオブジェクトがありません。" :
                "送信先「" + targetValue + "」という名前のオブジェクトが複数あります（UUID かタグで指定してください）。");
            message.target = GameEvents::TargetMode::Object;
            message.targetValue = matches.front()->GetObjectID().ToString();
        }
    }
    message.serial = nextSerial_++;
    queue_.push_back(std::move(message));
    return true;
}

std::uint64_t GameEventRouter::Subscribe(const std::string &eventName, const ComponentRef &owner, Handler handler, const std::string &key) {
    if (!handler || eventName.empty()) return 0;
    const auto folded = GameEvents::FoldName(eventName);
    for (auto &[id, subscription] : subscriptions_) {
        if (subscription.event == folded && subscription.owner == owner && subscription.key == key) {
            subscription.handler = std::move(handler);
            return id;
        }
    }
    const auto id = nextSubscription_++;
    subscriptions_.emplace(id, Subscription{folded, owner, key, std::move(handler)});
    return id;
}

bool GameEventRouter::Unsubscribe(std::uint64_t id) { return subscriptions_.erase(id) > 0; }

size_t GameEventRouter::UnsubscribeOwner(const ComponentRef &owner, const std::string &key) {
    return std::erase_if(subscriptions_, [&](const auto &entry) {
        return entry.second.owner == owner && (key.empty() || entry.second.key == key);
    });
}

bool GameEventRouter::Matches(const Subscription &s, const GameEvents::Message &message, IObjectComponent *&receiver, bool &stale) const {
    receiver = nullptr;
    stale = false;
    if (s.event != GameEvents::FoldName(message.name)) return false;
    if (!s.owner.IsValid()) return true; // ownerless observers (engine code / tests) see every delivery
    auto *scene = GetOwnerSceneContext();
    receiver = scene ? scene->ResolveComponent(s.owner) : nullptr;
    if (!receiver) { stale = true; return false; }
    if (!receiver->IsActive()) return false;
    const EmptyObject *object = receiver->GetOwnerObject();
    if (!object) return false;
    switch (message.target) {
    case GameEvents::TargetMode::Broadcast: return true;
    case GameEvents::TargetMode::Object: return object->GetObjectID().ToString() == message.targetValue || object->GetObjectID() == UUID128(message.targetValue);
    case GameEvents::TargetMode::Name: return object->GetName() == message.targetValue;
    case GameEvents::TargetMode::Tag: return object->GetTagName() == message.targetValue;
    }
    return false;
}

void GameEventRouter::Deliver(const GameEvents::Message &message) {
    // Fix the receiver list first: handlers may subscribe, unsubscribe or delete components safely.
    std::vector<std::uint64_t> targets;
    std::vector<std::uint64_t> staleIds;
    for (const auto &[id, subscription] : subscriptions_) {
        IObjectComponent *receiver = nullptr;
        bool stale = false;
        if (Matches(subscription, message, receiver, stale)) targets.push_back(id);
        else if (stale) staleIds.push_back(id);
    }
    for (auto id : staleIds) subscriptions_.erase(id);
    const std::weak_ptr<int> alive = alive_;
    current_ = &message;
    for (auto id : targets) {
        auto it = subscriptions_.find(id);
        if (it == subscriptions_.end()) continue; // unsubscribed by an earlier handler
        IObjectComponent *receiver = nullptr;
        bool stale = false;
        if (!Matches(it->second, message, receiver, stale)) { if (stale) subscriptions_.erase(it); continue; }
        Handler handler = it->second.handler; // copy: the handler may unsubscribe itself
        try {
            handler(receiver, message);
        } catch (const std::exception &e) {
            if (alive.expired()) return;
            AddDiagnostic("受信処理で例外: " + message.name + " / " + e.what());
        } catch (...) {
            if (alive.expired()) return;
            AddDiagnostic("受信処理で不明な例外: " + message.name);
        }
        if (alive.expired()) return;
        current_ = &message;
        ++delivered_;
    }
    current_ = nullptr;
}

size_t GameEventRouter::ProcessQueue() {
    if (current_) return 0; // re-entrant call from inside a handler: the outer loop keeps draining
    size_t processed = 0;
    const std::weak_ptr<int> alive = alive_;
    // Objects deleted by handlers (including a script deleting itself) are destroyed only after delivery,
    // also when this is called outside the scene's update loop. The router itself may be removed by a handler.
    auto *scene = GetOwnerSceneContext();
    const std::weak_ptr<void> sceneAlive = scene ? scene->GetLifetimeToken() : std::weak_ptr<void>();
    const bool deferred = scene ? scene->BeginDeferredDestroy() : true;
    while (!queue_.empty() && static_cast<int>(processed) < limits_.maxPerUpdate) {
        GameEvents::Message message = std::move(queue_.front()); // copy-owned while handlers run
        queue_.pop_front();
        Deliver(message);
        if (alive.expired()) break;
        current_ = nullptr;
        ++processed;
    }
    const bool routerAlive = !alive.expired();
    if (routerAlive && !queue_.empty()) AddDiagnostic("1回の更新の配送上限に達したため、残り " + std::to_string(queue_.size()) + " 件は次の更新で配送します。");
    if (scene && !sceneAlive.expired()) scene->EndDeferredDestroy(deferred);
    return processed;
}

void GameEventRouter::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!scene || !scene->IsPlaying()) return;
    ProcessQueue();
}

void GameEventRouter::Finalize() {
    queue_.clear();
    subscriptions_.clear();
    generation_ = NextGeneration(); // listeners see the change and subscribe again if this router is used again
}

JSON GameEventRouter::SaveToJson() const {
    return JSON{{"definitions", GameEvents::EncodeDefinitions(definitions_)},
        {"limits", {{"maxQueue", limits_.maxQueue}, {"maxPerUpdate", limits_.maxPerUpdate}, {"maxDepth", limits_.maxDepth}}}};
}

bool GameEventRouter::LoadFromJson(const JSON &json) {
    if (!json.is_object()) return false;
    std::vector<GameEvents::Definition> definitions;
    std::string error;
    if (json.contains("definitions") && !GameEvents::DecodeDefinitions(json["definitions"], definitions, &error)) {
        AddDiagnostic("イベント定義を読み込めませんでした: " + error);
        return false;
    }
    Limits limits = limits_;
    if (json.contains("limits")) {
        const auto &l = json["limits"];
        if (!l.is_object()) return false;
        auto read = [&](const char *key, int &value) {
            if (!l.contains(key)) return true;
            if (!l[key].is_number_integer()) return false;
            value = l[key].get<int>(); return true;
        };
        if (!read("maxQueue", limits.maxQueue) || !read("maxPerUpdate", limits.maxPerUpdate) || !read("maxDepth", limits.maxDepth)) return false;
    }
    const Limits previous = limits_;
    if (!SetLimits(limits)) { limits_ = previous; return false; }
    definitions_ = std::move(definitions);
    queue_.clear();
    return true;
}

#if defined(USE_IMGUI)
void GameEventRouter::ShowImGui() {
    ImGui::TextWrapped("ユーザー定義イベント。受信は GameEventListener・各ゲーム部品・スクリプト（SubscribeEvent）で行います。");
    ImGui::Text("定義 %zu 件 / 購読 %zu 件 / 未配送 %zu 件 / 配送済み %llu / 拒否 %llu", definitions_.size(), subscriptions_.size(), queue_.size(),
        static_cast<unsigned long long>(delivered_), static_cast<unsigned long long>(dropped_));
    int removeIndex = -1;
    for (size_t i = 0; i < definitions_.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto edited = definitions_[i];
        bool changed = false;
        if (ImGui::TreeNode("definition", "%s%s", GameEvents::DescribeDefinition(edited).c_str(), edited.shortTextManaged ? "（短文から作成）" : "")) {
            changed |= ImGui::InputText("名前", &edited.name, ImGuiInputTextFlags_EnterReturnsTrue);
            changed |= ImGui::InputTextMultiline("説明", &edited.description, ImVec2(-1, 48));
            int removeArg = -1;
            for (size_t a = 0; a < edited.args.size(); ++a) {
                ImGui::PushID(static_cast<int>(a));
                auto &arg = edited.args[a];
                ImGui::SetNextItemWidth(120);
                changed |= ImGui::InputText("##name", &arg.name, ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine(); ImGui::SetNextItemWidth(110);
                int type = static_cast<int>(arg.type);
                if (ImGui::Combo("##type", &type, GameEvents::ArgTypeLabels, GameEvents::ArgTypeCount)) { arg.type = static_cast<GameEvents::ArgType>(type); arg.defaultValue = nullptr; changed = true; }
                ImGui::SameLine();
                if (ImGui::Checkbox("必須", &arg.required)) { if (arg.required) arg.defaultValue = nullptr; changed = true; }
                if (!arg.required) {
                    ImGui::SameLine(); ImGui::SetNextItemWidth(120);
                    std::string text = arg.defaultValue.is_null() ? "" : arg.defaultValue.dump();
                    if (ImGui::InputText("省略値(JSON)", &text, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        try { arg.defaultValue = text.empty() ? GameEvents::Value() : GameEvents::Value::parse(text); changed = true; }
                        catch (const std::exception &) { editError_ = "省略値はJSONで入力してください（例: 10, \"text\", [0,1,0]）。"; }
                    }
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("削除")) removeArg = static_cast<int>(a);
                ImGui::PopID();
            }
            if (removeArg >= 0) { edited.args.erase(edited.args.begin() + removeArg); changed = true; }
            if (edited.args.size() < GameEvents::MaxArgs && ImGui::Button("引数を追加")) {
                GameEvents::ArgDef arg; arg.name = "arg" + std::to_string(edited.args.size() + 1);
                edited.args.push_back(arg); changed = true;
            }
            if (ImGui::Button("この定義を削除")) removeIndex = static_cast<int>(i);
            ImGui::TreePop();
        }
        if (changed) {
            std::string error;
            const auto name = definitions_[i].name;
            if (ReplaceDefinition(name, edited, &error)) editError_.clear();
            else editError_ = error;
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0) definitions_.erase(definitions_.begin() + removeIndex);
    if (definitions_.size() < GameEvents::MaxDefinitions && ImGui::Button("イベントを追加")) {
        GameEvents::Definition d;
        for (int n = 1;; ++n) { d.name = "Event" + std::to_string(n); if (!FindDefinition(d.name)) break; }
        AddDefinition(d, &editError_);
    }
    if (!editError_.empty()) ImGui::TextWrapped("反映していません: %s", editError_.c_str());
    if (ImGui::TreeNode("制限")) {
        Limits next = limits_;
        ImGui::InputInt("キューの上限", &next.maxQueue);
        ImGui::InputInt("1更新の配送上限", &next.maxPerUpdate);
        ImGui::InputInt("連鎖の深さ上限", &next.maxDepth);
        if (next.maxQueue != limits_.maxQueue || next.maxPerUpdate != limits_.maxPerUpdate || next.maxDepth != limits_.maxDepth)
            if (!SetLimits(next)) ImGui::TextWrapped("範囲外です（キュー1〜65536、配送1〜4096、深さ1〜64）。");
        ImGui::TreePop();
    }
    auto *scene = GetOwnerSceneContext();
    if (scene && scene->IsPlaying()) {
        ImGui::SeparatorText("テスト送信（全体へ）");
        ImGui::InputText("イベント名", &testEvent_);
        ImGui::InputText("引数JSON", &testPayload_);
        if (ImGui::Button("送信")) {
            std::string error;
            try {
                if (Emit(testEvent_, GameEvents::Value::parse(testPayload_), "", GameEvents::TargetMode::Broadcast, "", &error)) editError_.clear();
                else editError_ = error;
            } catch (const std::exception &) { editError_ = "引数はJSONで入力してください。"; }
        }
    }
    if (!diagnostics_.empty() && ImGui::TreeNode("診断", "診断（%zu）", diagnostics_.size())) {
        for (const auto &line : diagnostics_) ImGui::BulletText("%s", line.c_str());
        if (ImGui::Button("診断を消去")) diagnostics_.clear();
        ImGui::TreePop();
    }
}
#endif
} // namespace KashipanEngine
