#include "Scene/Editor/ShortTextBehaviorEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <cstring>
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/SceneEditorContext.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {
namespace {
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
            if (rule.event == ShortText::Event::Timer) ImGui::DragFloat("間隔 (秒)", &rule.interval, 0.05f, 0.05f, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextUnformatted("条件はすべて満たした場合に実行します。条件なしは常に実行します。");
            for (size_t c = 0; c < rule.conditions.size(); ++c) {
                ImGui::PushID(static_cast<int>(c));
                auto &condition = rule.conditions[c];
                int test = static_cast<int>(condition.test);
                if (NamesCombo("条件", test, ShortText::TestLabels)) condition.test = static_cast<ShortText::Test>(test);
                ImGui::Combo("条件の軸", &condition.axis, "X\0Y\0Z\0");
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
                ImGui::Combo("操作する軸", &action.axis, "X\0Y\0Z\0");
                if (action.operation != ShortText::Operation::StopVelocity)
                    ImGui::DragFloat("操作値", &action.value, 0.1f, action.operation == ShortText::Operation::SetScale ? 0.001f : -10000.0f,
                        action.operation == ShortText::Operation::SetScale ? 100.0f : 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
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
    if (recipe.kind == ShortText::Kind::Rules && ShortText::NeedsVelocity(recipe.rules) && !target->GetComponent<Velocity>())
        group->AddCommand(std::make_unique<AddComponentCommand>(target, "Velocity"));
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
    if (!ImGui::Begin("短文の挙動（試験）###ShortTextBehaviorEditor", &open)) { ImGui::End(); return; }
    ImGui::TextWrapped("短い言葉で、選択したひとつのオブジェクトに動きを付けます。");
    ImGui::Text("対象：%s", selected ? selected->GetName().c_str() : "ひとつ選択してください");
    ImGui::TextDisabled("動き・キー入力・条件・動作 / ローカル座標 / 実行時のAI・通信不要");
    if (defaults_ && !defaults_->Entries().empty()) ImGui::Text("標準の例文：%zu 件", defaults_->Entries().size());
    if (!defaultsStatus_.empty()) ImGui::TextWrapped("%s", defaultsStatus_.c_str());
    if (context->IsPlaying()) ImGui::TextWrapped("編集は再生を停止してから行ってください。");
    if (ImGui::InputText("文章", input_, sizeof(input_))) { parsed_ = {}; status_.clear(); }
    const char *examples[] = {"上下にふわふわ動かして", "左右に振幅2で周期3秒で往復して", "ゆっくり右に回して", "Y軸で毎秒90度回転して", "右に毎秒2で動かして"};
    if (ImGui::BeginCombo("例文", "選んで入力")) {
        for (const auto *example : examples) {
            if (ImGui::Selectable(example)) {
                const size_t length = std::strlen(example);
                std::memcpy(input_, example, length + 1);
                parsed_ = {}; status_.clear();
            }
        }
        if (defaults_) {
            ImGui::Separator();
            for (const auto &entry : defaults_->Entries()) if (ImGui::Selectable(entry.phrase.c_str())) {
                std::memcpy(input_, entry.phrase.c_str(), entry.phrase.size() + 1);
                parsed_ = {}; status_.clear();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(context->IsPlaying() || !selected);
    if (ImGui::Button("解釈する")) {
        parsed_ = learning_ ? learning_->Interpret(input_) : ShortText::Parse(input_);
        if (!parsed_.success && defaults_) parsed_ = defaults_->Interpret(input_);
        targetID_ = selected->GetObjectID();
        sceneID_ = context->GetSceneID();
        status_.clear();
    }
    ImGui::EndDisabled();
    if (!parsed_.message.empty()) ImGui::TextWrapped("%s", parsed_.message.c_str());
    const bool sameTarget = selected && selected->GetObjectID() == targetID_ && context->GetSceneID() == sceneID_;
    if (parsed_.success && !sameTarget) ImGui::TextWrapped("対象が変わりました。もう一度解釈してください。");
    if (selected && selected->GetComponent<ShortTextBehavior>())
        ImGui::TextWrapped("適用すると、この対象の既存の短文挙動を置き換えます。");
    ImGui::BeginDisabled(context->IsPlaying() || !sameTarget || !parsed_.success);
    auto apply = [&]() {
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
    ImGui::TextWrapped("1オブジェクトに1レシピ。登録したイベント・条件・複数動作も利用できます。未登録の条件文は推測しません。");
    ShowLearning(context->GetSceneObject(selectedID));
    ImGui::End();
}

void ShortTextBehaviorEditor::ShowLearning(EmptyObject *selected) {
    if (!ImGui::CollapsingHeader("言葉を教える（ローカル）", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (!learning_) { ImGui::TextWrapped("プロジェクトを開くと、言葉と挙動を保存できます。"); return; }
    ImGui::TextWrapped("このプロジェクトの ShortTextLearning.json に保存します。登録だけではシーンは変わりません。");
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
        ImGui::TextWrapped("例：ぷかぷか → ふわふわ。周期3秒でぷかぷかさせて、のように使えます。言い換え先は標準の言葉で指定します。");
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
            ImGui::TextWrapped("移動は単位/秒、回転は度/秒です。その他はイベント1回ごとの代入・加算です。速度を使う場合は適用時にVelocityを追加します。");
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
        ImGui::TextWrapped("この文章全体を、設定したひとつの挙動として覚えます。数値の変更は設定を更新してください。");
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
