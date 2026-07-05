#pragma once
#if defined(USE_IMGUI)
#include <imgui.h>
#include <string>
#include <vector>

#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {
namespace TargetObjectSelector {

/// @brief 指定オブジェクトが描画先コンポーネントを持っているか
inline bool HasRenderTargetComponent(EmptyObject *object) {
    if (!object) return false;
    return object->HasComponents<NormalWindowObject>() > 0 ||
           object->HasComponents<OverlayWindowObject>() > 0 ||
           object->HasComponents<ScreenBufferObject>() > 0 ||
           object->HasComponents<ShadowMapObject>() > 0;
}

/// @brief 描画先オブジェクトの選択UI（シーン上のオブジェクトから選択 or ヒエラルキーからのD&D）
/// @param label ラベル
/// @param sceneContext 所属シーンのコンテキスト
/// @param targetObjectID 選択中の描画先オブジェクトID（変更時に上書きされる）
/// @param allowNone 未指定（None）を選択可能にする
/// @return 値が変更された場合は true
inline bool ShowSelector(const char *label, SceneContext *sceneContext, UUID128 &targetObjectID, bool allowNone = true) {
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
