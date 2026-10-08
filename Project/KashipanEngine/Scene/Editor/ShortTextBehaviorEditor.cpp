#include "Scene/Editor/ShortTextBehaviorEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <cstring>
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {
std::unique_ptr<IEditorCommand> MakeShortTextBehaviorCommand(EmptyObject *target,
    const ShortText::Recipe &recipe, const std::string &sourceText) {
    if (!target || !target->GetComponent<Transform>() || !ShortText::IsValid(recipe) || sourceText.size() > 512) return nullptr;
    const JSON state = MakeShortTextBehaviorState(recipe, sourceText);
    if (auto *existing = target->GetComponent<ShortTextBehavior>()) {
        const JSON before = target->SaveComponentToJson(existing);
        JSON after = before;
        after["data"]["customData"] = state["customData"];
        return std::make_unique<ComponentEditCommand>(target, existing, before, after);
    }
    return std::make_unique<AddComponentCommand>(target, "ShortTextBehavior", state);
}

void ShortTextBehaviorEditor::Show(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *selected) {
    if (!open || !context || !commands) return;
    ImGui::SetNextWindowSize(ImVec2(540, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("短文の挙動（試験）###ShortTextBehaviorEditor", &open)) { ImGui::End(); return; }
    ImGui::TextWrapped("短い言葉で、選択したひとつのオブジェクトに動きを付けます。");
    ImGui::Text("対象：%s", selected ? selected->GetName().c_str() : "ひとつ選択してください");
    ImGui::TextDisabled("往復・回転・一定方向の移動 / ローカル座標 / AI・通信不要");
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
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(context->IsPlaying() || !selected);
    if (ImGui::Button("解釈する")) {
        parsed_ = ShortText::Parse(input_);
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
    ImGui::TextWrapped("1オブジェクトに1挙動。条件・否定・複数動作は未対応です。速度や振幅はInspectorでも調整できます。回転は既定でZ軸、右回りは負方向です。");
    ImGui::End();
}
} // namespace KashipanEngine
#endif
