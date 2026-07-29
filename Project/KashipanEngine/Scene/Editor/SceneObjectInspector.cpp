#include "SceneObjectInspector.h"
#include <algorithm>
#include <unordered_map>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Scene/Editor/ComponentAddMenu.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneEditorCommands.h"

namespace KashipanEngine {

namespace {
/// @brief インスペクター内でのコンポーネント並び替えD&Dペイロード型名（この.cpp内のみで完結する）
constexpr const char *kComponentDragDropType = "DND_COMPONENT";
struct ComponentDragDropPayload {
    IObjectComponent *component = nullptr;
};
} // namespace

void SceneObjectInspector::ShowImGui() {
    ImGui::Begin("Scene Object Inspector");
    if (objectHierarchy_) {
        EmptyObject *selectedObject = objectHierarchy_->GetSelectedObject();
        const auto &selectedObjects = objectHierarchy_->GetSelectedObjects();
        if (selectedObject && selectedObjects.size() >= 2) {
            ShowMultiObjectInspector(selectedObject, selectedObjects);
        } else if (selectedObject) {
            ShowObjectInspector(selectedObject);
        } else {
            ImGui::Text("No object selected.");
        }
    } else {
        ImGui::Text("No object hierarchy available.");
    }
    ImGui::End();

    FlushPendingComponentEdit();
}

void SceneObjectInspector::ShowObjectInspector(EmptyObject *obj) {
    ImGui::Text("Object Inspector");
    ImGui::Separator();

    //--------- 名前（編集セッション終了時にUndo履歴へ積む） ---------//
    std::string name = obj->GetName();
    if (ImGui::InputText("Name", &name)) {
        obj->SetName(name);
    }
    if (ImGui::IsItemActivated()) {
        nameBeforeEdit_ = name;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && commands_ && obj->GetName() != nameBeforeEdit_) {
        commands_->PushExecuted(std::make_unique<ObjectPropertyCommand>(
            obj, nameBeforeEdit_, obj->GetName(), obj->IsActive(), obj->IsActive()));
    }

    //--------- アクティブ状態 ---------//
    bool active = obj->IsActive();
    if (ImGui::Checkbox("Active", &active)) {
        if (commands_) {
            commands_->Execute(std::make_unique<ObjectPropertyCommand>(
                obj, obj->GetName(), obj->GetName(), obj->IsActive(), active));
        } else {
            obj->SetActive(active);
        }
    }

    //--------- EditorOnly（エディター専用。シーンビューにのみ描画され、再生時・ゲーム実行時には存在しない） ---------//
    ImGui::SameLine();
    bool editorOnly = obj->IsEditorOnly();
    if (ImGui::Checkbox("Editor Only", &editorOnly)) {
        obj->SetEditorOnly(editorOnly);
    }

    //--------- タグ（分類・判別用の任意文字列） ---------//
    std::string tagName = obj->GetTagName();
    if (ImGui::InputText("Tag", &tagName)) {
        obj->SetTag(tagName);
    }

    //--------- コンポーネント一覧（処理優先順位＝更新優先度の順に並べる） ---------//
    IObjectComponent *componentToRemove = nullptr;
    int id = 0;
    for (IObjectComponent *comp : GetOrderedComponents(obj)) {
        ImGui::PushID(id);
        ImGui::Separator();
        bool componentActive = comp->IsActive();
        if (ImGui::Checkbox("##Active", &componentActive)) {
            comp->SetActive(componentActive);
        }
        ImGui::SameLine();
        // 開閉状態はコンポーネントの種類ごとに保存される（デフォルトは開いた状態）
        bool headerOpen = EditorSettings::PersistentTreeNode(comp->GetComponentType().c_str(),
                "inspector.component." + comp->GetComponentType());
        // ヒエラルキーと同様に、D&Dでコンポーネント自体の処理優先順位を並び替えられるようにする
        DragAndDropComponent(comp);
        if (headerOpen) {
            if (ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    componentToRemove = comp;
                }
                ImGui::EndPopup();
            }
            // パラメータ変更をUndo履歴へ積むため、表示前後の状態を比較する
            JSON before = obj->SaveComponentToJson(comp);
            // タグ（分類・判別用の任意文字列。before/afterの間で編集することでUndo対象になる）
            std::string componentTag = comp->GetTagName();
            if (ImGui::InputText("Tag", &componentTag)) {
                comp->SetTag(componentTag);
            }
            obj->ShowComponentImGui(comp);
            JSON after = obj->SaveComponentToJson(comp);
            if (before != after) {
                TrackComponentEdit(obj, comp, before, after);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++id;
    }
    ApplyComponentDragAndDrop(obj);

    if (componentToRemove) {
        // 編集中のコンポーネントを先に削除すると保留キャッシュがダングリングするため、
        // 削除コマンドより前のUndo履歴として確定させる。
        CommitPendingEdit();
        if (commands_) {
            commands_->Execute(std::make_unique<RemoveComponentCommand>(obj, componentToRemove));
        } else {
            obj->RemoveComponent(componentToRemove);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        // カテゴリごとのツリーメニューから追加する型を選択する
        std::string selectedType;
        if (ComponentAddMenu::Show(GetRegisteredObjectComponentTypes(),
                [](const std::string &typeName) -> const std::vector<std::string> & { return GetObjectComponentCategory(typeName); },
                selectedType)) {
            if (commands_) {
                commands_->Execute(std::make_unique<AddComponentCommand>(obj, selectedType));
            } else {
                auto newComp = CreateObjectComponentByType(selectedType);
                if (newComp) {
                    obj->AddComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

void SceneObjectInspector::ShowMultiObjectInspector(EmptyObject *primary, const std::unordered_set<EmptyObject *> &selectedObjects) {
    ImGui::Text("Object Inspector (%d objects)", static_cast<int>(selectedObjects.size()));
    ImGui::Separator();

    //--------- 共通プロパティ（プライマリの値を表示し、変更を全選択オブジェクトへ適用する） ---------//
    bool active = primary->IsActive();
    if (ImGui::Checkbox("Active", &active)) {
        if (commands_) {
            auto composite = std::make_unique<CompositeCommand>(
                "Set Active: " + std::to_string(selectedObjects.size()) + " Objects");
            for (auto *obj : selectedObjects) {
                if (!obj) continue;
                composite->AddCommand(std::make_unique<ObjectPropertyCommand>(
                    obj, obj->GetName(), obj->GetName(), obj->IsActive(), active));
            }
            commands_->Execute(std::move(composite));
        } else {
            for (auto *obj : selectedObjects) {
                if (obj) obj->SetActive(active);
            }
        }
    }

    ImGui::SameLine();
    bool editorOnly = primary->IsEditorOnly();
    if (ImGui::Checkbox("Editor Only", &editorOnly)) {
        for (auto *obj : selectedObjects) {
            if (obj) obj->SetEditorOnly(editorOnly);
        }
    }

    std::string tagName = primary->GetTagName();
    if (ImGui::InputText("Tag", &tagName)) {
        for (auto *obj : selectedObjects) {
            if (obj) obj->SetTag(tagName);
        }
    }

    //--------- 共通コンポーネント ---------//
    // プライマリのコンポーネント順に「同じ型のn個目」が全選択オブジェクトに存在するものだけを表示する。
    // 値の編集はbefore/afterの差分を取り、変更された値だけを他オブジェクトの対応コンポーネントへ適用する
    struct PendingRemove {
        EmptyObject *object = nullptr;
        IObjectComponent *component = nullptr;
    };
    std::vector<PendingRemove> pendingRemoves;
    std::string removeTypeName;

    std::unordered_map<std::string, size_t> typeOrdinals;
    int id = 0;
    for (IObjectComponent *comp : GetOrderedComponents(primary)) {
        const std::string &typeName = comp->GetComponentType();
        const size_t ordinal = typeOrdinals[typeName]++;

        // 全選択オブジェクトから対応コンポーネントを収集する（1つでも欠けたら共通ではない）
        ComponentCounterparts counterparts;
        bool commonToAll = true;
        for (auto *obj : selectedObjects) {
            if (!obj || obj == primary) continue;
            IObjectComponent *match = FindComponentByTypeOrdinal(obj, typeName, ordinal);
            if (!match) {
                commonToAll = false;
                break;
            }
            counterparts.emplace_back(obj, match);
        }
        if (!commonToAll) continue;

        ImGui::PushID(id);
        ImGui::Separator();
        bool componentActive = comp->IsActive();
        if (ImGui::Checkbox("##Active", &componentActive)) {
            comp->SetActive(componentActive);
            for (auto &[obj, counterpart] : counterparts) {
                counterpart->SetActive(componentActive);
            }
        }
        ImGui::SameLine();
        // 開閉状態はコンポーネントの種類ごとに保存される（単一選択時と共有）
        bool headerOpen = EditorSettings::PersistentTreeNode(typeName.c_str(), "inspector.component." + typeName);
        if (headerOpen) {
            if (ImGui::BeginPopupContextItem("ComponentContextMenu")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removeTypeName = typeName;
                    pendingRemoves.push_back({ primary, comp });
                    for (auto &[obj, counterpart] : counterparts) {
                        pendingRemoves.push_back({ obj, counterpart });
                    }
                }
                ImGui::EndPopup();
            }
            // パラメータ変更をUndo履歴へ積むため、表示前後の状態を比較する
            JSON before = primary->SaveComponentToJson(comp);
            std::string componentTag = comp->GetTagName();
            if (ImGui::InputText("Tag", &componentTag)) {
                comp->SetTag(componentTag);
            }
            primary->ShowComponentImGui(comp);
            JSON after = primary->SaveComponentToJson(comp);
            if (before != after) {
                // 先にTrackComponentEditで対象の「変更前」状態を控えてから、差分を適用する
                TrackComponentEdit(primary, comp, before, after, counterparts);
                ApplyEditToCounterparts(before, after, counterparts);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++id;
    }

    //--------- コンポーネント削除（全選択オブジェクトの対応コンポーネントをまとめて削除する） ---------//
    if (!pendingRemoves.empty()) {
        CommitPendingEdit();
        if (commands_) {
            auto composite = std::make_unique<CompositeCommand>(
                "Remove Component: " + removeTypeName + " (" + std::to_string(pendingRemoves.size()) + " Objects)");
            for (const auto &remove : pendingRemoves) {
                composite->AddCommand(std::make_unique<RemoveComponentCommand>(remove.object, remove.component));
            }
            commands_->Execute(std::move(composite));
        } else {
            for (const auto &remove : pendingRemoves) {
                remove.object->RemoveComponent(remove.component);
            }
        }
    }

    //--------- コンポーネント追加（全選択オブジェクトへ追加する） ---------//
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        std::string selectedType;
        if (ComponentAddMenu::Show(GetRegisteredObjectComponentTypes(),
                [](const std::string &typeName) -> const std::vector<std::string> & { return GetObjectComponentCategory(typeName); },
                selectedType)) {
            if (commands_) {
                auto composite = std::make_unique<CompositeCommand>(
                    "Add Component: " + selectedType + " (" + std::to_string(selectedObjects.size()) + " Objects)");
                for (auto *obj : selectedObjects) {
                    if (obj) composite->AddCommand(std::make_unique<AddComponentCommand>(obj, selectedType));
                }
                commands_->Execute(std::move(composite));
            } else {
                for (auto *obj : selectedObjects) {
                    if (!obj) continue;
                    auto newComp = CreateObjectComponentByType(selectedType);
                    if (newComp) obj->AddComponent(std::move(newComp));
                }
            }
        }
        ImGui::EndPopup();
    }
}

namespace {
/// @brief before→afterで「変更された値」だけを含むJSONパッチを作る
/// @details オブジェクト同士は再帰的に比較し、配列・プリミティブ値は変更があれば丸ごとafterの値になる。
///          結果はnlohmann::jsonのmerge_patch（RFC 7386）でそのまま適用できる形になる
JSON MakeChangedValuePatch(const JSON &before, const JSON &after) {
    if (!before.is_object() || !after.is_object()) {
        return (before == after) ? JSON() : after;
    }
    JSON patch = JSON::object();
    for (auto it = after.begin(); it != after.end(); ++it) {
        if (!before.contains(it.key())) {
            patch[it.key()] = it.value();
            continue;
        }
        const JSON &beforeValue = before[it.key()];
        if (beforeValue == it.value()) continue;
        if (beforeValue.is_object() && it.value().is_object()) {
            JSON sub = MakeChangedValuePatch(beforeValue, it.value());
            if (sub.is_object() && !sub.empty()) patch[it.key()] = sub;
        } else {
            patch[it.key()] = it.value();
        }
    }
    return patch;
}
} // namespace

void SceneObjectInspector::ApplyEditToCounterparts(const JSON &before, const JSON &after, const ComponentCounterparts &counterparts) {
    // 変更された値だけを適用することで、対象ごとに異なる他の値（位置など）は保持される
    const JSON patch = MakeChangedValuePatch(before, after);
    if (!patch.is_object() || patch.empty()) return;
    for (const auto &[obj, comp] : counterparts) {
        if (!obj || !comp) continue;
        JSON current = obj->SaveComponentToJson(comp);
        current.merge_patch(patch);
        obj->LoadComponentFromJson(comp, current);
    }
}

IObjectComponent *SceneObjectInspector::FindComponentByTypeOrdinal(EmptyObject *obj, const std::string &typeName, size_t ordinal) {
    size_t count = 0;
    for (IObjectComponent *comp : GetOrderedComponents(obj)) {
        if (comp->GetComponentType() != typeName) continue;
        if (count == ordinal) return comp;
        ++count;
    }
    return nullptr;
}

void SceneObjectInspector::TrackComponentEdit(EmptyObject *obj, IObjectComponent *component, const JSON &before, const JSON &after,
    const ComponentCounterparts &linkedTargets) {
    // 別のコンポーネントの編集が始まった場合は先に確定させる
    if (hasPendingEdit_ && (editObject_ != obj || editComponent_ != component)) {
        CommitPendingEdit();
    }

    if (!hasPendingEdit_) {
        hasPendingEdit_ = true;
        editObject_ = obj;
        editComponent_ = component;
        editBefore_ = before;
        // 一括編集対象の「変更前」状態は編集セッション開始時点のものを控える
        // （呼び出し元は、このメソッドを呼んだ後に差分を対象へ適用すること）
        editLinkedTargets_.clear();
        for (const auto &[linkedObject, linkedComponent] : linkedTargets) {
            if (!linkedObject || !linkedComponent) continue;
            editLinkedTargets_.push_back({ linkedObject, linkedComponent, linkedObject->SaveComponentToJson(linkedComponent) });
        }
    }
    editAfter_ = after;
}

void SceneObjectInspector::CommitPendingEdit() {
    if (!hasPendingEdit_) return;

    // スクリプト、Undo/Redo、再生停止によるシーン復元で、編集開始時のポインターが
    // UIフレームの途中に無効化される場合がある。所有コンテナとの照合前には参照解除しない。
    EmptyObject *editObject = context_ ? context_->GetSceneObject(editObject_) : nullptr;
    IObjectComponent *editComponent = editObject ? editObject->GetComponent(editComponent_) : nullptr;
    if (!editObject || !editComponent) {
        ResetPendingEdit();
        return;
    }

    if (commands_ && editBefore_ != editAfter_) {
        if (editLinkedTargets_.empty()) {
            commands_->PushExecuted(std::make_unique<ComponentEditCommand>(editObject, editComponent, editBefore_, editAfter_));
        } else {
            // 複数選択の一括編集は、全対象の変更をまとめて1つのUndo操作にする
            auto composite = std::make_unique<CompositeCommand>(
                "Edit Component: " + editComponent->GetComponentType() + " (" + std::to_string(editLinkedTargets_.size() + 1) + " Objects)");
            composite->AddCommand(std::make_unique<ComponentEditCommand>(editObject, editComponent, editBefore_, editAfter_));
            for (const auto &target : editLinkedTargets_) {
                EmptyObject *linkedObject = context_->GetSceneObject(target.object);
                IObjectComponent *linkedComponent = linkedObject ? linkedObject->GetComponent(target.component) : nullptr;
                if (!linkedObject || !linkedComponent) continue;
                JSON after = linkedObject->SaveComponentToJson(linkedComponent);
                if (after != target.before) {
                    composite->AddCommand(std::make_unique<ComponentEditCommand>(linkedObject, linkedComponent, target.before, after));
                }
            }
            if (!composite->IsEmpty()) commands_->PushExecuted(std::move(composite));
        }
    }
    ResetPendingEdit();
}

void SceneObjectInspector::ResetPendingEdit() {
    hasPendingEdit_ = false;
    editObject_ = nullptr;
    editComponent_ = nullptr;
    editBefore_ = JSON();
    editAfter_ = JSON();
    editLinkedTargets_.clear();
}

void SceneObjectInspector::FlushPendingComponentEdit() {
    if (!hasPendingEdit_) return;
    // 編集ウィジェットの操作が終わったタイミングでひとつの操作として確定する
    if (ImGui::IsAnyItemActive()) return;
    CommitPendingEdit();
}

std::vector<IObjectComponent *> SceneObjectInspector::GetOrderedComponents(EmptyObject *obj) {
    // (更新優先度, 追加順ID) で並べる。実際の Update 実行順（EmptyObject::RegenerateUpdateComponentsList）と一致させる
    std::vector<std::pair<IObjectComponent *, size_t>> entries;
    for (const auto &entry : obj->GetAllComponents()) {
        if (!entry.first) continue;
        entries.emplace_back(entry.first.get(), entry.second);
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        if (a.first->GetUpdatePriority() != b.first->GetUpdatePriority()) {
            return a.first->GetUpdatePriority() < b.first->GetUpdatePriority();
        }
        return a.second < b.second;
    });

    std::vector<IObjectComponent *> result;
    result.reserve(entries.size());
    for (const auto &entry : entries) {
        result.push_back(entry.first);
    }
    return result;
}

void SceneObjectInspector::DragAndDropComponent(IObjectComponent *comp) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ComponentDragDropPayload dndPayload;
        dndPayload.component = comp;
        ImGui::SetDragDropPayload(kComponentDragDropType, &dndPayload, sizeof(dndPayload));
        ImGui::Text("%s", comp->GetComponentType().c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags targetFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kComponentDragDropType, targetFlags)) {
            const ImVec2 itemRectMin = ImGui::GetItemRectMin();
            const ImVec2 itemRectMax = ImGui::GetItemRectMax();
            const float mouseY = ImGui::GetMousePos().y;
            const float itemMidY = (itemRectMin.y + itemRectMax.y) * 0.5f;
            const ComponentDropPosition dropPosition = (mouseY < itemMidY) ? ComponentDropPosition::Above : ComponentDropPosition::Below;

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            constexpr ImU32 kHighlightColor = IM_COL32(0, 255, 0, 255);
            const float lineY = (dropPosition == ComponentDropPosition::Above) ? itemRectMin.y : itemRectMax.y;
            drawList->AddLine(ImVec2(itemRectMin.x, lineY), ImVec2(itemRectMax.x, lineY), kHighlightColor, 2.0f);

            if (payload->IsDelivery()) {
                IM_ASSERT(payload->DataSize == sizeof(ComponentDragDropPayload));
                auto *dndPayload = static_cast<const ComponentDragDropPayload *>(payload->Data);
                componentDragSource_ = dndPayload->component;
                componentDragTarget_ = comp;
                componentDragPosition_ = dropPosition;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectInspector::ApplyComponentDragAndDrop(EmptyObject *obj) {
    IObjectComponent *source = componentDragSource_;
    IObjectComponent *target = componentDragTarget_;
    const ComponentDropPosition position = componentDragPosition_;
    componentDragSource_ = nullptr;
    componentDragTarget_ = nullptr;
    if (!source || !target || source == target) return;

    std::vector<IObjectComponent *> ordered = GetOrderedComponents(obj);
    auto sourceIt = std::find(ordered.begin(), ordered.end(), source);
    if (sourceIt == ordered.end()) return;
    ordered.erase(sourceIt);

    auto targetIt = std::find(ordered.begin(), ordered.end(), target);
    if (targetIt == ordered.end()) return;
    size_t insertIndex = static_cast<size_t>(std::distance(ordered.begin(), targetIt));
    if (position == ComponentDropPosition::Below) ++insertIndex;
    ordered.insert(ordered.begin() + insertIndex, source);

    // 新しい並び順どおりに更新優先度を振り直す（変更があったものだけUndo履歴へ積む）
    auto composite = std::make_unique<CompositeCommand>("Reorder Component: " + source->GetComponentType());
    bool anyChange = false;
    for (size_t i = 0; i < ordered.size(); ++i) {
        IObjectComponent *comp = ordered[i];
        const int newPriority = static_cast<int>(i);
        if (comp->GetUpdatePriority() == newPriority) continue;

        const JSON before = obj->SaveComponentToJson(comp);
        comp->SetUpdatePriority(newPriority);
        const JSON after = obj->SaveComponentToJson(comp);
        if (before != after) {
            composite->AddCommand(std::make_unique<ComponentEditCommand>(obj, comp, before, after));
            anyChange = true;
        }
    }
    if (anyChange && commands_) {
        commands_->PushExecuted(std::move(composite));
    }
}

} // namespace KashipanEngine
