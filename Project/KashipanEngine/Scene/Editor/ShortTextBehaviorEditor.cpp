#include "Scene/Editor/ShortTextBehaviorEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <cstring>
#include <map>
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/PropertyMotion.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextEventPlan.h"
#include "Scene/SceneEditorContext.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Scene/Editor/EditorHelp.h"

namespace KashipanEngine {
namespace {
void JsonInput(const char *label, ShortText::Value &value) {
    // Keep incomplete edits until the user finishes a JSON value.
    struct Draft { std::string text; std::string saved; };
    static std::map<ImGuiID, Draft> drafts;
    const auto serialized = value.dump(2);
    auto [entry, inserted] = drafts.try_emplace(ImGui::GetID(label), Draft{serialized, serialized});
    (void)inserted;
    if (entry->second.saved != serialized) entry->second = {serialized, serialized};
    auto &draft = entry->second.text;
    if (ImGui::InputTextMultiline(label, &draft, ImVec2(-1, 110))) {
        try {
            auto parsed = ShortText::Value::parse(draft);
            if (ShortText::ValidValue(parsed) && parsed.is_object()) { value = std::move(parsed); entry->second.saved = value.dump(2); }
        } catch (const std::exception &) { }
    }
    try {
        const auto parsed = ShortText::Value::parse(draft);
        if (!parsed.is_object() || !ShortText::ValidValue(parsed)) ImGui::TextWrapped("制限内のJSONオブジェクトを入力してください。");
    } catch (const std::exception &) { ImGui::TextWrapped("JSON編集中です。正しいJSONになると設定に反映します。"); }
}
template <size_t N> bool NamesCombo(const char *label, int &index, const std::array<const char *, N> &names) {
    bool changed = false;
    if (ImGui::BeginCombo(label, names[static_cast<size_t>(index)])) {
        for (size_t i = 0; i < N; ++i) if (ImGui::Selectable(names[i], static_cast<int>(i) == index)) { index = static_cast<int>(i); changed = true; }
        ImGui::EndCombo();
    }
    return changed;
}
void ShowRules(std::vector<ShortText::Rule> &rules) {
    for (size_t i = 0; i < rules.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto &rule = rules[i];
        bool remove = false;
        if (ImGui::TreeNode("rule", "ルール %zu", i + 1)) {
            int event = static_cast<int>(rule.event);
            if (NamesCombo("イベント", event, ShortText::EventLabels)) rule.event = static_cast<ShortText::Event>(event);
            if (rule.event >= ShortText::Event::KeyPressed && rule.event <= ShortText::Event::KeyReleased) {
                if (ImGui::BeginCombo("キー", ShortText::KeyName(rule.key).c_str())) {
                    for (int key = static_cast<int>(Key::A); key <= static_cast<int>(Key::F12); ++key) {
                        const auto name = ShortText::KeyName(static_cast<Key>(key));
                        if (!name.empty() && ImGui::Selectable(name.c_str(), rule.key == static_cast<Key>(key))) rule.key = static_cast<Key>(key);
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::Checkbox("このルールを一度だけ実行", &rule.once);
            ImGui::DragFloat("クールダウン (秒)", &rule.cooldown, 0.05f, 0, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::InputText("実行を許可する状態（空欄は全状態）", &rule.state);
            ImGui::DragFloat("受付開始 (秒)", &rule.windowStart, 0.05f, 0, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("受付終了 (秒、-1は無制限)", &rule.windowEnd, 0.05f, -1, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (rule.event >= ShortText::Event::MousePressed && rule.event <= ShortText::Event::MouseReleased) ImGui::Combo("マウスボタン", &rule.button, "左\0右\0中\0");
            if (rule.event == ShortText::Event::VariableChanged) ImGui::InputText("変化を検出するシーン変数", &rule.variable);
            if (rule.event == ShortText::Event::Timer) ImGui::DragFloat("間隔 (秒)", &rule.interval, 0.05f, 0.05f, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ShortText::IsContact(rule.event)) {
                ImGui::InputText("接触相手のオブジェクトタグ", &rule.tag);
                EditorHelp("空欄は全タグ。対象と相手のColliderが必要です。同じ相手の複数Colliderは1件として扱います。");
            }
            ImGui::TextUnformatted("条件");
            EditorHelp("条件はすべて満たした場合に実行します。条件なしは常に実行します。");
            for (size_t c = 0; c < rule.conditions.size(); ++c) {
                ImGui::PushID(static_cast<int>(c));
                auto &condition = rule.conditions[c];
                int test = static_cast<int>(condition.test);
                if (NamesCombo("条件", test, ShortText::TestLabels)) condition.test = static_cast<ShortText::Test>(test);
                if (condition.test == ShortText::Test::Expression) JsonInput("条件式 JSON", condition.data);
                else if (ShortText::IsVariable(condition.test)) ImGui::InputText("条件のシーン変数名", &condition.name);
                else if (condition.test == ShortText::Test::DistanceAtMost || condition.test == ShortText::Test::DistanceAtLeast) ImGui::InputText("距離を測るオブジェクト名", &condition.name);
                else ImGui::Combo("条件の軸", &condition.axis, "X\0Y\0Z\0");
                ImGui::DragFloat("比較値", &condition.value, 0.1f, -10000, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                const bool erase = ImGui::Button("この条件を削除");
                ImGui::PopID();
                if (erase) { rule.conditions.erase(rule.conditions.begin() + static_cast<std::ptrdiff_t>(c)); break; }
            }
            if (rule.conditions.size() < 4 && ImGui::Button("条件を追加")) rule.conditions.push_back({});
            ImGui::Separator();
            for (size_t a = 0; a < rule.actions.size(); ++a) {
                ImGui::PushID(static_cast<int>(a) + 100);
                auto &action = rule.actions[a];
                int operation = static_cast<int>(action.operation);
                if (NamesCombo("動作", operation, ShortText::OperationLabels)) {
                    action.operation = static_cast<ShortText::Operation>(operation);
                    if (action.operation == ShortText::Operation::SetScale) action.value = 1;
                }
                int target = static_cast<int>(action.target);
                if (NamesCombo("操作対象", target, ShortText::TargetLabels)) action.target = static_cast<ShortText::Target>(target);
                if (action.target >= ShortText::Target::Name) ImGui::InputText("対象の名前 / タグ / ID", &action.targetName);
                if (action.operation >= ShortText::Operation::EngineCommand) {
                    ImGui::InputText("部品名 / プロパティ / 状態", &action.name);
                    JsonInput("部品の引数 JSON", action.data);
                }
                if (action.operation == ShortText::Operation::Follow || action.operation == ShortText::Operation::MoveToward) ImGui::InputText("追従するオブジェクト名", &action.reference);
                if (action.operation >= ShortText::Operation::TweenPosition) ImGui::DragFloat("時間 (秒)", &action.duration, 0.05f, 0.01f, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                if (ShortText::IsVariable(action.operation) || action.operation == ShortText::Operation::ShowVariableText) ImGui::InputText("操作するシーン変数名", &action.name);
                else if (action.operation == ShortText::Operation::CreateObject || action.operation == ShortText::Operation::CloneObject) ImGui::InputText("生成後のオブジェクト名", &action.name);
                else if (action.operation == ShortText::Operation::ChangeScene) ImGui::InputText("遷移先のシーン名", &action.name);
                if (action.operation == ShortText::Operation::SetText || action.operation == ShortText::Operation::ShowVariableText) ImGui::InputText("テキスト / 数値の前に付ける文字", &action.text);
                else if (action.operation <= ShortText::Operation::StopVelocity || action.operation == ShortText::Operation::TweenPosition || action.operation == ShortText::Operation::TweenRotation || action.operation == ShortText::Operation::TweenScale || action.operation == ShortText::Operation::CreateObject || action.operation == ShortText::Operation::CloneObject || action.operation == ShortText::Operation::Follow) ImGui::Combo("操作する軸", &action.axis, "X\0Y\0Z\0");
                const bool toggle = action.operation == ShortText::Operation::SetVisible || action.operation == ShortText::Operation::SetActive;
                if (toggle) { bool enabled = action.value != 0; if (ImGui::Checkbox("有効 / 表示", &enabled)) action.value = enabled ? 1.0f : 0.0f; }
                else if ((action.operation <= ShortText::Operation::SetRotation) || ShortText::IsVariable(action.operation) || action.operation == ShortText::Operation::SetVolume ||
                    action.operation == ShortText::Operation::CreateObject || action.operation == ShortText::Operation::CloneObject || action.operation == ShortText::Operation::Follow ||
                    action.operation == ShortText::Operation::MoveToward || (action.operation >= ShortText::Operation::TweenPosition && action.operation <= ShortText::Operation::TweenScale))
                    ImGui::DragFloat("操作値", &action.value, 0.1f, action.operation == ShortText::Operation::SetScale ? 0.001f : action.operation == ShortText::Operation::SetVolume ? 0.0f : -10000.0f,
                        action.operation == ShortText::Operation::SetScale ? 100.0f : action.operation == ShortText::Operation::SetVolume ? 1.0f : 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                const bool erase = rule.actions.size() > 1 && ImGui::Button("この動作を削除");
                ImGui::PopID();
                if (erase) { rule.actions.erase(rule.actions.begin() + static_cast<std::ptrdiff_t>(a)); break; }
            }
            if (rule.actions.size() < 8 && ImGui::Button("動作を追加")) rule.actions.push_back({});
            remove = ImGui::Button("このルールを削除");
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove) { rules.erase(rules.begin() + static_cast<std::ptrdiff_t>(i)); break; }
    }
    if (rules.size() < 16 && ImGui::Button("ルールを追加")) {
        ShortText::Rule rule; rule.actions.push_back({}); rules.push_back(rule);
    }
}
}
std::unique_ptr<IEditorCommand> MakeShortTextBehaviorCommand(EmptyObject *target,
    const ShortText::Recipe &recipe, const std::string &sourceText) {
    if (!target || !target->GetComponent<Transform>() || !ShortText::IsValid(recipe) || sourceText.size() > 512) return nullptr;
    const JSON state = MakeShortTextBehaviorState(recipe, sourceText);
    auto group = std::make_unique<CompositeCommand>("短文の挙動を適用");
    // Required asset-free components are added at edit time; the rest is listed by PlanShortTextBehavior.
    for (const auto &type : AnalyzeShortTextRecipeNeeds(target, recipe).add)
        group->AddCommand(std::make_unique<AddComponentCommand>(target, type));
    for (auto *motion : FindConflictingShortTextMotions(target, recipe))
        group->AddCommand(std::make_unique<RemoveComponentCommand>(target, motion));
    ShortTextComponentPlan conflicts;
    AppendShortTextKeyframeConflicts(group.get(), target, recipe, conflicts);
    std::unique_ptr<IEditorCommand> behavior;
    if (auto *existing = target->GetComponent<ShortTextBehavior>()) {
        const JSON before = target->SaveComponentToJson(existing);
        JSON after = before;
        after["data"]["customData"] = state["customData"];
        if (recipe.kind == ShortText::Kind::Rules) after["data"]["priority"] = 0;
        behavior = std::make_unique<ComponentEditCommand>(target, existing, before, after);
    } else {
        behavior = std::make_unique<AddComponentCommand>(target, "ShortTextBehavior", state);
    }
    if (group->IsEmpty()) return behavior;
    group->AddCommand(std::move(behavior));
    return group;
}

bool ShortTextBehaviorEditor::SetInput(const std::string &sentence) {
    if (sentence.empty() || sentence.size() >= sizeof(input_)) return false;
    std::memcpy(input_, sentence.c_str(), sentence.size() + 1);
    parsed_ = {}; property_ = {}; event_ = {}; plan_ = {}; status_.clear();
    return true;
}
void ShortTextBehaviorEditor::Show(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *selected) {
    if (!open || !context || !commands) return;
    const UUID128 selectedID = selected ? selected->GetObjectID() : UUID128{};
    const auto &projectRoot = ProjectPaths::ProjectRoot();
    if (!defaults_) {
        defaults_.emplace(Utf8StringToPath(ProjectPaths::InEngineRoot("EditorTools/ShortTextDefaults.json")));
        defaults_->Reload(defaultsStatus_);
    }
    if (projectRoot != learningProjectRoot_) {
        learningProjectRoot_ = projectRoot;
        learning_.reset(); learningSelection_ = -1; parsed_ = {};
        learningStatus_.clear();
        if (!projectRoot.empty()) {
            learning_.emplace(Utf8StringToPath(projectRoot) / L"ShortTextLearning.json");
            learning_->Reload(learningStatus_);
        }
    }
    ImGui::SetNextWindowSize(ImVec2(610, 580), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("短文からの挙動作成（試験）###ShortTextBehaviorEditor", &open)) { ImGui::End(); return; }
    ImGui::TextUnformatted("短文の作成と適用");
    EditorHelp("短い言葉で選択したひとつのオブジェクトに動きを付けます。文章を解釈し、適用内容を確認してから適用してください。実行時のAI・通信は不要です。移動はローカル座標です。");
    ImGui::Text("対象：%s", selected ? selected->GetName().c_str() : "ひとつ選択してください");
    if (defaults_ && !defaults_->Entries().empty()) ImGui::Text("標準の例文：%zu 件", defaults_->Entries().size());
    if (!defaultsStatus_.empty()) ImGui::TextWrapped("%s", defaultsStatus_.c_str());
    if (context->IsPlaying()) ImGui::TextWrapped("編集は再生を停止してから行ってください。");
    if (ImGui::InputText("文章", input_, sizeof(input_))) { parsed_ = {}; property_ = {}; event_ = {}; plan_ = {}; status_.clear(); }
    const char *examples[] = {"上下にふわふわ動かして", "左右に振幅2で周期3秒で往復して", "ゆっくり右に回して", "Y軸で毎秒90度回転して", "右に毎秒2で動かして",
        "Transformの移動X軸を-1.0fから1.0fの間で往復移動", "Transformの回転Y軸を-90.0fから90.0fの間で往復",
        "BoxColliderのサイズX軸を1.0fから3.0fの間で往復", "BoxColliderのサイズX軸を2.0fにする", "Transformの移動Y軸を0から2の間で周期4秒でなめらかに往復",
        "1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動", "2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転",
        "1秒かけてTransformのスケールX軸を1.0fから2.0fまでLinearで変更", "1秒かけてBoxColliderのサイズX軸を1.0fから3.0fまでEaseInOutCubicで変更"};
    if (ImGui::BeginCombo("例文", "選んで入力")) {
        for (const auto *example : examples) {
            if (ImGui::Selectable(example)) {
                const size_t length = std::strlen(example);
                std::memcpy(input_, example, length + 1);
                parsed_ = {}; property_ = {}; event_ = {}; plan_ = {}; status_.clear();
            }
        }
        ImGui::Separator();
        for (const auto &example : ShortText::EventExampleSentences()) if (ImGui::Selectable(example.c_str())) {
            std::memcpy(input_, example.c_str(), example.size() + 1);
            parsed_ = {}; property_ = {}; event_ = {}; plan_ = {}; status_.clear();
        }
        if (defaults_) {
            ImGui::Separator();
            for (const auto &entry : defaults_->Entries()) if (ImGui::Selectable(entry.phrase.c_str())) {
                std::memcpy(input_, entry.phrase.c_str(), entry.phrase.size() + 1);
                parsed_ = {}; property_ = {}; event_ = {}; plan_ = {}; status_.clear();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(context->IsPlaying() || !selected);
    if (ImGui::Button("解釈する")) {
        parsed_ = learning_ ? learning_->Interpret(input_) : ShortText::Parse(input_);
        property_ = {}; plan_ = {}; event_ = {};
        // Learned phrases keep priority. The event grammar claims only sentences it fully recognizes (none of the
        // 486 standard phrases); a recognized sentence is either planned completely or rejected with its reason.
        if (!parsed_.success) {
            event_ = ShortText::ParseEventRequest(input_);
            if (event_.recognized) {
                parsed_ = {};
                if (event_.success) MakeShortTextEventCommand(context, selected, event_.request, input_, &plan_);
                if (!event_.success || !plan_.success) {
                    parsed_.message = event_.success ? plan_.message : event_.message;
                    event_.success = false;
                }
            } else if (defaults_) parsed_ = defaults_->Interpret(input_);
        }
        // Standard phrases keep priority over the property grammar, which only claims 「〇〇の〇〇」 sentences.
        if (event_.recognized) {
        } else if (!parsed_.success) {
            property_ = ParseShortTextPropertyRequest(input_, selected);
            if (property_.recognized) {
                parsed_ = {};
                if (property_.success) MakeShortTextPropertyCommand(selected, property_.request, input_, &plan_);
                if (!property_.success || !plan_.success) {
                    parsed_.message = property_.success ? plan_.message : property_.message;
                    property_.success = false;
                }
            }
        } else plan_ = PlanShortTextBehavior(selected, parsed_.recipe);
        targetID_ = selected->GetObjectID();
        sceneID_ = context->GetSceneID();
        status_.clear();
    }
    ImGui::EndDisabled();
    if (!parsed_.message.empty()) ImGui::TextWrapped("%s", parsed_.message.c_str());
    const bool propertyReady = property_.recognized && property_.success && plan_.success;
    const bool eventReady = event_.recognized && event_.success && plan_.success;
    if (propertyReady || eventReady) ImGui::TextWrapped("%s", plan_.message.c_str());
    if ((parsed_.success || propertyReady || eventReady) && !plan_.steps.empty()) {
        ImGui::TextUnformatted("適用内容（1回のUndoで戻せます）:");
        for (const auto &step : plan_.steps) ImGui::BulletText("%s", step.c_str());
        if (!plan_.requirements.empty()) ImGui::TextUnformatted("確認・自動では設定しない内容:");
        for (const auto &requirement : plan_.requirements) ImGui::BulletText("%s", requirement.c_str());
    }
    if (parsed_.success && parsed_.recipe.kind == ShortText::Kind::Rules) {
        bool contact = false, audio = false;
        for (const auto &rule : parsed_.recipe.rules) {
            contact |= ShortText::IsContact(rule.event);
            for (const auto &action : rule.actions) audio |= action.operation == ShortText::Operation::PlaySound || action.operation == ShortText::Operation::StopSound || action.operation == ShortText::Operation::SetVolume;
        }
        if (contact) EditorHelp("接触には対象と相手のColliderが必要です。タグは相手オブジェクトのタグを参照します。");
        if (audio) EditorHelp("音声操作は対象の最初のAudioSourceを使います。音を設定し、playOnAwakeを無効にしてください。");
    }
    const bool sameTarget = selected && selected->GetObjectID() == targetID_ && context->GetSceneID() == sceneID_;
    if ((parsed_.success || propertyReady || eventReady) && !sameTarget) ImGui::TextWrapped("対象が変わりました。もう一度解釈してください。");
    if (parsed_.success && selected && selected->GetComponent<ShortTextBehavior>())
        ImGui::TextWrapped("適用すると、この対象の既存の短文挙動を置き換えます。");
    ImGui::BeginDisabled(context->IsPlaying() || !sameTarget || !(parsed_.success || propertyReady || eventReady));
    auto apply = [&]() {
        if (eventReady) {
            // Rebuilt from the current scene so the command never uses a stale plan.
            ShortTextComponentPlan plan;
            auto command = MakeShortTextEventCommand(context, selected, event_.request, input_, &plan);
            const bool ok = command && commands->Execute(std::move(command));
            status_ = ok ? "イベント・部品を適用しました。Inspector（シーンの GameEventRouter・各部品）で確認・編集でき、Undoで戻せます。" : "適用できませんでした: " + plan.message;
            if (ok) { event_ = {}; plan_ = {}; }
            return ok;
        }
        if (propertyReady) {
            ShortTextComponentPlan plan;
            auto command = MakeShortTextPropertyCommand(selected, property_.request, input_, &plan);
            const bool ok = command && commands->Execute(std::move(command));
            status_ = ok ? "コンポーネントを適用しました。Inspectorで確認・調整でき、Undoで戻せます。" : "適用できませんでした: " + plan.message;
            if (ok) { property_ = {}; plan_ = {}; }
            return ok;
        }
        auto command = MakeShortTextBehaviorCommand(selected, parsed_.recipe, input_);
        const bool ok = command && commands->Execute(std::move(command));
        status_ = ok ? "適用しました。Undoで戻せます。再生すると動作します。" : "適用できませんでした。対象のTransformと状態を確認してください。";
        return ok;
    };
    if (ImGui::Button("適用")) apply();
    ImGui::SameLine();
    if (ImGui::Button("適用して再生") && apply()) context->PlayStart();
    ImGui::EndDisabled();
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::Separator();
    if (ImGui::TreeNode("使い方")) {
        ImGui::TextUnformatted("挙動の仕様");
        EditorHelp("1オブジェクトに1レシピ。登録したイベント・条件・複数動作も利用できます。未登録の条件文は推測しません。");
        ImGui::TextUnformatted("プロパティの指定");
        EditorHelp("「Transformの移動X軸を-1から1の間で往復移動」「BoxColliderのサイズX軸を2にする」のような「コンポーネントの項目」指定は、"
            "実際のコンポーネント（設定またはPropertyMotion）として追加します。往復は時間未指定なら1往復2秒・一定速度、回転は度で指定します。");
        ImGui::TextUnformatted("キーフレーム");
        EditorHelp("「1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動」のような「N秒かけて AからBまで イージング名」の指定は、"
            "KeyFrameAnimator に本物のキーフレームアニメーションとして登録します（1回だけ再生し、終了後は到達値を保持）。");
        ImGui::TextUnformatted("カスタムイベント");
        EditorHelp("「GoalReachedイベントを作成」「Damageイベントを作成、amountは数値」でシーンのイベント（GameEventRouter）を定義し、"
            "「〜を受け取ったら」「〜に触れたら」「Eキーを押したら」＋「発火／送る／関数を呼ぶ／体力をamountだけ減らす／N秒かけて…」で受信処理（GameEventListener）を作ります。"
            "体力・アイテム・寿命・生成・インタラクト・復活・インベントリ・会話・アニメーション状態の部品も追加できます。名前の重複や未定義のイベントは適用しません。");
        ImGui::TreePop();
    }
    ShowLearning(context->GetSceneObject(selectedID));
    ImGui::End();
}

void ShortTextBehaviorEditor::ShowLearning(EmptyObject *selected) {
    if (!ImGui::CollapsingHeader("言葉を教える（ローカル）", 0)) return;
    if (!learning_) { ImGui::TextWrapped("プロジェクトを開くと、言葉と挙動を保存できます。"); return; }
    ImGui::TextUnformatted("学習の保存");
    EditorHelp("このプロジェクトの ShortTextLearning.json に保存します。登録だけではシーンは変わりません。");
    if (ImGui::Button("学習内容を再読み込み")) {
        if (learning_->Reload(learningStatus_)) learningStatus_ = "学習内容を再読み込みしました。";
        learningSelection_ = -1; parsed_ = {};
        if (defaults_) defaults_->Reload(defaultsStatus_);
    }
    if (!learningStatus_.empty()) ImGui::TextWrapped("%s", learningStatus_.c_str());
    if (ImGui::Button("入力中の文章を教える")) {
        std::memcpy(teachPhrase_, input_, std::strlen(input_) + 1);
        if (parsed_.success) teachRecipe_ = parsed_.recipe;
        teachMode_ = 0; learningSelection_ = -1;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected || !selected->GetComponent<ShortTextBehavior>());
    if (ImGui::Button("選択対象の挙動を取り込む")) teachRecipe_ = selected->GetComponent<ShortTextBehavior>()->GetRecipe();
    ImGui::EndDisabled();
    ImGui::InputText("教える言葉", teachPhrase_, sizeof(teachPhrase_));
    ImGui::Combo("覚え方", &teachMode_, "文章全体に挙動を登録\0言い回しを言い換える\0");
    if (teachMode_ == 1) {
        ImGui::InputText("言い換え先", teachReplacement_, sizeof(teachReplacement_));
        EditorHelp("例：ぷかぷか → ふわふわ。周期3秒でぷかぷかさせて、のように使えます。言い換え先は標準の言葉で指定します。");
    } else {
        int kind = static_cast<int>(teachRecipe_.kind);
        if (ImGui::Combo("動き", &kind, "往復\0回転\0移動\0イベントと動作\0")) {
            teachRecipe_.kind = static_cast<ShortText::Kind>(kind);
            teachRecipe_.speed = kind == 1 ? 90.0f : 1.0f;
            teachRecipe_.direction = 1;
            teachRecipe_.rules.clear();
            if (kind == 3) { ShortText::Rule rule; rule.event = ShortText::Event::KeyPressed; rule.actions.push_back({ShortText::Operation::SetVelocity, 1, 16}); teachRecipe_.rules.push_back(rule); }
        }
        if (teachRecipe_.kind == ShortText::Kind::Rules) {
            ShowRules(teachRecipe_.rules);
            EditorHelp("移動は単位/秒、回転は度/秒です。その他はイベント1回ごとの代入・加算です。速度を使う場合は適用時にVelocityを追加します。");
        } else {
        ImGui::Combo("学習する軸", &teachRecipe_.axis, "X\0Y\0Z\0");
        if (teachRecipe_.kind == ShortText::Kind::Oscillate) {
            ImGui::DragFloat("学習する振幅", &teachRecipe_.amplitude, 0.01f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("学習する周期 (秒)", &teachRecipe_.period, 0.01f, 0.05f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        } else {
            ImGui::DragFloat(teachRecipe_.kind == ShortText::Kind::Rotate ? "学習する速度 (度/秒)" : "学習する速度 (単位/秒)",
                &teachRecipe_.speed, 0.1f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            bool reverse = teachRecipe_.direction < 0;
            if (ImGui::Checkbox("学習する方向を反転", &reverse)) teachRecipe_.direction = reverse ? -1 : 1;
        }
        ImGui::TextWrapped("%s", ShortText::Describe(teachRecipe_).c_str());
        EditorHelp("この文章全体を、設定したひとつの挙動として覚えます。数値の変更は設定を更新してください。");
        }
    }
    ImGui::BeginDisabled(!learning_->IsReady());
    if (ImGui::Button("登録 / 更新")) {
        ShortText::LearnedEntry entry;
        entry.phrase = teachPhrase_;
        entry.mode = teachMode_ == 1 ? ShortText::LearningMode::Alias : ShortText::LearningMode::Recipe;
        entry.replacement = teachReplacement_;
        entry.recipe = teachRecipe_;
        if (learning_->Upsert(entry, learningStatus_)) {
            learningStatus_ = "学習内容を保存しました。同じ言葉は上書き更新されます。文章を再解釈して適用してください。";
            parsed_ = {}; learningSelection_ = -1;
        }
    }
    ImGui::EndDisabled();
    ImGui::Text("登録済み：%zu 件", learning_->Entries().size());
    if (ImGui::BeginListBox("登録した言葉", ImVec2(-1, 100))) {
        int index = 0;
        for (const auto &entry : learning_->Entries()) {
            ImGui::PushID(index);
            if (ImGui::Selectable(entry.phrase.c_str(), learningSelection_ == index)) {
                learningSelection_ = index;
                std::memcpy(teachPhrase_, entry.phrase.c_str(), entry.phrase.size() + 1);
                std::memcpy(teachReplacement_, entry.replacement.c_str(), entry.replacement.size() + 1);
                teachRecipe_ = entry.recipe;
                teachMode_ = entry.mode == ShortText::LearningMode::Alias ? 1 : 0;
            }
            ImGui::PopID(); ++index;
        }
        ImGui::EndListBox();
    }
    ImGui::BeginDisabled(learningSelection_ < 0);
    if (ImGui::Button("選択した学習内容を削除")) {
        if (learning_->Remove(static_cast<size_t>(learningSelection_), learningStatus_)) {
            learningStatus_ = "学習内容を削除しました。シーンに適用済みの挙動は変更しません。";
            learningSelection_ = -1; parsed_ = {};
        }
    }
    ImGui::EndDisabled();
}
} // namespace KashipanEngine
#endif
