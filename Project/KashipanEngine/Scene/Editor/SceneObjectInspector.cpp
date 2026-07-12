#include "SceneObjectInspector.h"
#include <algorithm>
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
        if (selectedObject) {
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

void SceneObjectInspector::TrackComponentEdit(EmptyObject *obj, IObjectComponent *component, const JSON &before, const JSON &after) {
    // 別のコンポーネントの編集が始まった場合は先に確定させる
    if (hasPendingEdit_ && (editObject_ != obj || editComponent_ != component)) {
        if (commands_ && editBefore_ != editAfter_) {
            commands_->PushExecuted(std::make_unique<ComponentEditCommand>(editObject_, editComponent_, editBefore_, editAfter_));
        }
        hasPendingEdit_ = false;
    }

    if (!hasPendingEdit_) {
        hasPendingEdit_ = true;
        editObject_ = obj;
        editComponent_ = component;
        editBefore_ = before;
    }
    editAfter_ = after;
}

void SceneObjectInspector::FlushPendingComponentEdit() {
    if (!hasPendingEdit_) return;
    // 編集ウィジェットの操作が終わったタイミングでひとつの操作として確定する
    if (ImGui::IsAnyItemActive()) return;

    if (commands_ && editBefore_ != editAfter_) {
        commands_->PushExecuted(std::make_unique<ComponentEditCommand>(editObject_, editComponent_, editBefore_, editAfter_));
    }
    hasPendingEdit_ = false;
    editObject_ = nullptr;
    editComponent_ = nullptr;
    editBefore_ = JSON();
    editAfter_ = JSON();
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
