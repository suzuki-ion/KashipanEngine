#include "TargetObjectSelector.h"
#ifdef USE_IMGUI
#include <imgui.h>

#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"

namespace KashipanEngine {
namespace TargetObjectSelector {

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

} // namespace TargetObjectSelector
} // namespace KashipanEngine

#endif // USE_IMGUI
