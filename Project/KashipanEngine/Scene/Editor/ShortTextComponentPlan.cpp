#include "Scene/Editor/ShortTextComponentPlan.h"
#ifdef USE_IMGUI
#include <algorithm>
#include <map>
#include <sstream>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/PropertyMotion.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/SceneEditorContext.h"
#include "Utilities/KeyframeAnimation.h"

namespace KashipanEngine {
namespace {
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;

// Components that work with default values and need no asset, reference or shape decision.
bool IsSelfContained(const std::string &type) {
    for (const char *name : {"Transform", "Velocity", "BoxCollider", "Box2DCollider", "SphereCollider", "Circle2DCollider"})
        if (type == name) return true;
    return false;
}
bool IsRegisteredComponent(const std::string &type) {
    const auto &types = GetRegisteredObjectComponentTypes();
    return std::find(types.begin(), types.end(), type) != types.end();
}
std::string Number(double value) {
    std::ostringstream out; out << value; return out.str();
}
std::string RequestPath(const ShortText::PropertyRequest &r) {
    return r.component + "." + r.member + (r.axis >= 0 ? std::string(".") + "xyz"[r.axis] : "");
}
// Simple legacy recipes add to Transform each frame: Oscillate/Move → translate_, Rotate → rotate_.
bool LegacyDrives(const ShortText::Recipe &recipe, const std::string &member, int axis) {
    if (!ShortText::IsValid(recipe) || recipe.kind == ShortText::Kind::Rules || recipe.axis != axis) return false;
    return member == (recipe.kind == ShortText::Kind::Rotate ? "rotate_" : "translate_");
}
// Whether a saved KeyFrameAnimator entry writes one channel of the first component of a type.
bool EntryDrives(const JSON &entry, const std::string &component, const std::string &member, int axis) {
    if (!entry.is_object() || !entry.contains("bindings") || !entry["bindings"].is_array()) return false;
    const int channel = std::max(0, axis);
    for (const auto &binding : entry["bindings"]) {
        if (!binding.is_object() || binding.value("isScriptVariable", false)) continue;
        if (binding.value("componentType", std::string{}) == component && binding.value("componentIndex", 0) == 0 &&
            binding.value("parameterName", std::string{}) == member && (axis == -2 || binding.value("channel", 0) == channel)) return true;
    }
    return false;
}
bool EntryManaged(const JSON &entry) {
    return entry.is_object() && entry.contains("shortTextManaged") && entry["shortTextManaged"].is_boolean() && entry["shortTextManaged"].get<bool>();
}
// Short-text keyframe entries driving one channel are removed; with `replacement` the first one is updated
// in place (keeping its name) instead. Hand-made entries are kept and reported. `group` may be null (plan only).
// Returns true when an entry was replaced.
bool EditShortTextKeyframes(CompositeCommand *group, const EmptyObject *target, const std::string &component, const std::string &member,
    int axis, const JSON *replacement, const std::string &describe, ShortTextComponentPlan &result, std::string *replacedName = nullptr) {
    if (!target) return false;
    bool replaced = false;
    int manual = 0;
    for (auto *animator : target->GetComponents<KeyFrameAnimator>()) {
        const JSON before = target->SaveComponentToJson(animator);
        if (!before.contains("data") || !before["data"].contains("customData")) continue;
        const auto &custom = before["data"]["customData"];
        if (!custom.is_object() || !custom.contains("animations") || !custom["animations"].is_array()) continue;
        JSON kept = JSON::array();
        bool changed = false;
        for (const auto &entry : custom["animations"]) {
            if (!EntryDrives(entry, component, member, axis)) { kept.push_back(entry); continue; }
            const std::string name = entry.value("name", std::string{});
            if (!EntryManaged(entry)) { ++manual; kept.push_back(entry); continue; }
            changed = true;
            if (replacement && !replaced) {
                JSON next = *replacement;
                next["name"] = name;
                kept.push_back(std::move(next));
                replaced = true;
                if (replacedName) *replacedName = name;
                result.steps.push_back("既存の KeyFrameAnimator の生成アニメーション「" + name + "」を更新: " + describe);
            } else {
                result.steps.push_back("同じ項目を動かす短文由来のキーフレームアニメーション「" + name + "」を削除（二重に動かさないため）");
            }
        }
        if (!changed || !group) continue;
        JSON after = before;
        after["data"]["customData"]["animations"] = std::move(kept);
        group->AddCommand(std::make_unique<ComponentEditCommand>(const_cast<EmptyObject *>(target), animator, before, after));
    }
    if (manual > 0)
        result.requirements.push_back("手動で作成された KeyFrameAnimator のアニメーションが同じ項目（" + component + "." + member +
            "）に書き込んでいます（" + std::to_string(manual) + " 件、削除しません）。");
    return replaced;
}
// One KeyFrameAnimator entry holding a real KeyframeAnimation: from at 0 s (with the ease), to at duration.
JSON MakeShortTextTweenEntry(const ShortText::PropertyRequest &r, const std::string &name, const std::string &sourceText, bool playOnStart) {
    EaseType ease = EaseType::Linear;
    if (!TryStringToEaseType(r.ease, ease)) return JSON();
    KeyframeAnimation keys;
    keys.AddKeyframe(0.0f, r.from, ease);
    keys.AddKeyframe(r.duration, r.to, EaseType::Linear);
    KeyFrameAnimator builder; // detached: only used to produce the saved entry format
    if (!builder.AddAnimationData(name, keys.SaveToJson()) ||
        !builder.AddBinding(name, r.component, r.member, std::max(0, r.axis), 0, false) ||
        !builder.SetLoop(name, false) || !builder.SetPlayOnStart(name, playOnStart) ||
        !builder.SetValueScale(name, r.unit == ShortText::PropertyUnit::Degrees ? static_cast<float>(kDegreesToRadians) : 1.0f) ||
        !builder.SetShortTextSource(name, true, sourceText)) return JSON();
    return builder.SaveAnimationEntryToJson(name);
}
std::vector<PropertyMotion *> Motions(const EmptyObject *target) {
    std::vector<PropertyMotion *> motions;
    if (target) for (const auto &[component, addedID] : target->GetAllComponents()) {
        (void)addedID;
        if (auto *motion = dynamic_cast<PropertyMotion *>(component)) motions.push_back(motion);
    }
    return motions;
}

// Writes one registered member through the same safe adapter used at runtime. Undo restores the snapshot.
class ShortTextPropertyWriteCommand final : public IEditorCommand {
public:
    ShortTextPropertyWriteCommand(const UUID128 &objectID, std::string type, std::string path, ShortText::Value value)
        : objectID_(objectID), type_(std::move(type)), path_(std::move(path)), value_(std::move(value)) {}
    bool Execute(SceneEditorContext *context) override {
        SetAppliedChangesAfterFailure(false);
        auto *object = context ? context->GetSceneObject(objectID_) : nullptr;
        auto *component = FindComponentOfType(object, type_);
        if (!object || !component) return false;
        before_ = object->SaveComponentToJson(component);
        if (ShortText::WriteExtendedProperty(object, path_, value_)) return true;
        SetAppliedChangesAfterFailure(!object->LoadComponentFromJson(component, before_));
        return false;
    }
    bool Undo(SceneEditorContext *context) override {
        auto *object = context ? context->GetSceneObject(objectID_) : nullptr;
        auto *component = FindComponentOfType(object, type_);
        return object && component && object->LoadComponentFromJson(component, before_);
    }
    std::string GetName() const override { return "短文でプロパティを設定: " + path_; }
private:
    UUID128 objectID_;
    std::string type_;
    std::string path_;
    ShortText::Value value_;
    JSON before_;
};
} // namespace

ShortText::PropertySchema MakeShortTextPropertySchema(const EmptyObject *target) {
    // Detached instances only expose registered member metadata; they are never attached to the scene.
    auto detached = std::make_shared<std::map<std::string, std::unique_ptr<IObjectComponent>>>();
    auto inspect = [target, detached](const std::string &type) -> const IObjectComponent * {
        if (auto *existing = FindComponentOfType(target, type)) return existing;
        auto found = detached->find(type);
        if (found == detached->end()) found = detached->emplace(type, IsRegisteredComponent(type) ? CreateObjectComponentByType(type) : nullptr).first;
        return found->second.get();
    };
    ShortText::PropertySchema schema;
    schema.resolveComponent = [](const std::string &lower) -> std::string {
        for (const auto &type : GetRegisteredObjectComponentTypes())
            if (type != "PropertyMotion" && ShortText::AsciiLower(type) == lower) return type;
        return {};
    };
    schema.resolveMember = [inspect](const std::string &type, const std::string &lower) -> std::string {
        const auto *component = inspect(type);
        if (!component) return {};
        for (const auto &[name, variable] : component->GetAllMemberVariables()) {
            (void)variable;
            const auto key = ShortText::AsciiLower(name);
            if (key == lower || key == lower + "_") return name;
        }
        return {};
    };
    schema.member = [inspect](const std::string &type, const std::string &member) {
        return InspectPropertyMember(inspect(type), member);
    };
    return schema;
}

ShortText::PropertyParseResult ParseShortTextPropertyRequest(const std::string &text, const EmptyObject *target) {
    return ShortText::ParsePropertyRequest(text, MakeShortTextPropertySchema(target));
}

std::unique_ptr<IEditorCommand> MakeShortTextPropertyCommand(EmptyObject *target, const ShortText::PropertyRequest &request,
    const std::string &sourceText, ShortTextComponentPlan *plan, const ShortTextTweenOptions *tween) {
    ShortTextComponentPlan local;
    const ShortTextTweenOptions defaults;
    const auto &options = tween ? *tween : defaults;
    if (options.appliedName) options.appliedName->clear();
    auto &result = plan ? *plan : local;
    result = {};
    auto fail = [&](const std::string &message) -> std::unique_ptr<IEditorCommand> { result.success = false; result.message = message; return nullptr; };
    if (!target) return fail("対象のオブジェクトをひとつ選択してください。");
    if (sourceText.size() > 512) return fail("文章は512バイト以下にしてください。");
    if (!IsRegisteredComponent(request.component) || request.component == "PropertyMotion") return fail(request.component + " は登録されたコンポーネントではありません。");
    std::string message;
    if (!ShortText::ValidatePropertyRequest(request, MakeShortTextPropertySchema(target).member(request.component, request.member), message)) return fail(message);
    if (request.positiveOnly != ShortText::IsPositiveMember(request.component, request.member)) return fail("値の制約が項目と一致しません。再解釈してください。");
    if (request.unit == ShortText::PropertyUnit::Degrees && !ShortText::IsAngleMember(request.component, request.member)) return fail("度の指定は Transform の回転にだけ使えます。");

    auto group = std::make_unique<CompositeCommand>("短文からコンポーネントを設定");
    const std::string path = RequestPath(request);
    if (!FindComponentOfType(target, request.component)) {
        group->AddCommand(std::make_unique<AddComponentCommand>(target, request.component));
        result.steps.push_back(request.component + " を追加（既定値）");
        if (!IsSelfContained(request.component))
            result.requirements.push_back(request.component + " は既定値で追加します。素材・参照先などは推測しないため、Inspectorで設定してください。");
    } else result.steps.push_back("既存の " + request.component + " を使用");

    PropertyMotion::Settings key;
    key.component = request.component; key.componentIndex = 0; key.member = request.member; key.axis = request.axis;
    std::vector<PropertyMotion *> sameTarget;
    for (auto *motion : Motions(target)) {
        const auto &settings = motion->GetSettings();
        const bool wholeMember = !request.values.empty() && settings.component == request.component && settings.componentIndex == 0 && settings.member == request.member;
        if (wholeMember || motion->HasSameTarget(key)) sameTarget.push_back(motion);
    }
    // Managed (short-text) motions come first so a re-application updates them instead of adding another.
    std::stable_partition(sameTarget.begin(), sameTarget.end(), [](PropertyMotion *m) { return m->GetSettings().shortTextManaged; });

    // Legacy ShortTextBehavior that moves the same Transform channel is replaced (PingPong / Tween).
    auto replaceLegacy = [&]() {
        if (auto *behavior = target->GetComponent<ShortTextBehavior>()) {
            const int legacyAxis = request.values.empty() ? request.axis : behavior->GetRecipe().axis;
            if (request.component == "Transform" && LegacyDrives(behavior->GetRecipe(), request.member, legacyAxis)) {
                group->AddCommand(std::make_unique<RemoveComponentCommand>(target, behavior));
                result.steps.push_back("同じ項目を動かす既存の短文挙動（" + ShortText::Describe(behavior->GetRecipe()) + "）を削除（二重に動かさないため）");
            } else if (request.component == "Transform" && behavior->GetRecipe().kind == ShortText::Kind::Rules)
                result.requirements.push_back("既存の短文挙動（イベントと動作）も Transform を変更する場合があります。必要に応じて確認してください（削除しません）。");
        }
    };
    auto noteMovers = [&]() {
        if (request.component == "Transform" && request.member == "translate_") {
            for (const char *mover : {"Velocity", "RigidBody2D", "RigidBody3D", "CharacterController2D", "CharacterController3D"})
                if (FindComponentOfType(target, mover)) result.requirements.push_back(std::string(mover) + " も位置を変更します。短文の動きは毎フレーム絶対値で上書きします。");
        }
    };
    const bool degrees = request.unit == ShortText::PropertyUnit::Degrees;
    if (request.mode == ShortText::PropertyMode::Tween) {
        const std::string unit = degrees ? " 度" : "";
        const std::string name = options.entryName.empty() ? "ShortText." + path : options.entryName;
        JSON entry = MakeShortTextTweenEntry(request, name, sourceText, options.playOnStart);
        if (entry.is_null()) return fail("キーフレームアニメーションを生成できませんでした。");
        const auto describe = path + "（ローカル絶対値）キー 0秒=" + Number(request.from) + unit + "（" + request.ease + "）→ " +
            Number(request.duration) + "秒=" + Number(request.to) + unit + (options.playOnStart ? "、ループなし・再生開始時に自動再生・終了後は " : "、ループなし・自動再生しない（イベントで再生）・終了後は ") +
            Number(request.to) + unit + " を保持" + (degrees ? "、キーは度で保存し valueScale=π/180 でラジアンへ変換" : "");
        std::string applied = name;
        if (EditShortTextKeyframes(group.get(), target, request.component, request.member, request.axis, &entry, describe, result, &applied)) {
            if (options.appliedName) *options.appliedName = applied;
        } else {
            const auto animators = target->GetComponents<KeyFrameAnimator>();
            if (!animators.empty()) {
                auto *animator = animators.front();
                std::string unique = name;
                for (int n = 2; animator->HasAnimation(unique); ++n) unique = name + "#" + std::to_string(n);
                entry["name"] = unique;
                if (options.appliedName) *options.appliedName = unique;
                const JSON before = target->SaveComponentToJson(animator);
                JSON after = before;
                auto &list = after["data"]["customData"]["animations"];
                if (!list.is_array()) list = JSON::array();
                list.push_back(entry);
                group->AddCommand(std::make_unique<ComponentEditCommand>(target, animator, before, after));
                result.steps.push_back("既存の KeyFrameAnimator にアニメーション「" + unique + "」を追加（手作りのアニメーションはそのまま）: " + describe);
            } else {
                const auto detached = CreateObjectComponentByType("KeyFrameAnimator");
                if (!detached) return fail("KeyFrameAnimator を作成できません。");
                const JSON state{{"priority", detached->GetUpdatePriority()}, {"isActive", true}, {"tag", ""},
                    {"customData", JSON{{"animations", JSON::array({entry})}}}};
                group->AddCommand(std::make_unique<AddComponentCommand>(target, "KeyFrameAnimator", state));
                if (options.appliedName) *options.appliedName = name;
                result.steps.push_back("KeyFrameAnimator を追加し、アニメーション「" + name + "」を登録: " + describe);
            }
        }
        for (auto *motion : sameTarget) {
            if (motion->GetSettings().shortTextManaged) {
                group->AddCommand(std::make_unique<RemoveComponentCommand>(target, motion));
                result.steps.push_back("同じ項目を往復させていた短文由来の PropertyMotion を削除（二重に動かさないため）");
            } else result.requirements.push_back("手動で設定された PropertyMotion が再生中に " + path + " を上書きします（削除しません）。");
        }
        replaceLegacy();
        noteMovers();
        result.requirements.push_back(std::string("編集中の値は変わりません。") + (options.playOnStart ? "再生開始フレームで " : "再生要求を受けたフレームで ") + Number(request.from) + unit + " から始まり、" +
            Number(request.duration) + " 秒で " + Number(request.to) + unit + " に到達して止まり、その値を保持します。停止すると再生前の値に戻ります。");
    } else if (request.mode == ShortText::PropertyMode::Set) {
        EditShortTextKeyframes(group.get(), target, request.component, request.member, request.values.empty() ? request.axis : -2, nullptr, {}, result);
        ShortText::Value value;
        if (request.values.empty()) value = degrees ? request.from * kDegreesToRadians : request.from;
        else {
            value = ShortText::Value::array();
            for (float componentValue : request.values) value.push_back(degrees ? componentValue * kDegreesToRadians : componentValue);
        }
        group->AddCommand(std::make_unique<ShortTextPropertyWriteCommand>(target->GetObjectID(), request.component, path, ShortText::Value(value)));
        result.steps.push_back(ShortText::DescribePropertyRequest(request) + "（Inspectorで確認できます）");
        for (auto *motion : sameTarget) {
            if (motion->GetSettings().shortTextManaged) {
                group->AddCommand(std::make_unique<RemoveComponentCommand>(target, motion));
                result.steps.push_back("同じ項目を往復させていた短文由来の PropertyMotion を削除（固定値と二重に動かさないため）");
            } else result.requirements.push_back("手動で設定された PropertyMotion が再生中に " + path + " を上書きします（削除しません）。");
        }
        if (!request.values.empty()) replaceLegacy();
    } else {
        PropertyMotion::Settings settings = key;
        settings.from = request.from; settings.to = request.to; settings.period = request.period;
        settings.unit = request.unit; settings.wave = request.wave;
        settings.shortTextManaged = true; settings.sourceText = sourceText;
        if (!PropertyMotion::IsValid(settings)) return fail("PropertyMotion の設定として不正です。");
        EditShortTextKeyframes(group.get(), target, request.component, request.member, request.axis, nullptr, {}, result);
        const auto describe = path + " を " + Number(request.from) + "〜" + Number(request.to) +
            (request.unit == ShortText::PropertyUnit::Degrees ? " 度" : "") + " で往復、1往復 " + Number(request.period) + " 秒、" +
            (request.wave == ShortText::PropertyWave::Smooth ? "なめらか" : "一定速度");
        if (!sameTarget.empty()) {
            auto *reuse = sameTarget.front();
            const JSON before = target->SaveComponentToJson(reuse);
            JSON after = before;
            after["data"]["customData"] = EncodePropertyMotionSettings(settings);
            after["data"]["isActive"] = true;
            group->AddCommand(std::make_unique<ComponentEditCommand>(target, reuse, before, after));
            result.steps.push_back("既存の PropertyMotion を更新: " + describe);
            for (size_t i = 1; i < sameTarget.size(); ++i) if (sameTarget[i]->GetSettings().shortTextManaged) {
                group->AddCommand(std::make_unique<RemoveComponentCommand>(target, sameTarget[i]));
                result.steps.push_back("重複していた短文由来の PropertyMotion を削除");
            }
        } else {
            group->AddCommand(std::make_unique<AddComponentCommand>(target, "PropertyMotion", MakePropertyMotionState(settings)));
            result.steps.push_back("PropertyMotion を追加: " + describe);
        }
        replaceLegacy();
        noteMovers();
        result.requirements.push_back("編集中の値は変わりません。再生中だけ往復し、停止すると再生前の値に戻ります。");
    }
    result.success = true;
    result.message = ShortText::DescribePropertyRequest(request);
    return group;
}

ShortTextRecipeNeeds AnalyzeShortTextRecipeNeeds(const EmptyObject *target, const ShortText::Recipe &recipe) {
    ShortTextRecipeNeeds needs;
    if (!target || recipe.kind != ShortText::Kind::Rules) return needs;
    using Types = std::vector<std::string>;
    auto has = [&](const Types &types) {
        for (const auto &type : types) if (FindComponentOfType(target, type)) return true;
        return false;
    };
    auto add = [&](const std::string &type) { if (std::find(needs.add.begin(), needs.add.end(), type) == needs.add.end()) needs.add.push_back(type); };
    auto note = [&](const std::string &text) { if (std::find(needs.notes.begin(), needs.notes.end(), text) == needs.notes.end()) needs.notes.push_back(text); };
    auto requireAny = [&](const std::string &what, const Types &types) {
        if (has(types)) return;
        std::string names;
        for (const auto &type : types) names += (names.empty() ? "" : " または ") + type;
        note(what + "には " + names + " が必要です。種類・素材・参照先を推測できないため自動では追加しません。");
    };
    auto propertyPath = [&](const std::string &path) {
        const auto end = path.find_first_of(".[");
        if (end == std::string::npos || end == 0) return;
        const auto type = path.substr(0, end);
        if (!IsRegisteredComponent(type) || has({type})) return;
        if (IsSelfContained(type)) add(type);
        else note("プロパティ " + path + " には " + type + " が必要です。自動では追加しません。");
    };
    auto scanData = [&](auto &&self, const ShortText::Value &data) -> void {
        if (data.is_object()) {
            if (data.contains("path") && data["path"].is_string()) propertyPath(data["path"].get<std::string>());
            if (data.contains("read") && data["read"].is_string()) {
                const auto read = data["read"].get<std::string>();
                if (read.starts_with("property:self:")) propertyPath(read.substr(14));
            }
            for (const auto &item : data.items()) self(self, item.value());
        } else if (data.is_array()) for (const auto &item : data) self(self, item);
    };
    if (ShortText::NeedsVelocity(recipe.rules) && !has({"Velocity"})) add("Velocity");
    bool otherTargets = false;
    bool anyCollider = false;
    for (const auto &[component, addedID] : target->GetAllComponents()) { (void)addedID; anyCollider |= dynamic_cast<ICollider *>(component) != nullptr; }
    for (const auto &rule : recipe.rules) {
        if (ShortText::IsContact(rule.event) && !anyCollider)
            note("接触イベントには、この対象と相手の両方にColliderが必要です。形状（Box/Sphere/2Dなど）が決まっていないため自動では追加しません。");
        if (rule.event == ShortText::Event::ButtonClicked) requireAny("ボタンのクリック", {"UIButton"});
        for (const auto &condition : rule.conditions) scanData(scanData, condition.data);
        for (const auto &action : rule.actions) {
            if (action.target != ShortText::Target::Self) { otherTargets = true; continue; }
            using Op = ShortText::Operation;
            switch (action.operation) {
            case Op::PlaySound: case Op::StopSound: case Op::SetVolume: requireAny("音の操作（音声ファイルの設定も必要）", {"AudioSource"}); break;
            case Op::SetText: case Op::ShowVariableText: requireAny("文字の表示", {"TextRenderer", "BitmapTextRenderer"}); break;
            case Op::TweenProperty: propertyPath(action.name); break;
            case Op::EngineCommand: {
                struct Prefix { const char *prefix; Types types; };
                static const Prefix prefixes[] = {
                    {"audio.", {"AudioSource"}}, {"video.", {"VideoSource"}}, {"gif.", {"GifSource"}}, {"animator.", {"Animator"}},
                    {"keyframe.", {"KeyFrameAnimator"}}, {"shake.", {"Shake"}}, {"particles.", {"ParticleSystem2D", "ParticleSystem3D"}},
                    {"rigid_", {"RigidBody2D", "RigidBody3D"}}, {"controller_move", {"CharacterController2D", "CharacterController3D"}},
                    {"text.", {"TextRenderer", "BitmapTextRenderer"}} };
                for (const auto &prefix : prefixes) if (action.name.starts_with(prefix.prefix)) { requireAny("「" + action.name + "」", prefix.types); break; }
                scanData(scanData, action.data);
                break;
            }
            default: break;
            }
        }
    }
    if (otherTargets) note("他のオブジェクトを操作する動作は、適用時には相手の部品を確認しません。");
    return needs;
}

std::vector<PropertyMotion *> FindConflictingShortTextMotions(const EmptyObject *target, const ShortText::Recipe &recipe) {
    std::vector<PropertyMotion *> conflicts;
    for (auto *motion : Motions(target)) {
        const auto &s = motion->GetSettings();
        if (s.shortTextManaged && s.component == "Transform" && s.componentIndex == 0 && LegacyDrives(recipe, s.member, s.axis)) conflicts.push_back(motion);
    }
    return conflicts;
}

void AppendShortTextKeyframeConflicts(CompositeCommand *group, const EmptyObject *target, const ShortText::Recipe &recipe, ShortTextComponentPlan &plan) {
    if (!target || !ShortText::IsValid(recipe) || recipe.kind == ShortText::Kind::Rules || recipe.axis < 0) return;
    const std::string member = recipe.kind == ShortText::Kind::Rotate ? "rotate_" : "translate_";
    ShortTextComponentPlan local;
    EditShortTextKeyframes(group, target, "Transform", member, recipe.axis, nullptr, {}, local);
    // Only removals are reported: hand-made animations of the same member are left to the user.
    for (const auto &step : local.steps) plan.steps.push_back(step);
}

ShortTextComponentPlan PlanShortTextBehavior(const EmptyObject *target, const ShortText::Recipe &recipe) {
    ShortTextComponentPlan plan;
    if (!target || !ShortText::IsValid(recipe)) { plan.message = "適用できない挙動です。"; return plan; }
    const auto needs = AnalyzeShortTextRecipeNeeds(target, recipe);
    for (const auto &type : needs.add) plan.steps.push_back(type + " を追加（既定値）");
    for (auto *motion : FindConflictingShortTextMotions(target, recipe)) {
        (void)motion;
        plan.steps.push_back("同じ項目を往復させていた短文由来の PropertyMotion を削除（二重に動かさないため）");
    }
    AppendShortTextKeyframeConflicts(nullptr, target, recipe, plan);
    plan.steps.push_back(target->GetComponent<ShortTextBehavior>() ? "既存の ShortTextBehavior のレシピを置き換え" : "ShortTextBehavior を追加");
    plan.requirements = needs.notes;
    plan.success = true;
    plan.message = ShortText::Describe(recipe);
    return plan;
}
} // namespace KashipanEngine
#endif
