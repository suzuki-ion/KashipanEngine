#include "TargetObjectSelector.h"
#ifdef USE_IMGUI
#include <imgui.h>

#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/IRenderTarget.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace TargetObjectSelector {

namespace {
const char *ToString(RenderTargetKind kind) {
    switch (kind) {
    case RenderTargetKind::Window:          return "Window";
    case RenderTargetKind::ScreenBuffer:    return "ScreenBuffer";
    case RenderTargetKind::ShadowMapBuffer: return "ShadowMapBuffer";
    default:                                return "Unknown";
    }
}
} // namespace

bool HasRenderTargetComponent(EmptyObject *object) {
    if (!object) return false;
    return object->HasComponents<NormalWindowObject>() > 0 ||
           object->HasComponents<OverlayWindowObject>() > 0 ||
           object->HasComponents<ScreenBufferObject>() > 0 ||
           object->HasComponents<ShadowMapObject>() > 0;
}

bool ShowSelector(const char *label, SceneContext *sceneContext, UUID128 &targetObjectID, bool allowNone, bool restrictToRenderTargets) {
    if (!sceneContext) return false;
    bool changed = false;

    EmptyObject *current = targetObjectID.IsValid() ? sceneContext->GetSceneObject(targetObjectID) : nullptr;
    const std::string preview = current ? current->GetName() : "(None)";

    // 検索ボックス＋リスト（最大12行）が収まるよう、既定（8項目分）より高いポップアップを許可する
    if (ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        // コンボのポップアップは同時に1つしか開かないため、検索状態は全セレクターで共有する
        static ImGuiTextFilter sFilter;
        const bool appearing = ImGui::IsWindowAppearing();
        const bool enterPressed = ImGuiCustom::PopupSearchBox("##TargetObjectSearch", sFilter);

        std::vector<EmptyObject *> matches;
        for (auto *object : sceneContext->GetSceneObjects()) {
            if (!object) continue;
            // restrictToRenderTargets が true の場合は描画先コンポーネントを持つオブジェクトのみを候補にする
            if (restrictToRenderTargets && !HasRenderTargetComponent(object)) continue;
            if (!sFilter.PassFilter(object->GetName().c_str())) continue;
            matches.push_back(object);
        }
        const bool showNone = allowNone && !sFilter.IsActive();

        // Enterで先頭の候補を決定する
        if (enterPressed && !matches.empty()) {
            if (matches.front() != current) {
                targetObjectID = matches.front()->GetObjectID();
                changed = true;
            }
            ImGui::CloseCurrentPopup();
        }

        // 子ウィンドウ内のSelectableはポップアップを自動で閉じないため、選択時は明示的に閉じる
        ImGui::BeginChild("##TargetObjectList", ImVec2(0.0f, ImGuiCustom::PopupSearchListHeight(matches.size() + (showNone ? 1 : 0))));
        if (showNone) {
            const bool selected = !current;
            if (ImGui::Selectable(TranslationLabel("component.targetobjectselector.none"), selected)) {
                if (!selected) {
                    targetObjectID = UUID128();
                    changed = true;
                }
                ImGui::CloseCurrentPopup();
            }
        }
        for (auto *object : matches) {
            const bool selected = (object == current);
            ImGui::PushID(object);
            if (ImGui::Selectable(object->GetName().c_str(), selected)) {
                if (!selected) {
                    targetObjectID = object->GetObjectID();
                    changed = true;
                }
                ImGui::CloseCurrentPopup();
            }
            // 開いた直後は選択中の項目が見える位置までスクロールする
            if (selected && appearing) ImGui::SetScrollHereY(0.5f);
            ImGui::PopID();
        }
        if (matches.empty() && !showNone) {
            ImGui::TextDisabled("%s", TranslationC("editor.imguicustom.search.noresults"));
        }
        ImGui::EndChild();
        ImGui::EndCombo();
    }

    // ヒエラルキーからのD&Dを受け付ける
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSceneObjectDragDropType)) {
            IM_ASSERT(payload->DataSize == sizeof(SceneObjectDragDropPayload));
            auto *dndPayload = static_cast<const SceneObjectDragDropPayload *>(payload->Data);
            if (dndPayload->object) {
                targetObjectID = dndPayload->object->GetObjectID();
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

void ShowRenderTargetFilters(SceneContext *sceneContext, const UUID128 &targetObjectID, std::unordered_set<std::string> &excludedRenderTargetNames) {
    if (!sceneContext || !targetObjectID.IsValid()) return;
    EmptyObject *targetObject = sceneContext->GetSceneObject(targetObjectID);
    if (!targetObject) return;

    std::vector<IRenderTarget *> targets;
    SceneRenderer::CollectRenderTargets(targetObject, targets);
    if (targets.empty()) return;

    // 使われなくなった除外設定はそのまま残しておく（一時的にコンポーネントを外しただけの場合を考慮）
    if (ImGui::TreeNode(TranslationLabel("component.targetobjectselector.render_target_filter"))) {
        for (auto *target : targets) {
            if (!target) continue;
            const std::string &name = target->GetRenderTargetName();
            bool included = !excludedRenderTargetNames.contains(name);
            const std::string label = std::string("[") + ToString(target->GetRenderTargetKind()) + "] " + name;
            ImGui::PushID(target);
            if (ImGui::Checkbox(label.c_str(), &included)) {
                if (included) {
                    excludedRenderTargetNames.erase(name);
                } else {
                    excludedRenderTargetNames.insert(name);
                }
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

} // namespace TargetObjectSelector
} // namespace KashipanEngine

#endif // USE_IMGUI
