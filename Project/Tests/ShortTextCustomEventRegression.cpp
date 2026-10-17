// Native regression: user-defined events (GameEventRouter / GameEventListener / script API), the ten gameplay
// components and their short-text configuration. Real Scene / SceneEditorContext / commands / components / AngelScript.
// Headless: no window, GPU, audio device or input device (key input is passed to the components' Advance functions).
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <angelscript.h>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Components/GameEventRouter.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/Components/Script/SceneScriptEngine.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextEventPlan.h"
#include "Objects/Components/Collider/BoxCollider.h"
#include "Objects/Components/GameEventListener.h"
#include "Objects/Components/GameplayComponents.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Utilities/KeyframeAnimation.h"
#include "ShortTextTestSceneManager.h"

using namespace KashipanEngine;
using GameEvents::Value;
namespace {
int failures = 0;
void Check(bool value, const std::string &message) {
    if (!value) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
bool Near(double a, double b, double e = 1e-4) { return std::abs(a - b) <= e; }
void ScriptDiagnostic(const asSMessageInfo *message, void *) {
    std::printf("SCRIPT: %s(%d,%d): %s\n", message->section, message->row, message->col, message->message);
}
const auto kUpdateKey = std::bit_cast<Passkey<EmptyObject>>(std::uint8_t{});

struct Applied { std::unique_ptr<IEditorCommand> command; ShortTextComponentPlan plan; };
Applied Apply(SceneEditorContext &context, EmptyObject *target, const std::string &text) {
    const auto parsed = ShortText::ParseEventRequest(text);
    Require(parsed.recognized && parsed.success, "parse: " + text + " / " + parsed.message);
    Applied applied;
    const auto before = context.GetSceneObjects().size();
    applied.command = MakeShortTextEventCommand(&context, target, parsed.request, text, &applied.plan);
    Require(applied.command && applied.plan.success, "plan: " + text + " / " + applied.plan.message);
    Require(context.GetSceneObjects().size() == before, "planning creates nothing: " + text);
    Require(applied.command->Execute(&context), "execute: " + text);
    return applied;
}
void RejectWithoutMutation(Scene &scene, SceneEditorContext &context, EmptyObject *target, const std::string &text, const std::string &expect = "") {
    const auto before = scene.SaveToJSON().dump();
    const auto parsed = ShortText::ParseEventRequest(text);
    std::string message = parsed.message;
    if (parsed.recognized && parsed.success) {
        ShortTextComponentPlan plan;
        Check(!MakeShortTextEventCommand(&context, target, parsed.request, text, &plan) && !plan.success, "must reject: " + text);
        message = plan.message;
    } else Check(parsed.recognized && !parsed.message.empty(), "rejected with a message: " + text);
    Check(!message.empty() && (expect.empty() || message.find(expect) != std::string::npos), "rejection reason for 「" + text + "」: " + message);
    Check(scene.SaveToJSON().dump() == before, "rejection must not mutate: " + text);
}
bool Mentions(const std::vector<std::string> &lines, const std::string &word) {
    for (const auto &line : lines) if (line.find(word) != std::string::npos) return true;
    return false;
}
template <class T> size_t Count(const EmptyObject *object) { return object ? object->GetComponents<T>().size() : 0; }
EmptyObject *Box(SceneContext *sc, const std::string &name, const std::string &tag, const Vector3 &position) {
    auto *object = sc->CreateEmptyObject(name);
    object->SetTag(tag);
    object->GetComponent<Transform>()->SetTranslate(position);
    object->AddComponent<BoxCollider>();
    return object;
}
void AddClip(EmptyObject *object, const std::string &name) {
    auto *animator = object->GetComponent<KeyFrameAnimator>();
    if (!animator) animator = object->AddComponent<KeyFrameAnimator>();
    KeyframeAnimation clip;
    clip.AddKeyframe(0, 0);
    clip.AddKeyframe(1, 1);
    Require(animator->AddAnimationData(name, clip.SaveToJson()), "clip " + name);
}
JSON Script(const EmptyObject *object, const std::string &field) {
    JSON value;
    object->GetComponent<ScriptComponent>()->GetSerializedVariable(field, value);
    return value;
}
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto original = std::filesystem::current_path();
    const auto sandbox = std::filesystem::temp_directory_path() /
        ("KashipanCustomEvents-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox);
    std::filesystem::current_path(sandbox);
    int result = 0;
    try {
        auto manager = ShortTextTest::MakeSceneManager();
        Scene scene(std::string("CustomEventRegression"));
        ShortTextTest::AttachSceneManager(scene, *manager);
        SceneEditorContext context(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &scene);
        auto *sc = scene.GetSceneContext();
        sc->AddComponent<SceneObjectCollider>();
        auto *hero = Box(sc, "Hero", "Player", Vector3(0, 0, 0));
        const auto heroID = hero->GetObjectID();

        //==================================================
        // A. Event definitions by short text (router added, undo/redo, no duplicates, hand-made protected)
        //==================================================
        {
            const auto before = scene.SaveToJSON().dump();
            const auto parsed = ShortText::ParseEventRequest("GoalReachedイベントを作成");
            ShortTextComponentPlan plan;
            auto command = MakeShortTextEventCommand(&context, hero, parsed.request, "GoalReachedイベントを作成", &plan);
            Check(command && plan.success && Mentions(plan.steps, "GameEventRouter を追加") && Mentions(plan.steps, "GoalReached"), "define plan preview");
            Check(scene.SaveToJSON().dump() == before && !context.GetComponent<GameEventRouter>(), "planning is side-effect free");
        }
        auto defineGoal = Apply(context, hero, "GoalReachedイベントを作成");
        auto *router = context.GetComponent<GameEventRouter>();
        Require(router && router->FindDefinition("GoalReached"), "router added with GoalReached");
        // PlayStop rebuilds the scene from the edit-time snapshot: raw pointers are fetched again afterwards.
        auto stop = [&]() {
            scene.PlayStop();
            router = context.GetComponent<GameEventRouter>();
            hero = sc->GetSceneObject(heroID);
            Require(router && hero && !scene.IsPlaying(), "scene restored after play stop");
        };
        Check(router->FindDefinition("goalreached") == router->FindDefinition("GoalReached"), "case-insensitive lookup");
        Check(defineGoal.command->Undo(&context) && !context.GetComponent<GameEventRouter>(), "undo removes the added router");
        Check(defineGoal.command->Execute(&context) && context.GetComponent<GameEventRouter>() && context.GetComponent<GameEventRouter>()->FindDefinition("GoalReached"), "redo re-adds");
        router = context.GetComponent<GameEventRouter>();
        Apply(context, hero, "goalreachedイベントを作成");
        Check(router->GetDefinitions().size() == 1, "re-application (any case) does not duplicate");
        auto defineDamage = Apply(context, hero, "Damageイベントを作成、amountは数値");
        Check(router->GetDefinitions().size() == 2 && router->FindDefinition("Damage")->args.size() == 1 && router->FindDefinition("Damage")->args[0].required &&
            router->FindDefinition("Damage")->shortTextManaged, "Damage(amount: number) defined and managed");
        {
            auto changed = Apply(context, hero, "Damageイベントを作成、amountは整数");
            Check(router->FindDefinition("Damage")->args[0].type == GameEvents::ArgType::Int && router->GetDefinitions().size() == 2, "managed definition updated in place");
            Check(changed.command->Undo(&context) && router->FindDefinition("Damage")->args[0].type == GameEvents::ArgType::Number, "undo restores the schema");
        }
        {
            GameEvents::Definition manual;
            manual.name = "Hit";
            manual.args.push_back({"power", GameEvents::ArgType::Number, true, nullptr});
            Require(router->AddDefinition(manual), "hand-made definition");
            RejectWithoutMutation(scene, context, hero, "Hitイベントを作成、powerは整数", "手作り");
            Check(!router->AddDefinition(manual), "duplicate name rejected by the router");
            GameEvents::Definition bad; bad.name = "a b";
            Check(!router->AddDefinition(bad), "invalid name rejected by the router");
        }
        for (const char *name : {"Ping", "Pong", "Died", "Picked", "Expired", "Spawned", "Limit", "Opened", "Respawned", "Changed", "Choice", "Ended", "Moved"})
            Apply(context, hero, std::string(name) + "イベントを作成");
        Apply(context, hero, "Scoreイベントを作成、valueは整数、labelは省略可能な文字列、dirはVector3、whoは省略可能なオブジェクト");
        Apply(context, hero, "Hurtイベントを作成、amountは数値、senderはオブジェクト");

        //==================================================
        // B. Router: typed payloads, multiple deliveries, target selection, limits, safety
        //==================================================
        {
            std::vector<GameEvents::Message> seen;
            const auto observer = router->Subscribe("Score", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &m) { seen.push_back(m); });
            std::string error;
            Check(router->Emit("Score", Value{{"value", 1}, {"dir", {0, 1, 0}}, {"who", hero->GetObjectID().ToString()}}, "") &&
                router->Emit("score", Value{{"value", 2}, {"dir", {1, 0, 0}}}, ""), "two typed sends of the same event in one update");
            Check(!router->Emit("Score", Value{{"value", 1.5}, {"dir", {0, 1, 0}}}, "", GameEvents::TargetMode::Broadcast, "", &error) && error.find("value") != std::string::npos, "type mismatch is not sent");
            Check(!router->Emit("Score", Value{{"dir", {0, 1, 0}}, {"value", 1}, {"extra", 1}}, "", GameEvents::TargetMode::Broadcast, "", &error), "undeclared argument is not sent");
            Check(!router->Emit("Nope", Value::object(), "", GameEvents::TargetMode::Broadcast, "", &error), "unknown event is not sent");
            Check(router->GetQueuedCount() == 2, "only valid messages are queued");
            Check(router->ProcessQueue() == 2 && seen.size() == 2, "both messages delivered separately (not collapsed into a counter)");
            Check(seen.size() == 2 && seen[0].payload["value"] == 1 && seen[1].payload["value"] == 2 && seen[0].serial < seen[1].serial && seen[0].name == "Score", "order, payloads and stored spelling");
            Check(seen.size() == 2 && !seen[1].Has("who") && seen[0].Has("who"), "absent optional argument stays absent");
            Check(router->Unsubscribe(observer), "unsubscribe observer");
        }
        // Target selection with real components as owners (stable ComponentRef + object identity).
        auto *a = Box(sc, "A", "Enemy", Vector3(50, 0, 0));
        auto *b = Box(sc, "B", "Enemy", Vector3(60, 0, 0));
        auto *c = Box(sc, "C", "NPC", Vector3(70, 0, 0));
        std::map<std::string, int> received;
        for (auto *object : {a, b, c}) {
            const auto name = object->GetName();
            router->Subscribe("Ping", object->GetComponent<Transform>()->GetComponentRef(), [&received, name](IObjectComponent *receiver, const GameEvents::Message &) {
                Require(receiver != nullptr, "receiver resolved"); ++received[name]; }, "test");
        }
        auto deliver = [&](GameEvents::TargetMode mode, const std::string &value) { received.clear(); Require(router->Emit("Ping", Value::object(), "", mode, value), "emit ping"); router->ProcessQueue(); };
        deliver(GameEvents::TargetMode::Broadcast, "");
        Check(received["A"] == 1 && received["B"] == 1 && received["C"] == 1, "broadcast reaches every subscriber");
        deliver(GameEvents::TargetMode::Object, a->GetObjectID().ToString());
        Check(received["A"] == 1 && received["B"] == 0 && received["C"] == 0, "object UUID target");
        deliver(GameEvents::TargetMode::Name, "B");
        Check(received["A"] == 0 && received["B"] == 1, "name target");
        deliver(GameEvents::TargetMode::Tag, "Enemy");
        Check(received["A"] == 1 && received["B"] == 1 && received["C"] == 0, "tag target");
        b->GetComponent<Transform>()->SetActive(false);
        deliver(GameEvents::TargetMode::Broadcast, "");
        Check(received["B"] == 0 && received["A"] == 1, "inactive receiver is skipped");
        b->GetComponent<Transform>()->SetActive(true);
        {
            const auto cID = c->GetObjectID().ToString();
            Require(sc->DeleteObject(c), "delete C");
            std::string error;
            Check(!router->Emit("Ping", Value::object(), "", GameEvents::TargetMode::Object, cID, &error), "deleted UUID target is rejected at send time");
            const auto subscriptions = router->GetSubscriptionCount();
            deliver(GameEvents::TargetMode::Broadcast, "");
            Check(received.count("C") == 0 && router->GetSubscriptionCount() == subscriptions - 1, "subscription of a deleted object is dropped");
        }
        // Names select exactly one object (fixed to its UUID when queued); tags select many.
        {
            Box(sc, "Dup", "X", Vector3(80, 0, 0)); Box(sc, "Dup", "X", Vector3(81, 0, 0));
            const auto queued = router->GetQueuedCount();
            std::string error;
            Check(!router->Emit("Ping", Value::object(), "", GameEvents::TargetMode::Name, "Dup", &error) && error.find("複数") != std::string::npos && router->GetQueuedCount() == queued, "duplicate name rejected without queueing");
            Check(!router->Emit("Ping", Value::object(), "", GameEvents::TargetMode::Name, "Nobody", &error) && router->GetQueuedCount() == queued, "missing name rejected without queueing");
            Check(!router->Emit("Ping", Value::object(), "", static_cast<GameEvents::TargetMode>(9), "x", &error) && router->GetQueuedCount() == queued, "out-of-range target mode rejected");
            received.clear();
            Require(router->Emit("Ping", Value::object(), "", GameEvents::TargetMode::Name, "A"), "unique name accepted");
            auto *renamed = sc->CreateEmptyObject("A2");
            (void)renamed;
            router->ProcessQueue();
            Check(received["A"] == 1 && received["B"] == 0, "name resolved to one object");
        }
        // Handlers that delete, unsubscribe and resubscribe during delivery.
        {
            int afterDelete = 0;
            auto *victim = sc->CreateEmptyObject("Victim");
            router->Subscribe("Pong", a->GetComponent<Transform>()->GetComponentRef(), [&](IObjectComponent *, const GameEvents::Message &) {
                sc->DeleteObject(victim);
                router->UnsubscribeOwner(a->GetComponent<Transform>()->GetComponentRef(), "pong");
                router->Subscribe("Pong", b->GetComponent<Transform>()->GetComponentRef(), [&](IObjectComponent *, const GameEvents::Message &) { ++afterDelete; }, "late");
            }, "pong");
            router->Subscribe("Pong", victim->GetComponent<Transform>()->GetComponentRef(), [&](IObjectComponent *, const GameEvents::Message &) { ++afterDelete; }, "victim");
            router->Subscribe("Pong", ComponentRef{}, [](IObjectComponent *, const GameEvents::Message &) { throw std::runtime_error("handler failure"); }, "throws");
            int afterThrow = 0;
            router->Subscribe("Pong", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++afterThrow; }, "afterThrow");
            Require(router->Emit("Pong", Value::object(), ""), "emit pong");
            router->ProcessQueue();
            Check(afterDelete == 0, "deleted receiver is not called; a subscription added during delivery waits for the next message");
            Check(afterThrow == 1 && Mentions(std::vector<std::string>(router->GetDiagnostics().begin(), router->GetDiagnostics().end()), "handler failure"), "exception becomes a diagnostic and delivery continues");
            Require(router->Emit("Pong", Value::object(), ""), "emit pong again");
            router->ProcessQueue();
            Check(afterDelete == 1, "resubscribed handler receives the next message");
            router->UnsubscribeOwner(b->GetComponent<Transform>()->GetComponentRef(), "late");
            router->UnsubscribeOwner(ComponentRef{}, "throws");
            router->UnsubscribeOwner(ComponentRef{}, "afterThrow");
        }
        // A→B→A cycles end by the depth limit; the per-update cap keeps one frame bounded.
        {
            for (auto *object : {a, b}) router->UnsubscribeOwner(object->GetComponent<Transform>()->GetComponentRef());
            int pings = 0, pongs = 0;
            router->Subscribe("Ping", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++pings; router->Emit("Pong", Value::object(), ""); }, "cycle");
            router->Subscribe("Pong", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++pongs; router->Emit("Ping", Value::object(), ""); }, "cycle");
            Require(router->Emit("Ping", Value::object(), ""), "start cycle");
            size_t frames = 0;
            while (router->GetQueuedCount() && frames < 10) { router->ProcessQueue(); ++frames; }
            Check(router->GetQueuedCount() == 0 && frames == 1 && pings + pongs == router->GetLimits().maxDepth + 1, "cycle stopped by the depth limit (" + std::to_string(pings + pongs) + ")");
            Check(Mentions(std::vector<std::string>(router->GetDiagnostics().begin(), router->GetDiagnostics().end()), "連鎖"), "depth limit diagnostic");
            router->UnsubscribeOwner(ComponentRef{}, "cycle");
            int flood = 0;
            router->Subscribe("Ping", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++flood; }, "flood");
            for (int i = 0; i < 300; ++i) router->Emit("Ping", Value::object(), "");
            Check(router->ProcessQueue() == static_cast<size_t>(router->GetLimits().maxPerUpdate) && router->GetQueuedCount() == 300 - static_cast<size_t>(router->GetLimits().maxPerUpdate), "per-update delivery cap");
            router->ProcessQueue();
            Check(flood == 300, "remaining messages delivered next update");
            router->UnsubscribeOwner(ComponentRef{}, "flood");
            GameEventRouter::Limits tooBig; tooBig.maxDepth = 1000;
            Check(!router->SetLimits(tooBig), "limits validated");
        }
        // Scene switching: another scene's router is independent; no delivery crosses scenes.
        {
            Scene other(std::string("Other"));
            auto *otherRouter = other.GetSceneContext()->AddComponent<GameEventRouter>();
            GameEvents::Definition ping; ping.name = "Ping";
            Require(otherRouter->AddDefinition(ping), "other definition");
            int here = 0;
            router->Subscribe("Ping", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++here; }, "here");
            Require(otherRouter->Emit("Ping", Value::object(), ""), "emit in other scene");
            otherRouter->ProcessQueue();
            router->ProcessQueue();
            Check(here == 0 && GameEventRouter::Find(other.GetSceneContext()) == otherRouter && GameEventRouter::Find(sc) == router, "routers are per scene");
            router->UnsubscribeOwner(ComponentRef{}, "here");
        }

        // Router removal and replacement (also at the same address): listeners subscribe again by generation.
        {
            auto *probe = sc->CreateEmptyObject("Probe");
            auto *listener = probe->AddComponent<GameEventListener>();
            GameEventListener::Settings settings;
            GameEventListener::Handler handler;
            handler.id = "probe"; handler.event = "Ping";
            handler.actions.push_back({GameEventListener::ActionKind::SetProperty, "", "Transform.translate_.x", {{"value", {GameEventListener::SourceKind::Fixed, 1.0, ""}}}});
            settings.handlers.push_back(handler);
            Require(listener->SetSettings(settings), "probe listener");
            listener->Advance(0.016);
            Require(router->Emit("Ping", Value::object(), ""), "ping probe");
            router->ProcessQueue();
            Check(Near(probe->GetComponent<Transform>()->GetTranslate().x, 1), "probe receives");
            const auto generation = router->GetGeneration();
            const auto definitions = router->GetDefinitions();
            Require(context.RemoveComponent(router), "remove router");
            Check(!context.GetComponent<GameEventRouter>(), "router removed");
            listener->Advance(0.016);
            Check(!listener->GetLastError().empty(), "listener reports the missing router");
            router = context.AddComponent<GameEventRouter>();
            Require(router && router->SetDefinitions(definitions) && router->GetGeneration() != generation, "replacement router has a new generation");
            probe->GetComponent<Transform>()->SetTranslate(Vector3(0, 0, 0));
            listener->Advance(0.016);
            Require(router->Emit("Ping", Value::object(), ""), "ping replacement");
            router->ProcessQueue();
            Check(Near(probe->GetComponent<Transform>()->GetTranslate().x, 1), "listener resubscribed to the replacement router");
            GameEventListener::Settings continuous = settings;
            continuous.handlers[0].actions[0] = {GameEventListener::ActionKind::EngineCommand, "", "accelerated_move", {}};
            Check(!GameEventListener::IsValid(continuous) && !listener->SetSettings(continuous), "continuous engine commands are rejected for event handlers");
            sc->DeleteObject(probe);
        }

        //==================================================
        // C. Short text: contact → emit, receive → keyframe / script, typed send → component command
        //==================================================
        auto *goal = Box(sc, "Goal", "Goal", Vector3(100, 0, 0));
        auto contactApplied = Apply(context, hero, "HeroがGoalに触れたらGoalReachedを発火");
        Check(Count<GameEventListener>(hero) == 1 && hero->GetComponent<GameEventListener>()->GetSettings().handlers.size() == 1 &&
            hero->GetComponent<GameEventListener>()->GetSettings().handlers[0].contactName == "Goal", "contact handler added on Hero");
        RejectWithoutMutation(scene, context, hero, "HeroがGhostに触れたらGoalReachedを発火", "Ghost");
        RejectWithoutMutation(scene, context, hero, "HeroがGoalに触れたらUnknownEvを発火", "定義されていません");
        auto tween = Apply(context, goal, "GoalReachedを受け取ったら1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動");
        auto *goalAnimator = goal->GetComponent<KeyFrameAnimator>();
        Require(goalAnimator, "keyframe animator added to Goal");
        const std::string clipName = "ShortText.OnGoalReached.Transform.translate_.y";
        Check(goalAnimator->HasAnimation(clipName) && !goalAnimator->GetPlayOnStart(clipName) && goalAnimator->IsShortTextManaged(clipName), "event-driven clip: no autoplay, managed");
        Check(goal->GetComponent<GameEventListener>() && goal->GetComponent<GameEventListener>()->GetSettings().handlers[0].actions[0].name == "keyframe.play", "receive handler plays the clip");
        auto callApplied = Apply(context, goal, "GoalReachedを受け取ったらOnGoal関数を呼ぶ");
        Check(Mentions(callApplied.plan.requirements, "ScriptComponent"), "missing ScriptComponent is listed, not created");
        Check(goal->GetComponent<GameEventListener>()->GetSettings().handlers.size() == 2, "second handler added to the same listener");
        Apply(context, goal, "GoalReachedを受け取ったらOnGoal関数を呼ぶ");
        Check(goal->GetComponent<GameEventListener>()->GetSettings().handlers.size() == 2 && Count<GameEventListener>(goal) == 1, "re-application does not duplicate handlers");
        // A hand-made handler on the same listener is kept untouched by short-text updates.
        {
            auto settings = goal->GetComponent<GameEventListener>()->GetSettings();
            GameEventListener::Handler manual;
            manual.id = "manual";
            manual.event = "GoalReached";
            manual.actions.push_back({GameEventListener::ActionKind::SetProperty, "", "Transform.translate_.x", {{"value", {GameEventListener::SourceKind::Fixed, 7.0, ""}}}});
            settings.handlers.push_back(manual);
            Require(goal->GetComponent<GameEventListener>()->SetSettings(settings), "manual handler");
            Apply(context, goal, "GoalReachedを受け取ったらOnGoal関数を呼ぶ");
            Check(goal->GetComponent<GameEventListener>()->FindHandler("manual") && goal->GetComponent<GameEventListener>()->GetSettings().handlers.size() == 3, "hand-made handler kept");
        }
        // Script on Goal: OnGoal(), typed-argument call and GameEvent@ receiver with Script emit.
        {
            std::ofstream file("goal.as");
            file << R"(
class Goal : ScriptComponentBehavior {
    [SerializeField] int goals = 0;
    [SerializeField] float total = 0;
    [SerializeField] int received = 0;
    [SerializeField] string lastSender = "";
    [SerializeField] bool subscribed = false;
    [SerializeField] bool defined = false;
    void Awake() {
        Json args; // array of argument definitions
        defined = DefineEvent("Scripted", args, "defined by a script");
        subscribed = SubscribeEvent("Damage", "OnDamage");
    }
    void OnGoal() { goals++; }
    void Take(float amount) { total += amount; }
    void OnDamage(GameEvent@ e) { received++; total += e.GetFloat("amount"); lastSender = e.sender; }
    void Fire() { Json j; j.SetFloat("amount", 7); EmitEvent("Damage", j); }
    void FireBad() { Json j; j.SetString("amount", "x"); EmitEvent("Damage", j); }
}
)";
        }
        auto *scriptEngine = sc->GetComponent<SceneScriptEngine>();
        if (!scriptEngine) scriptEngine = sc->AddComponent<SceneScriptEngine>();
        Require(scriptEngine && scriptEngine->GetEngine(), "script engine initialized");
        scriptEngine->GetEngine()->SetMessageCallback(asFUNCTION(ScriptDiagnostic), nullptr, asCALL_CDECL);
        auto *script = goal->AddComponent<ScriptComponent>();
        script->SetScriptPath("goal.as");
        script->ReloadFromDisk();
        Require(script->GetLastError().empty(), "script compiled: " + script->GetLastError());
        Check(Script(goal, "subscribed") == true && Script(goal, "defined") == true && router->FindDefinition("Scripted"), "script DefineEvent / SubscribeEvent");

        scene.PlayStart();
        Require(scene.IsPlaying(), "playing");
        auto *heroListener = hero->GetComponent<GameEventListener>();
        auto *goalListener = goal->GetComponent<GameEventListener>();
        heroListener->Advance(0.016); goalListener->Advance(0.016);
        router->ProcessQueue();
        Check(!goalAnimator->IsPlaying(clipName) && Script(goal, "goals") == 0, "nothing happens before contact");
        goal->GetComponent<Transform>()->SetTranslate(Vector3(0.5f, 0, 0));
        heroListener->Advance(0.016);
        Check(router->GetQueuedCount() == 1, "contact enter emits GoalReached once");
        router->ProcessQueue();
        Check(goalAnimator->IsPlaying(clipName), "GoalReached plays the keyframe clip");
        Check(Script(goal, "goals") == 1, "GoalReached calls OnGoal()");
        Check(Near(goal->GetComponent<Transform>()->GetTranslate().x, 7), "hand-made SetProperty handler ran with its fixed value");
        heroListener->Advance(0.016);
        Check(router->GetQueuedCount() == 0, "staying in contact does not re-emit");
        // Script → router → script receiver (GameEvent@) with typed getters and sender.
        Require(script->InvokeMethod("Fire"), "Fire()");
        router->ProcessQueue();
        Check(Script(goal, "received") == 1 && Near(Script(goal, "total").get<double>(), 7) && Script(goal, "lastSender") == goal->GetObjectID().ToString(), "script emit and GameEvent@ receive");
        Require(script->InvokeMethod("FireBad"), "FireBad()");
        Check(router->GetQueuedCount() == 0, "script payload with a wrong type is not sent");
        stop();

        // A script receiver deleting its own object while handling an event, outside the update loop.
        {
            std::ofstream("selfdelete.as") << R"(
class SelfDelete : ScriptComponentBehavior {
    void Awake() { SubscribeEvent("Ping", "OnPing"); }
    void OnPing(GameEvent@ e) { GetScene().DeleteObject(GetOwnerObject()); EmitEvent("Ping"); }
    void Die() { GetScene().DeleteObject(GetOwnerObject()); EmitEvent("Ping"); }
}
)";
            auto *doomed = sc->CreateEmptyObject("Doomed");
            const auto doomedID = doomed->GetObjectID();
            auto *doomedScript = doomed->AddComponent<ScriptComponent>();
            doomedScript->SetScriptPath("selfdelete.as");
            doomedScript->ReloadFromDisk();
            Require(doomedScript->GetLastError().empty(), "self delete script: " + doomedScript->GetLastError());
            int after = 0;
            router->Subscribe("Ping", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++after; }, "after");
            Require(router->Emit("Ping", Value::object(), ""), "ping self delete");
            router->ProcessQueue();
            Check(!sc->GetSceneObject(doomedID) && after == 1, "self-deleting script receiver: deleted after delivery, other receivers still run");
            router->UnsubscribeOwner(ComponentRef{}, "after");
            auto *doomed2 = sc->CreateEmptyObject("Doomed2");
            const auto doomed2ID = doomed2->GetObjectID();
            auto *script2 = doomed2->AddComponent<ScriptComponent>();
            script2->SetScriptPath("selfdelete.as");
            script2->ReloadFromDisk();
            auto *listener = doomed2->AddComponent<GameEventListener>();
            GameEventListener::Settings settings;
            GameEventListener::Handler handler;
            handler.id = "die"; handler.trigger = GameEventListener::Trigger::Start;
            handler.actions.push_back({GameEventListener::ActionKind::CallScript, "", "Die", {}});
            handler.actions.push_back({GameEventListener::ActionKind::Emit, "", "Ping", {}});
            settings.handlers.push_back(handler);
            Require(listener->SetSettings(settings), "die listener");
            const auto queued = router->GetQueuedCount();
            listener->Advance(0.016);
            Check(!sc->GetSceneObject(doomed2ID) && router->GetQueuedCount() == queued, "listener whose script deletes the object stops its remaining actions safely");
            router->ClearQueue();
        }
        {
            Apply(context, hero, "Greetingイベントを作成、textは文字列");
            auto *literalSender = sc->CreateEmptyObject("LiteralSender");
            Apply(context, literalSender, "Greetingイベントを全体に、textを「Hello world、Ｆｏｏ　bar」として送る");
            Value actual;
            const auto subscription = router->Subscribe("Greeting", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &message) { actual = message.payload.at("text"); }, "literal");
            literalSender->GetComponent<GameEventListener>()->Advance(0.016);
            router->ProcessQueue();
            Check(actual == "Hello world、Ｆｏｏ　bar", "quoted string delivered without changing spaces, width or punctuation");
            router->Unsubscribe(subscription);
            sc->DeleteObject(literalSender);
        }
        // Damage with a typed payload into Health (receiver) and from a sender's start handler.
        auto *enemy = Box(sc, "Enemy", "Enemy", Vector3(200, 0, 0));
        const auto enemyID = enemy->GetObjectID();
        auto receiveDamage = Apply(context, enemy, "Damageを受け取ったら体力をamountだけ減らす");
        Require(enemy->GetComponent<Health>() && enemy->GetComponent<GameEventListener>(), "Health auto-added for the command and listener added");
        Check(Mentions(receiveDamage.plan.steps, "Health を追加"), "plan lists the added Health");
        Apply(context, enemy, "Damageを受け取ったらTake関数をamountを渡して呼ぶ");
        RejectWithoutMutation(scene, context, enemy, "Damageを受け取ったら体力をpowerだけ減らす", "power");
        RejectWithoutMutation(scene, context, enemy, "GoalReachedを受け取ったら体力をamountだけ減らす", "amount");
        auto send = Apply(context, hero, "Damageイベントを対象Enemyに、amountを10として送る");
        {
            const auto *handler = &hero->GetComponent<GameEventListener>()->GetSettings().handlers.back();
            Check(handler->trigger == GameEventListener::Trigger::Start && handler->actions[0].emitTarget == GameEvents::TargetMode::Object &&
                handler->actions[0].emitTargetValue == enemy->GetObjectID().ToString() && handler->actions[0].args[0].second.value == 10.0, "send handler stores the UUID and a typed value");
        }
        RejectWithoutMutation(scene, context, hero, "Damageイベントを対象Enemyに、powerを10として送る", "power");
        RejectWithoutMutation(scene, context, hero, "Damageイベントを対象Enemyに、amountを「abc」として送る", "数値");
        RejectWithoutMutation(scene, context, hero, "Damageイベントを対象Enemyに送る", "必須");
        Box(sc, "Twin", "T", Vector3(300, 0, 0)); Box(sc, "Twin", "T", Vector3(310, 0, 0));
        RejectWithoutMutation(scene, context, hero, "Damageイベントを対象Twinに、amountを1として送る", "一意");
        Apply(context, hero, "Damageイベントで対象Enemyに25を渡す");
        Check(hero->GetComponent<GameEventListener>()->GetSettings().handlers.size() == 2, "only-argument form updates the same start handler");
        {
            auto *script2 = enemy->AddComponent<ScriptComponent>();
            std::ofstream("enemy.as") << "class E : ScriptComponentBehavior { [SerializeField] float total = 0; void Take(float amount) { total += amount; } }";
            script2->SetScriptPath("enemy.as");
            script2->ReloadFromDisk();
            Require(script2->GetLastError().empty(), "enemy script");
        }
        scene.PlayStart();
        heroListener = hero->GetComponent<GameEventListener>();
        auto *enemyListener = enemy->GetComponent<GameEventListener>();
        enemyListener->Advance(0.016);
        heroListener->Advance(0.016);
        router->ProcessQueue();
        Check(Near(enemy->GetComponent<Health>()->GetCurrent(), 75), "typed payload amount=25 reduced Health 100→75");
        Check(Near(Script(enemy, "total").get<double>(), 25), "typed payload passed to Take(float)");
        heroListener->Advance(0.016);
        router->ProcessQueue();
        Check(Near(enemy->GetComponent<Health>()->GetCurrent(), 75), "start handler runs once");
        stop();
        enemy = sc->GetSceneObject(enemyID);
        Check(enemy && Near(enemy->GetComponent<Health>()->GetCurrent(), 100), "play stop restores edit-time values");

        //==================================================
        // D. The ten gameplay categories
        //==================================================
        // 1. Health / damage / heal / death / cooldown
        auto *fighter = Box(sc, "Fighter", "Fighter", Vector3(400, 0, 0));
        auto healthApplied = Apply(context, fighter, "体力を100にして、Enemyに触れたら10ダメージ");
        auto *health = fighter->GetComponent<Health>();
        auto *contact = fighter->GetComponent<ContactDamage>();
        Require(health && contact, "Health + ContactDamage");
        Check(Near(health->GetMax(), 100) && Near(health->GetCurrent(), 100) && contact->GetSettings().otherTag == "Enemy" && Near(contact->GetSettings().amount, 10), "configured values");
        Apply(context, fighter, "体力を120にして、Enemyに触れたら15ダメージ");
        Check(Count<Health>(fighter) == 1 && Count<ContactDamage>(fighter) == 1 && Near(fighter->GetComponent<Health>()->GetMax(), 120), "re-application updates, no duplicates");
        {
            auto *manual = sc->CreateEmptyObject("ManualHealth");
            manual->AddComponent<Health>();
            RejectWithoutMutation(scene, context, manual, "体力を100にする", "手作り");
        }
        scene.PlayStart();
        health = fighter->GetComponent<Health>(); contact = fighter->GetComponent<ContactDamage>();
        {
            auto s = health->GetSettings();
            s.deathEvent = "Died"; s.damagedEvent = "Changed";
            Require(health->SetSettings(s), "death event");
            int deaths = 0, damaged = 0;
            router->Subscribe("Died", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++deaths; }, "t");
            router->Subscribe("Changed", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &m) { ++damaged; (void)m; }, "t");
            std::string error;
            Check(health->Damage(10, "") && Near(health->GetCurrent(), 110), "damage");
            Check(!health->Damage(10, "", &error) && Near(health->GetCurrent(), 110), "invulnerability blocks repeated damage");
            health->Advance(0.6);
            Check(health->Damage(30, "") && Near(health->GetCurrent(), 80), "damage after cooldown");
            Check(health->Heal(500, "") && Near(health->GetCurrent(), 120), "heal clamps to max");
            Check(!health->Damage(std::numeric_limits<float>::quiet_NaN(), "") && !health->Damage(-1, "") && !health->Heal(0, ""), "invalid amounts rejected");
            health->Advance(0.6);
            Check(health->Damage(1000, "") && health->IsDead() && Near(health->GetCurrent(), 0), "death");
            health->Advance(0.6);
            Check(!health->Damage(1, "") && !health->Heal(1, ""), "dead: no damage or heal");
            router->ProcessQueue();
            Check(deaths == 1 && damaged == 3, "death notified once, damage events per hit (" + std::to_string(damaged) + ")");
            health->Revive();
            Check(!health->IsDead() && Near(health->GetCurrent(), 120), "revive");
            // An object argument accepts "" as "no object": engine-originated damage has no sender.
            {
                auto hurt = s; hurt.damagedEvent = "Hurt"; hurt.deathEvent.clear();
                Require(health->SetSettings(hurt), "hurt event");
                std::vector<std::string> senders;
                router->Subscribe("Hurt", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &m) { std::string v = "?"; m.GetString("sender", v); senders.push_back(v); }, "t");
                health->Advance(1);
                Check(health->Damage(1, ""), "damage without sender");
                health->Advance(1);
                Check(health->Damage(1, heroID.ToString()), "damage with sender");
                router->ProcessQueue();
                Check(senders.size() == 2 && senders[0].empty() && senders[1] == heroID.ToString(), "Hurt(sender: object) delivered with empty and real sender");
                health->Revive();
            }
            router->UnsubscribeOwner(ComponentRef{}, "t");
            // Real contact with an Enemy-tagged box: one hit per contact window.
            auto *spike = Box(sc, "Spike", "Enemy", Vector3(400.5f, 0, 0));
            health->Advance(1);
            Check(contact->Advance(0.016) == 1 && Near(health->GetCurrent(), 105), "real contact damage (15)");
            Check(contact->Advance(0.016) == 0 && Near(health->GetCurrent(), 105), "continued contact does not repeat");
            health->Advance(1);
            Check(contact->Advance(0.6) == 1 && Near(health->GetCurrent(), 90), "next window after cooldown");
            sc->DeleteObject(spike);
        }
        stop();
        Check(healthApplied.command != nullptr, "health command kept");

        // 2. Pickup + 7. Inventory (once, removal, recipient inventory)
        auto *coin = Box(sc, "Coin", "Item", Vector3(500, 0, 0));
        auto pickupApplied = Apply(context, coin, "Playerに触れたらコインを1増やして消える");
        Check(coin->GetComponent<Pickup>() && hero->GetComponent<Inventory>() && Mentions(pickupApplied.plan.steps, "Inventory"), "Pickup added, the only Player gets an Inventory");
        Check(pickupApplied.command->Undo(&context) && !coin->GetComponent<Pickup>() && !hero->GetComponent<Inventory>(), "undo removes both (external object included)");
        Check(pickupApplied.command->Execute(&context) && coin->GetComponent<Pickup>() && hero->GetComponent<Inventory>(), "redo");
        scene.PlayStart();
        {
            auto *coinNow = sc->GetSceneObject("Coin");
            const auto coinID = coinNow->GetObjectID();
            Check(!coinNow->GetComponent<Pickup>()->Advance() && hero->GetComponent<Inventory>()->Count("コイン") == 0, "no pickup without contact");
            coinNow->GetComponent<Transform>()->SetTranslate(Vector3(0.3f, 0, 0));
            Check(coinNow->GetComponent<Pickup>()->Advance(), "picked on contact");
            Check(hero->GetComponent<Inventory>()->Count("コイン") == 1 && !sc->GetSceneObject(coinID), "inventory +1 and coin deleted");
            auto *stay = sc->CreateEmptyObject("Stay");
            auto *p = stay->AddComponent<Pickup>();
            auto s = p->GetSettings(); s.after = 0; s.itemId = "Gem"; s.amount = 2;
            Require(p->SetSettings(s), "stay pickup");
            std::string error;
            Check(p->TryPickup(hero, &error) && !p->TryPickup(hero, &error) && hero->GetComponent<Inventory>()->Count("Gem") == 2, "picked only once");
            auto *npc = sc->CreateEmptyObject("Npc"); npc->SetTag("Player");
            auto *q = sc->CreateEmptyObject("Q")->AddComponent<Pickup>();
            Check(!q->TryPickup(npc, &error) && error.find("Inventory") != std::string::npos, "recipient without Inventory: explained, nothing created");
        }
        stop();
        {
            auto *inventory = hero->GetComponent<Inventory>();
            auto s = inventory->GetSettings(); s.capacity = 2; s.maxStack = 5; s.saveKey = "hero"; s.changedEvent = "Changed";
            Require(inventory->SetSettings(s), "inventory settings");
            int changes = 0;
            router->Subscribe("Changed", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++changes; }, "inv");
            std::string error;
            Check(inventory->Add("鍵", 1) && inventory->Add("剣", 3) && inventory->Has("鍵") && inventory->Count("剣") == 3, "add multiple kinds");
            Check(!inventory->Add("盾", 1, "", &error) && !inventory->Has("盾"), "kind capacity");
            Check(!inventory->Add("剣", 3, "", &error) && inventory->Count("剣") == 3, "stack limit leaves the count unchanged");
            Check(inventory->Remove("剣", 2) && inventory->Count("剣") == 1 && !inventory->Remove("剣", 2, "", &error) && inventory->Count("剣") == 1, "consume and insufficient consume");
            Check(inventory->Remove("鍵", 1) && !inventory->Has("鍵"), "consume to zero removes the kind");
            router->ProcessQueue();
            Check(changes == 4, "change events per successful change (" + std::to_string(changes) + ")");
            router->UnsubscribeOwner(ComponentRef{}, "inv");
            Check(inventory->SaveToGlobal(&error), "save to global: " + error);
            Require(inventory->Add("鍵", 1), "add after save");
            Check(inventory->LoadFromGlobal(&error) && !inventory->Has("鍵") && inventory->Count("剣") == 1, "load from global restores items");
            s = inventory->GetSettings(); s.items.clear(); s.capacity = 32; s.maxStack = 999; s.changedEvent.clear();
            Require(inventory->SetSettings(s), "reset inventory");
        }

        // 3. Lifetime (edit time frozen, dt validation, delete / pool reset)
        auto *shot = sc->CreateEmptyObject("Shot");
        const auto shotID = shot->GetObjectID();
        Apply(context, shot, "生成されてから3秒後に消える");
        auto *pooledObject = sc->CreateEmptyObject("Pooled");
        const auto pooledID = pooledObject->GetObjectID();
        Apply(context, pooledObject, "5秒後に無効化する");
        shot->GetComponent<Lifetime>()->UpdateInterface(kUpdateKey);
        Check(sc->GetSceneObject(shotID) && Near(shot->GetComponent<Lifetime>()->GetElapsed(), 0), "edit time does not advance or delete");
        scene.PlayStart();
        shot = sc->GetSceneObject(shotID);
        {
            auto *lifetime = shot->GetComponent<Lifetime>();
            Check(!lifetime->Advance(std::numeric_limits<double>::quiet_NaN()) && !lifetime->Advance(-1) && !lifetime->Advance(100) && Near(lifetime->GetElapsed(), 0), "invalid dt ignored");
            Check(!lifetime->Advance(2.9) && sc->GetSceneObject(shotID), "alive before 3 s");
            Check(lifetime->Advance(0.2) && !sc->GetSceneObject(shotID), "deleted after 3 s");
            auto *pooled = sc->GetSceneObject(pooledID);
            auto *poolLifetime = pooled->GetComponent<Lifetime>();
            Check(poolLifetime && !poolLifetime->Advance(4.9) && poolLifetime->Advance(0.2) && !pooled->IsActive() && sc->GetSceneObject(pooledID), "deactivated (pool release)");
            pooled->SetActive(true);
            Check(!poolLifetime->Advance(1) && pooled->IsActive() && Near(poolLifetime->GetElapsed(), 1), "reactivation restarts the timer");
            Check(!poolLifetime->Advance(3.5) && pooled->IsActive(), "counts from reactivation");
            Check(poolLifetime->Advance(1) && !pooled->IsActive(), "expires again 5 s after reactivation");
        }
        stop();

        // 4. Spawner (template by UUID, max alive, per-update cap, live UUIDs, pool reuse, self-replication guard)
        auto *enemyTemplate = Box(sc, "EnemyTemplate", "Enemy", Vector3(600, 0, 0));
        enemyTemplate->SetActive(false);
        auto *spawnerObject = sc->CreateEmptyObject("SpawnPoint");
        auto spawnApplied = Apply(context, spawnerObject, "2秒ごとにEnemyTemplateを生成し、最大5体にする");
        auto *spawner = spawnerObject->GetComponent<Spawner>();
        Require(spawner, "Spawner");
        Check(spawner->GetSettings().templateObject == enemyTemplate->GetObjectID().ToString() && spawner->GetSettings().maxAlive == 5 && Near(spawner->GetSettings().interval, 2), "template stored by UUID");
        RejectWithoutMutation(scene, context, spawnerObject, "2秒ごとにNoSuchを生成し、最大5体にする", "NoSuch");
        RejectWithoutMutation(scene, context, spawnerObject, "2秒ごとにSpawnPointを生成し、最大5体にする", "自分");
        {
            auto *recursive = sc->CreateEmptyObject("Recursive");
            recursive->AddComponent<Spawner>();
            RejectWithoutMutation(scene, context, spawnerObject, "2秒ごとにRecursiveを生成し、最大5体にする", "Spawner");
        }
        const auto objectsBefore = sc->GetSceneObjects().size();
        scene.PlayStart();
        spawner = sc->GetSceneObject("SpawnPoint")->GetComponent<Spawner>();
        {
            int limitEvents = 0;
            auto s = spawner->GetSettings(); s.limitEvent = "Limit";
            Require(spawner->SetSettings(s), "limit event");
            router->Subscribe("Limit", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++limitEvents; }, "sp");
            Check(spawner->Advance(1.0) == 0 && spawner->Advance(1.0) == 1, "spawns every 2 seconds");
            const auto aliveBeforeInvalidDt = spawner->GetAliveCount();
            Check(spawner->Advance(100) == 0 && spawner->GetAliveCount() == aliveBeforeInvalidDt, "invalid dt is rejected without spawning");
            Check(spawner->Advance(10) == 1 && spawner->Advance(0) == 0, "per-update cap drops the backlog");
            for (int i = 0; i < 10; ++i) spawner->Advance(2.0);
            Check(spawner->GetAliveCount() == 5 && sc->GetSceneObjects().size() == objectsBefore + 5, "max alive 5");
            router->ProcessQueue();
            Check(limitEvents == 1, "limit event once while at the limit");
            const auto first = spawner->GetSpawned().front();
            Require(sc->DeleteObject(sc->GetSceneObject(first)), "delete one spawned");
            Check(spawner->GetAliveCount() == 4 && spawner->Advance(2.0) == 1 && spawner->GetAliveCount() == 5, "live UUIDs: a deleted one is replaced");
            auto *spawned = sc->GetSceneObject(spawner->GetSpawned().back());
            Check(spawned && !spawned->GetComponent<Spawner>() && spawned->IsActive(), "spawned clone is active and has no Spawner");
            spawned->SetActive(false);
            const auto count = sc->GetSceneObjects().size();
            Check(spawner->Advance(2.0) == 1 && sc->GetSceneObjects().size() == count && spawned->IsActive(), "inactive spawned object is reused from the pool");
            router->UnsubscribeOwner(ComponentRef{}, "sp");
            auto *copy = sc->CloneObject(sc->GetSceneObject("SpawnPoint"), "SpawnCopy", false);
            Check(copy && copy->GetComponent<Spawner>() && copy->GetComponent<Spawner>()->GetSpawned().empty(), "a cloned Spawner does not inherit live objects");
        }
        stop();
        Check(sc->GetSceneObjects().size() == objectsBefore, "play stop removes spawned objects");

        // 5. Interact (distance + E, open clip, missing clip requirement)
        auto *door = Box(sc, "Door", "Door", Vector3(2, 0, 0));
        AddClip(door, "Open");
        Apply(context, door, "近づいてEキーを押したら扉を開く");
        Check(door->GetComponent<Interactable>() && door->GetComponent<Interactable>()->GetSettings().keyframeClip == "Open" && door->GetComponent<Interactable>()->GetSettings().key == Key::E, "door uses its Open clip");
        auto *plainDoor = sc->CreateEmptyObject("PlainDoor");
        auto plainApplied = Apply(context, plainDoor, "近づいてEキーを押したら扉を開く");
        Check(Mentions(plainApplied.plan.requirements, "Open"), "missing open clip is requested");
        scene.PlayStart();
        {
            auto *interact = sc->GetSceneObject("Door")->GetComponent<Interactable>();
            auto *animator = sc->GetSceneObject("Door")->GetComponent<KeyFrameAnimator>();
            Check(!interact->Advance(false) && !animator->IsPlaying("Open"), "no key, nothing");
            Check(interact->Advance(true) && animator->IsPlaying("Open"), "E within 2 → Open plays");
            animator->Stop("Open");
            hero->GetComponent<Transform>()->SetTranslate(Vector3(-10, 0, 0));
            Check(!interact->Advance(true) && !animator->IsPlaying("Open"), "too far → nothing");
            hero->GetComponent<Transform>()->SetTranslate(Vector3(0, 0, 0));
            auto *plain = sc->GetSceneObject("PlainDoor")->GetComponent<Interactable>();
            sc->GetSceneObject("PlainDoor")->GetComponent<Transform>()->SetTranslate(Vector3(1, 0, 0));
            std::string error;
            Check(!plain->Interact(hero, &error) && error.find("未設定") != std::string::npos, "no open method: explained");
        }
        stop();

        // 6. Checkpoint / respawn (fall, velocity reset)
        auto *runner = Box(sc, "Runner", "Runner", Vector3(700, 0, 0));
        runner->AddComponent<Velocity>();
        Apply(context, runner, "触れた場所を復活地点にして、落下したら戻る");
        auto *flag = Box(sc, "Flag", "Checkpoint", Vector3(720, 5, 0));
        flag->AddComponent<Checkpoint>();
        scene.PlayStart();
        {
            auto *r = sc->GetSceneObject("Runner");
            auto *respawner = r->GetComponent<Respawner>();
            Check(!respawner->Advance() && respawner->HasPoint() && Near(respawner->GetPoint().x, 700), "start position is the first point");
            r->GetComponent<Transform>()->SetTranslate(Vector3(720.2f, 5, 0));
            respawner->Advance();
            Check(Near(respawner->GetPoint().x, 720) && Near(respawner->GetPoint().y, 5), "touching the checkpoint updates the point");
            r->GetComponent<Transform>()->SetTranslate(Vector3(730, -20, 0));
            r->GetComponent<Velocity>()->SetVelocity(Vector3(3, -9, 0));
            Check(respawner->Advance(), "fall below -10 respawns");
            const auto p = r->GetComponent<Transform>()->GetTranslate();
            const auto v = r->GetComponent<Velocity>()->GetVelocity();
            Check(Near(p.x, 720) && Near(p.y, 5) && Near(v.x, 0) && Near(v.y, 0), "back at the checkpoint with zero velocity");
        }
        stop();

        // 7. Key door: pickup on the key object, item check, consume not required
        auto *keyDoor = Box(sc, "Gate", "Door", Vector3(-3, 0, 0));
        AddClip(keyDoor, "Open");
        auto *keyObject = Box(sc, "鍵", "Item", Vector3(800, 0, 0));
        auto keyApplied = Apply(context, keyDoor, "鍵を取得し、鍵を持っていたら扉を開ける");
        Check(keyDoor->GetComponent<Interactable>()->GetSettings().requiredItem == "鍵" && keyObject->GetComponent<Pickup>() && keyObject->GetComponent<Pickup>()->GetSettings().itemId == "鍵", "door requires 鍵; the key object becomes a Pickup");
        scene.PlayStart();
        {
            auto *gate = sc->GetSceneObject("Gate");
            int denied = 0;
            auto s = gate->GetComponent<Interactable>()->GetSettings(); s.deniedEvent = "Opened";
            Require(gate->GetComponent<Interactable>()->SetSettings(s), "denied event");
            router->Subscribe("Opened", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++denied; }, "kd");
            std::string error;
            Check(!gate->GetComponent<Interactable>()->Interact(hero, &error) && !gate->GetComponent<KeyFrameAnimator>()->IsPlaying("Open"), "without the key: denied");
            router->ProcessQueue();
            Check(denied == 1, "denied event");
            auto *k = sc->GetSceneObject("鍵");
            k->GetComponent<Transform>()->SetTranslate(Vector3(0.2f, 0, 0));
            Check(k->GetComponent<Pickup>()->Advance() && hero->GetComponent<Inventory>()->Has("鍵"), "key picked up");
            Check(gate->GetComponent<Interactable>()->Interact(hero, &error) && gate->GetComponent<KeyFrameAnimator>()->IsPlaying("Open"), "with the key: opens");
            router->UnsubscribeOwner(ComponentRef{}, "kd");
        }
        stop();

        // 8. Dialogue (order, choices, branch, end notice; text is never generated)
        auto *npc = Box(sc, "Elder", "NPC", Vector3(900, 0, 0));
        auto dialogueApplied = Apply(context, npc, "話しかけたら文章を順番に表示する");
        Check(npc->GetComponent<Dialogue>() && npc->GetComponent<Dialogue>()->GetSettings().lines.empty() && npc->GetComponent<Interactable>()->GetSettings().startDialogue, "dialogue + interactable, no generated text");
        Check(Mentions(dialogueApplied.plan.requirements, "文章"), "text requirement shown");
        {
            auto *dialogue = npc->GetComponent<Dialogue>();
            std::string error;
            Check(!dialogue->Start(&error) && error.find("未入力") != std::string::npos, "cannot start without text");
            Dialogue::Settings s = dialogue->GetSettings();
            s.lines = {{"長老", "ようこそ", {}, -2}, {"長老", "手伝うか？", {{"はい", 2}, {"いいえ", 3}}, -2}, {"長老", "ありがとう", {}, -1}, {"長老", "残念だ", {}, -2}};
            s.choiceEvent = "Choice"; s.endedEvent = "Ended";
            Require(dialogue->SetSettings(s), "dialogue lines");
            int choice = -1, ended = 0;
            router->Subscribe("Choice", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &m) { int v = -1; (void)m; choice = 1; (void)v; }, "dl");
            router->Subscribe("Ended", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++ended; }, "dl");
            Check(dialogue->Start() && dialogue->GetLineIndex() == 0 && dialogue->GetDisplayText().find("ようこそ") != std::string::npos, "first line");
            Check(dialogue->Advance() && dialogue->GetLineIndex() == 1 && dialogue->GetDisplayText().find("1. はい") != std::string::npos, "second line with choices");
            Check(!dialogue->Advance(&error) && dialogue->GetLineIndex() == 1, "choices block advance");
            Check(dialogue->Choose(1) && dialogue->GetLineIndex() == 3, "choice 2 branches to line 4");
            Check(dialogue->Advance() && !dialogue->IsRunning(), "past the last line ends");
            router->ProcessQueue();
            Check(choice == 1 && ended == 1, "choice and end notices");
            dialogue->Start(); dialogue->Advance(); dialogue->HandleInput(false, 0);
            Check(dialogue->GetLineIndex() == 2, "key choice 1 → line 3");
            dialogue->HandleInput(true, -1);
            Check(!dialogue->IsRunning(), "next -1 ends");
            router->UnsubscribeOwner(ComponentRef{}, "dl");
        }

        // 9. Animation state switching (no restart of the same state)
        auto *walker = sc->CreateEmptyObject("Walker");
        AddClip(walker, "Walk"); AddClip(walker, "Idle");
        Apply(context, walker, "移動中はWalk、停止中はIdleを再生する");
        auto *switcher = walker->GetComponent<AnimationStateSwitcher>();
        Require(switcher && switcher->GetSettings().states.size() == 2 && switcher->GetSettings().states[0].player == 0, "switcher with KeyFrameAnimator states");
        {
            auto *animator = walker->GetComponent<KeyFrameAnimator>();
            Check(switcher->Evaluate(0) && switcher->GetCurrentState() == "Idle" && animator->IsPlaying("Idle") && switcher->GetPlayCount() == 1, "idle");
            Check(!switcher->Evaluate(0.05f) && switcher->GetPlayCount() == 1, "same state does not restart");
            Check(switcher->Evaluate(2) && switcher->GetCurrentState() == "Walk" && animator->IsPlaying("Walk") && !animator->IsPlaying("Idle"), "walk, idle stopped");
            Check(!switcher->Evaluate(3) && switcher->GetPlayCount() == 2, "walking keeps playing without restart");
            walker->AddComponent<Velocity>()->SetVelocity(Vector3(0, 0, 0));
            Check(switcher->Advance(0.016) && switcher->GetCurrentState() == "Idle", "velocity drives the state");
            auto *bare = sc->CreateEmptyObject("Bare");
            auto bareApplied = Apply(context, bare, "移動中はRun、停止中はStandを再生する");
            Check(Mentions(bareApplied.plan.requirements, "再生先が未設定"), "unknown clips are requested, not guessed");
            Check(!bare->GetComponent<AnimationStateSwitcher>()->Evaluate(1) && !bare->GetComponent<AnimationStateSwitcher>()->GetLastError().empty(), "missing clip reported");
        }

        //==================================================
        // E. Save / load / clone / object JSON (prefab path) / undo of the whole first application
        //==================================================
        {
            const JSON saved = scene.SaveToJSON();
            Scene restored(JSON::parse(saved.dump()));
            auto *restoredRouter = restored.GetSceneContext()->GetComponent<GameEventRouter>();
            Check(restoredRouter && GameEvents::EncodeDefinitions(restoredRouter->GetDefinitions()) == GameEvents::EncodeDefinitions(router->GetDefinitions()), "definitions saved and loaded");
            enemy = sc->GetSceneObject(enemyID);
            auto *restoredEnemy = restored.GetSceneContext()->GetSceneObject(enemyID);
            Check(restoredEnemy && restoredEnemy->GetComponent<GameEventListener>() &&
                GameEventListener::EncodeSettings(restoredEnemy->GetComponent<GameEventListener>()->GetSettings()) == GameEventListener::EncodeSettings(enemy->GetComponent<GameEventListener>()->GetSettings()), "listener saved and loaded");
            for (const auto *name : {"Fighter", "SpawnPoint", "Door", "Runner", "Elder", "Walker"}) {
                auto *source = sc->GetSceneObject(name);
                auto *copy = restored.GetSceneContext()->GetSceneObject(source->GetObjectID());
                Check(copy && copy->GetAllComponents().size() == source->GetAllComponents().size(), std::string("components restored: ") + name);
                for (size_t i = 0; copy && i < source->GetAllComponents().size(); ++i) {
                    auto *component = source->GetAllComponents()[i].first;
                    auto *restoredComponent = copy->GetAllComponents()[i].first;
                    Check(source->SaveComponentToJson(component) == copy->SaveComponentToJson(restoredComponent), std::string("component JSON round trip: ") + name + "." + component->GetComponentType());
                }
            }
            auto *clone = sc->CloneObject(enemy, "EnemyClone", false);
            Check(clone && GameEventListener::EncodeSettings(clone->GetComponent<GameEventListener>()->GetSettings()) == GameEventListener::EncodeSettings(enemy->GetComponent<GameEventListener>()->GetSettings()) &&
                clone->GetComponent<Health>(), "clone copies listener and Health");
            Scene prefabScene(std::string("PrefabTarget"));
            auto *instance = prefabScene.GetSceneContext()->CreateObjectFromJson(sc->SaveObjectToJson(sc->GetSceneObject("Elder")));
            Check(instance && instance->GetComponent<Dialogue>() && instance->GetComponent<Dialogue>()->GetSettings().lines.size() == 4 && instance->GetComponent<Interactable>(), "object JSON (prefab path) keeps components");
            // A clone receives broadcast events through its own subscription.
            scene.PlayStart();
            auto *cloneNow = sc->GetSceneObject("EnemyClone");
            cloneNow->GetComponent<GameEventListener>()->Advance(0.016);
            sc->GetSceneObject("Enemy")->GetComponent<GameEventListener>()->Advance(0.016);
            Require(router->Emit("Damage", Value{{"amount", 5}}, ""), "broadcast damage");
            router->ProcessQueue();
            Check(Near(cloneNow->GetComponent<Health>()->GetCurrent(), 95) && Near(sc->GetSceneObject("Enemy")->GetComponent<Health>()->GetCurrent(), 95), "clone and original both receive");
            stop();
        }
        // Undo of an application made before several play sessions still works (IDs, not pointers).
        {
            const auto handlers = hero->GetComponent<GameEventListener>()->GetSettings().handlers.size();
            Check(send.command->Undo(&context) && sc->GetSceneObject(heroID)->GetComponent<GameEventListener>()->GetSettings().handlers.size() == handlers - 1, "undo removes the send handler");
            Check(send.command->Execute(&context) && sc->GetSceneObject(heroID)->GetComponent<GameEventListener>()->GetSettings().handlers.size() == handlers, "redo adds it again");
            hero = sc->GetSceneObject(heroID);
        }
        Check(contactApplied.command != nullptr && tween.command != nullptr && defineDamage.command != nullptr && spawnApplied.command != nullptr && keyApplied.command != nullptr, "commands kept");

        // Legacy event.emit: the counter still increments; a defined event of the same name is routed once.
        {
            ShortText::CommandContext legacy;
            legacy.owner = hero; legacy.target = hero; legacy.scene = sc;
            int routed = 0;
            router->Subscribe("GoalReached", ComponentRef{}, [&](IObjectComponent *, const GameEvents::Message &) { ++routed; }, "legacy");
            std::string error;
            Check(ShortText::ExecuteMediaCommand("event.emit", Value{{"name", "GoalReached"}}, legacy, error), "legacy emit: " + error);
            router->ProcessQueue();
            Check(sc->GetSceneVariable("__shortTextEvent.GoalReached")->AnyCast<int>() == 1 && routed == 1, "legacy counter + one routed delivery");
            Check(!ShortText::ExecuteMediaCommand("event.emit", Value{{"name", "Damage"}}, legacy, error) && !sc->GetSceneVariable("__shortTextEvent.Damage"), "invalid routed payload: legacy counter unchanged");
            Check(ShortText::ExecuteMediaCommand("event.emit", Value{{"name", "undefinedLegacy"}}, legacy, error) && sc->GetSceneVariable("__shortTextEvent.undefinedLegacy")->AnyCast<int>() == 1, "undefined legacy name keeps the counter-only behaviour");
            router->UnsubscribeOwner(ComponentRef{}, "legacy");
        }
    } catch (const std::exception &e) {
        std::printf("EXCEPTION: %s\n", e.what());
        result = 1;
    }
    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(sandbox, ignored);
    if (failures) result = 1;
    std::printf(result ? "ShortTextCustomEventRegression: FAIL (%d)\n" : "ShortTextCustomEventRegression: PASS\n", failures);
    return result;
}
