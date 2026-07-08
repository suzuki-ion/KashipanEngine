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

bool ShowSelector(const char *label, SceneContext *sceneContext, UUID128 &targetObjectID, bool allowNone) {
    if (!sceneContext) return false;
    bool changed = false;

    EmptyObject *current = targetObjectID.IsValid() ? sceneContext->GetSceneObject(targetObjectID) : nullptr;
    const std::string preview = current ? current->GetName() : "(None)";

    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allowNone) {
            const bool selected = !current;
            if (ImGui::Selectable("(None)", selected) && !selected) {
                targetObjectID = UUID128();
                changed = true;
            }
        }
        // 描画先コンポーネントを持つオブジェクトのみを候補にする
        for (const auto &object : sceneContext->GetSceneObjects()) {
            if (!object || !HasRenderTargetComponent(object.get())) continue;
            const bool selected = (object.get() == current);
            ImGui::PushID(object.get());
            if (ImGui::Selectable(object->GetName().c_str(), selected) && !selected) {
                targetObjectID = object->GetObjectID();
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
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
    if (ImGui::TreeNode("Render Target Filter")) {
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
