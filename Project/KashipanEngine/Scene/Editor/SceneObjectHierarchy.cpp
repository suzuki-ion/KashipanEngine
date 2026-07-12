#include "SceneObjectHierarchy.h"
#include <algorithm>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Objects/Components/Transform.h"

namespace KashipanEngine {

namespace {
/// @brief クリップボードのノード列を貼り付け用に複製し、全ノードへ新しいobjectIDを割り当てて
///        部分木内部の親子参照（Transformの"parent"）を新IDへ張り替える。
///        同じクリップボードから複数回貼り付けてもUUIDが衝突しないよう、貼り付けの都度呼び出すこと。
/// @param preserveRootParent trueの場合、ルートノード（parentIndexInSubtree<0）の"parent"参照は
///        元のまま変更しない（複製時に、複製元と同じ親へ自動的に接続されるようにするため）。
///        falseの場合はルートノードの"parent"を消去する（PasteObjectCommand側でattachParentへ接続する）。
std::vector<PasteObjectCommand::Node> PrepareNodesForInstantiation(const std::vector<PasteObjectCommand::Node> &source, bool preserveRootParent) {
    std::vector<PasteObjectCommand::Node> result = source;

    std::vector<UUID128> freshIDs;
    freshIDs.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        freshIDs.emplace_back(true);
    }

    for (size_t i = 0; i < result.size(); ++i) {
        JSON &json = result[i].json;
        json["objectID"] = freshIDs[i].ToString();
        if (!json.contains("components")) continue;
        for (auto &compJson : json["components"]) {
            if (compJson.value("type", "") != "Transform" || !compJson.contains("data")) continue;
            auto &data = compJson["data"];
            const int parentIndex = result[i].parentIndexInSubtree;
            if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < freshIDs.size()) {
                data["parent"] = freshIDs[static_cast<size_t>(parentIndex)].ToString();
            } else if (!preserveRootParent) {
                data.erase("parent");
            }
        }
    }
    return result;
}

/// @brief objがcandidatesのいずれかの子孫であるかをTransformの親参照チェーンから判定する
bool IsDescendantOfAny(EmptyObject *obj, const std::unordered_set<EmptyObject *> &candidates) {
    auto *transform = obj->GetComponent<Transform>();
    EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
    while (parent) {
        if (candidates.contains(parent)) return true;
        auto *parentTransform = parent->GetComponent<Transform>();
        parent = parentTransform ? parentTransform->GetParentObject() : nullptr;
    }
    return false;
}
} // namespace

void SceneObjectHierarchy::ShowImGui() {
    // このフレームの表示順はShowObjectItemの呼び出し毎に積み直す（Shift範囲選択の計算に使う）
    visibleOrderThisFrame_.clear();
    RebuildObjectItems();

    ImGui::Begin("Scene Object Hierarchy");

    HandleKeyboardShortcuts();

    if (EditorSettings::PersistentCollapsingHeader("Objects", "hierarchy.objects")) {
        size_t index = 0;
        for (size_t i = 0; i < objectItems_.size(); ++i) {
            ShowObjectItem(objectItems_[i], index);
            ++index;
        }
    }

    // Shift範囲選択はツリー全体の表示順（visibleOrderThisFrame_）が確定してからでないと
    // 対象範囲を計算できないため、全アイテムの描画後にまとめて適用する
    ApplyPendingRangeSelect();

    ShowHierarchyContextMenu();
    ApplyDragAndDrop();

    ImGui::End();
}

void SceneObjectHierarchy::RebuildObjectItems() {
    const auto &objects = editorContext_->GetSceneObjects();
    objectItems_.clear();
    objectParentMap_.clear();
    objectItems_.reserve(objects.size());
    objectParentMap_.reserve(objects.size());

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto &obj = objects[i];
        if (!obj) continue;

        objectParentMap_.emplace(obj.get(), std::vector<std::pair<EmptyObject *, size_t>>{});
        auto *transform = obj->GetComponent<Transform>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            objectParentMap_[parent].push_back({ obj.get(), i });
        } else {
            ObjectItem item{};
            item.object = obj.get();
            item.name = obj->GetName();
            item.depth = 0;
            item.originalIndex = i;
            objectItems_.push_back(std::move(item));
        }
    }

    for (auto &item : objectItems_) {
        RecursivelyBuildObjectItems(item.object, item, 0);
    }
}

void SceneObjectHierarchy::RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth) {
    for (const auto &childPair : objectParentMap_[obj]) {
        ObjectItem childItem{};
        childItem.object = childPair.first;
        childItem.name = childPair.first->GetName();
        childItem.depth = depth + 1;
        childItem.originalIndex = childPair.second;
        item.children.push_back(std::move(childItem));
        RecursivelyBuildObjectItems(childPair.first, item.children.back(), depth + 1);
    }
}

void SceneObjectHierarchy::ShowObjectItem(const ObjectItem &item, size_t &index) {
    // このフレームの表示順を記録する（Shift範囲選択の範囲計算に使う）
    visibleOrderThisFrame_.push_back(item.object);

    // インデックスではなくオブジェクトをIDにすることで、
    // オブジェクトの追加/削除があっても開閉状態が別のオブジェクトへずれないようにする
    ImGui::PushID(item.object);
    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Indent();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (item.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    } else {
        flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    }
    if (selectedObjects_.contains(item.object)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 開閉状態を保存・復元する（デフォルトは開いた状態）
    bool storedOpen = true;
    std::string settingsKey;
    if (!item.children.empty()) {
        settingsKey = "hierarchy.object." + item.object->GetObjectID().ToString();
        storedOpen = EditorSettings::GetBool(settingsKey, true);
        ImGui::SetNextItemOpen(storedOpen, ImGuiCond_Once);
    }

    // 非アクティブなオブジェクトは灰色の文字で表示する
    const bool isInactive = !item.object->IsActive();
    if (isInactive) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    const bool isOpen = ImGui::TreeNodeEx(item.name.c_str(), flags);
    if (isInactive) {
        ImGui::PopStyleColor();
    }
    if (!item.children.empty() && isOpen != storedOpen) {
        EditorSettings::SetBool(settingsKey, isOpen);
    }
    DragAndDropObject(const_cast<ObjectItem *>(&item));
    ShowObjectContextMenu(item.object);

    // クリックした瞬間（mouse down）に選択を確定すると、インスペクターがすぐ切り替わってしまい
    // インスペクター側へのD&D（ヒエラルキーからオブジェクトをドラッグしてフィールドへ設定する操作）が
    // 阻害されるため、実際にドラッグへ発展しなかった場合に限り、指を離した時点で選択を確定する。
    // ドラッグ判定は BeginDragDropSource() の成否ではなく、実際のマウス移動距離で厳密に行う
    // （BeginDragDropSource は一部ウィジェットでしきい値が変わることがあり、Ctrl/Shiftキーを
    // 同時に押しながらのクリックで手ブレ的な微小移動が起きた際に誤ってドラッグ扱いされ、
    // 選択処理そのものがスキップされてしまう不具合があったため）。
    if (ImGui::IsItemDeactivated()) {
        const ImGuiIO &io = ImGui::GetIO();
        const float dragThreshold = io.MouseDragThreshold;
        const bool wasDragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > (dragThreshold * dragThreshold);
        if (!wasDragged) {
            if (ImGui::IsKeyDown(ImGuiMod_Shift)) {
                // Shift+クリック: 範囲選択。対象の並び順はツリー全体を描画し終えないと確定しないため、
                // ここでは要求のみ記録し、ApplyPendingRangeSelect() で確定させる。
                pendingRangeTarget_ = item.object;
            } else if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                // Ctrl+クリック: 個別に選択/選択解除をトグルする
                if (selectedObjects_.contains(item.object)) {
                    selectedObjects_.erase(item.object);
                    if (selectedObject_ == item.object) {
                        selectedObject_ = selectedObjects_.empty() ? nullptr : *selectedObjects_.begin();
                    }
                } else {
                    selectedObjects_.insert(item.object);
                    selectedObject_ = item.object;
                }
                selectionAnchorObject_ = item.object;
            } else {
                // 修飾キー無しのクリックは単一選択に置き換える
                selectedObjects_.clear();
                selectedObjects_.insert(item.object);
                selectedObject_ = item.object;
                selectionAnchorObject_ = item.object;
            }
        }
    }

    if (!item.children.empty() && isOpen) {
        for (const auto &child : item.children) {
            ShowObjectItem(child, ++index);
        }
        ImGui::TreePop();
    }

    for (size_t i = 0; i < item.depth; ++i) {
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObjectContextMenu(EmptyObject *obj) {
    if (ImGui::BeginPopupContextItem("ObjectContextMenu")) {
        // 右クリックしたオブジェクトが複数選択に含まれる場合、Copy/Clone/Deleteは選択中の全オブジェクトを対象にする
        // （Create/Pasteはあくまで右クリックした1点を基準にした挿入操作のため対象外）
        const std::vector<EmptyObject *> targets =
            (selectedObjects_.size() > 1 && selectedObjects_.contains(obj)) ? GetSelectionRoots() : std::vector<EmptyObject *>{ obj };

        if (ImGui::BeginMenu("Create Object")) {
            // 右クリックしたオブジェクトと同じ階層かつ次のインデックス位置に作成する
            ShowCreateObjectMenu(obj, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create Child Object")) {
            // 右クリックしたオブジェクトの子オブジェクトとして最後尾に作成する
            ShowCreateObjectMenu(obj, true);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        const std::string copyLabel = (targets.size() > 1) ? ("Copy " + std::to_string(targets.size()) + " Objects") : "Copy Object";
        if (ImGui::MenuItem(copyLabel.c_str(), "Ctrl+C")) {
            CopyObjects(targets);
        }
        if (ImGui::MenuItem("Paste Object", "Ctrl+V", false, !clipboardNodes_.empty())) {
            auto *transform = obj->GetComponent<Transform>();
            EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
            const size_t index = editorContext_->GetObjectIndex(obj);
            const size_t insertIndex = (index == MAXSIZE_T) ? MAXSIZE_T : index + 1;
            PasteObject(parent, insertIndex);
        }
        if (ImGui::MenuItem("Paste to Child Object", "Ctrl+Shift+V", false, !clipboardNodes_.empty())) {
            PasteObject(obj, MAXSIZE_T);
        }
        const std::string cloneLabel = (targets.size() > 1) ? ("Clone " + std::to_string(targets.size()) + " Objects") : "Clone Object";
        if (ImGui::MenuItem(cloneLabel.c_str(), "Ctrl+D")) {
            CloneObjects(targets);
        }
        ImGui::Separator();
        const std::string deleteLabel = (targets.size() > 1) ? ("Delete " + std::to_string(targets.size()) + " Objects") : "Delete Object";
        if (ImGui::MenuItem(deleteLabel.c_str())) {
            DeleteObjects(targets);
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowHierarchyContextMenu() {
    // ImGuiPopupFlags_NoOpenOverItems を指定しないと、オブジェクト項目上での右クリックでも
    // この window レベルのメニューが同一フレームで開いてしまい、
    // オブジェクト自体の ObjectContextMenu を閉じてしまう（表示されないように見える）ため必須。
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create Object")) {
            ShowCreateObjectMenu(nullptr, false);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Paste Object", "Ctrl+V", false, !clipboardNodes_.empty())) {
            PasteObject(nullptr, MAXSIZE_T);
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowCreateObjectMenu(EmptyObject *referenceObject, bool asChild) {
    if (ImGui::MenuItem("Empty Object")) {
        CreateTemplateObject("EmptyObject", {}, referenceObject, asChild);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("3D")) {
        if (ImGui::MenuItem("Mesh Object")) {
            CreateTemplateObject("Mesh Object", { "MeshFilter", "MeshRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem("Camera Object")) {
            CreateTemplateObject("Camera Object", { "Camera3D", "CameraRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem("Light Object")) {
            CreateTemplateObject("Light Object", { "Light", "LightRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem("Skinned Mesh Object")) {
            CreateTemplateObject("Skinned Mesh Object", { "MeshFilter", "SkinnedMeshRenderer" }, referenceObject, asChild);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Render Target")) {
        if (ImGui::MenuItem("Window Object")) {
            CreateTemplateObject("Window Object", { "NormalWindowObject" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem("Screen Buffer Object")) {
            CreateTemplateObject("Screen Buffer Object", { "ScreenBufferObject" }, referenceObject, asChild);
        }
        ImGui::EndMenu();
    }
}

void SceneObjectHierarchy::CreateTemplateObject(const std::string &objectName, const std::vector<std::string> &componentTypes,
    EmptyObject *referenceObject, bool asChild) {
    UUID128 newObjectID;
    std::unique_ptr<IEditorCommand> createCommand;

    if (asChild) {
        auto cmd = std::make_unique<CreateChildObjectCommand>(objectName, referenceObject);
        newObjectID = cmd->GetObjectID();
        createCommand = std::move(cmd);
    } else if (referenceObject) {
        const size_t index = editorContext_->GetObjectIndex(referenceObject);
        const size_t newIndex = (index == MAXSIZE_T) ? MAXSIZE_T : index + 1;
        auto cmd = std::make_unique<CreateSiblingObjectCommand>(objectName, referenceObject, newIndex);
        newObjectID = cmd->GetObjectID();
        createCommand = std::move(cmd);
    } else {
        auto cmd = std::make_unique<CreateObjectCommand>(objectName);
        newObjectID = cmd->GetObjectID();
        createCommand = std::move(cmd);
    }

    if (commands_) {
        auto composite = std::make_unique<CompositeCommand>("Create " + objectName);
        composite->AddCommand(std::move(createCommand));
        for (const auto &type : componentTypes) {
            composite->AddCommand(std::make_unique<AddComponentCommand>(newObjectID, type));
        }
        commands_->Execute(std::move(composite));
        return;
    }

    // Undo/Redo管理が無い場合のフォールバック（コマンドを介さず直接生成する）
    EmptyObject *newObj = nullptr;
    if (asChild) {
        newObj = editorContext_->CreateEmptyObject(objectName);
        if (auto *newTransform = newObj ? newObj->GetComponent<Transform>() : nullptr) {
            newTransform->SetParentObject(referenceObject);
        }
    } else if (referenceObject) {
        const size_t index = editorContext_->GetObjectIndex(referenceObject);
        const size_t newIndex = (index == MAXSIZE_T) ? MAXSIZE_T : index + 1;
        newObj = editorContext_->CreateEmptyObject(objectName, UUID128(), newIndex);
        auto *refTransform = referenceObject->GetComponent<Transform>();
        if (auto *parent = refTransform ? refTransform->GetParentObject() : nullptr) {
            if (auto *newTransform = newObj ? newObj->GetComponent<Transform>() : nullptr) {
                newTransform->SetParentObject(parent);
            }
        }
    } else {
        newObj = editorContext_->CreateEmptyObject(objectName);
    }

    if (!newObj) return;
    for (const auto &type : componentTypes) {
        if (auto comp = CreateObjectComponentByType(type)) {
            newObj->AddComponent(std::move(comp));
        }
    }
}

void SceneObjectHierarchy::HandleKeyboardShortcuts() {
    // ヒエラルキーウィンドウにフォーカスがある時のみ有効にする（テキスト入力中などに誤爆させないため）
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (!ImGui::IsKeyDown(ImGuiMod_Ctrl)) return;

    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        if (!selectedObjects_.empty()) CopyObjects(GetSelectionRoots());
        return;
    }
    if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        // Ctrl+Shift+V: 選択中オブジェクトの子として貼り付ける（選択が無ければルート直下へ）
        PasteObject(selectedObject_, MAXSIZE_T);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        EmptyObject *parent = nullptr;
        size_t insertIndex = MAXSIZE_T;
        if (selectedObject_) {
            auto *transform = selectedObject_->GetComponent<Transform>();
            parent = transform ? transform->GetParentObject() : nullptr;
            const size_t index = editorContext_->GetObjectIndex(selectedObject_);
            insertIndex = (index == MAXSIZE_T) ? MAXSIZE_T : index + 1;
        }
        PasteObject(parent, insertIndex);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        if (!selectedObjects_.empty()) CloneObjects(GetSelectionRoots());
        return;
    }
}

std::vector<EmptyObject *> SceneObjectHierarchy::GetSelectionRoots() const {
    std::vector<EmptyObject *> roots;
    for (auto *obj : selectedObjects_) {
        if (obj && !IsDescendantOfAny(obj, selectedObjects_)) roots.push_back(obj);
    }
    return roots;
}

void SceneObjectHierarchy::CopyObjects(const std::vector<EmptyObject *> &objs) {
    if (objs.empty() || !editorContext_) return;
    clipboardNodes_.clear();
    for (auto *obj : objs) {
        if (obj) CollectSubtreeNodes(obj, -1, clipboardNodes_);
    }
    clipboardRootName_ = (objs.size() == 1 && objs[0]) ? objs[0]->GetName() : (std::to_string(objs.size()) + " Objects");
}

void SceneObjectHierarchy::PasteObject(EmptyObject *attachParent, size_t insertIndex) {
    if (clipboardNodes_.empty() || !editorContext_) return;
    auto nodes = PrepareNodesForInstantiation(clipboardNodes_, /*preserveRootParent=*/false);
    ExecutePasteCommand(std::make_unique<PasteObjectCommand>(
        std::move(nodes), attachParent, insertIndex, clipboardRootName_, "Paste Object"));
}

void SceneObjectHierarchy::CloneObjects(const std::vector<EmptyObject *> &objs) {
    if (objs.empty() || !editorContext_) return;
    std::vector<PasteObjectCommand::Node> nodes;
    for (auto *obj : objs) {
        if (obj) CollectSubtreeNodes(obj, -1, nodes);
    }
    // preserveRootParent=true: 各複製元ごとに元の親が異なっていても、それぞれ元通りの親へ接続されるようにする
    auto prepared = PrepareNodesForInstantiation(nodes, /*preserveRootParent=*/true);

    EmptyObject *firstObj = objs.front();
    const size_t originalIndex = editorContext_->GetObjectIndex(firstObj);
    const size_t insertIndex = (originalIndex == MAXSIZE_T) ? MAXSIZE_T : originalIndex + 1;
    const std::string name = (objs.size() == 1) ? firstObj->GetName() : (std::to_string(objs.size()) + " Objects");

    ExecutePasteCommand(std::make_unique<PasteObjectCommand>(
        std::move(prepared), nullptr, insertIndex, name, "Clone Object", /*preserveOriginalRootParent=*/true));
}

void SceneObjectHierarchy::DeleteObjects(const std::vector<EmptyObject *> &objs) {
    if (objs.empty() || !editorContext_) return;

    if (commands_) {
        if (objs.size() == 1) {
            commands_->Execute(std::make_unique<DeleteObjectCommand>(objs[0]));
        } else {
            auto composite = std::make_unique<CompositeCommand>("Delete " + std::to_string(objs.size()) + " Objects");
            for (auto *obj : objs) {
                if (obj) composite->AddCommand(std::make_unique<DeleteObjectCommand>(obj));
            }
            commands_->Execute(std::move(composite));
        }
    } else {
        for (auto *obj : objs) {
            if (obj) editorContext_->DeleteObject(obj);
        }
    }
    ClearSelection();
}

void SceneObjectHierarchy::CollectSubtreeNodes(EmptyObject *obj, int parentIndex, std::vector<PasteObjectCommand::Node> &out) const {
    if (!obj || !editorContext_) return;
    const int myIndex = static_cast<int>(out.size());
    out.push_back({ editorContext_->SaveObjectToJson(obj), parentIndex });

    // 子オブジェクトをTransformの親参照から探す（RebuildObjectItemsが使う手法と同じ）
    for (const auto &objPtr : editorContext_->GetSceneObjects()) {
        EmptyObject *candidate = objPtr.get();
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            CollectSubtreeNodes(candidate, myIndex, out);
        }
    }
}

void SceneObjectHierarchy::ExecutePasteCommand(std::unique_ptr<PasteObjectCommand> command) {
    if (!command || !editorContext_) return;
    PasteObjectCommand *rawCommand = command.get();
    const bool succeeded = commands_ ? commands_->Execute(std::move(command)) : rawCommand->Execute(editorContext_);
    if (!succeeded) return;

    auto newRoots = rawCommand->GetRootObjects(editorContext_);
    if (!newRoots.empty()) {
        selectedObjects_.clear();
        for (auto *obj : newRoots) selectedObjects_.insert(obj);
        selectedObject_ = newRoots.back();
        selectionAnchorObject_ = newRoots.back();
    }
}

void SceneObjectHierarchy::ApplyPendingRangeSelect() {
    if (!pendingRangeTarget_) return;
    EmptyObject *target = pendingRangeTarget_;
    pendingRangeTarget_ = nullptr;

    EmptyObject *anchor = selectionAnchorObject_ ? selectionAnchorObject_ : target;
    auto anchorIt = std::find(visibleOrderThisFrame_.begin(), visibleOrderThisFrame_.end(), anchor);
    auto targetIt = std::find(visibleOrderThisFrame_.begin(), visibleOrderThisFrame_.end(), target);
    if (anchorIt == visibleOrderThisFrame_.end() || targetIt == visibleOrderThisFrame_.end()) {
        // 起点が非表示（親が折りたたまれている等）で見つからない場合は対象単体を選択する
        selectedObjects_.clear();
        selectedObjects_.insert(target);
        selectedObject_ = target;
        selectionAnchorObject_ = target;
        return;
    }

    size_t anchorIndex = static_cast<size_t>(std::distance(visibleOrderThisFrame_.begin(), anchorIt));
    size_t targetIndex = static_cast<size_t>(std::distance(visibleOrderThisFrame_.begin(), targetIt));
    if (anchorIndex > targetIndex) std::swap(anchorIndex, targetIndex);

    selectedObjects_.clear();
    for (size_t i = anchorIndex; i <= targetIndex; ++i) {
        selectedObjects_.insert(visibleOrderThisFrame_[i]);
    }
    selectedObject_ = target;
    // 起点はそのまま維持する（連続Shiftクリックで同じ起点から範囲を再計算できるようにするため）
}

void SceneObjectHierarchy::DragAndDropObject(ObjectItem *objItem) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        // 外部（インスペクター等）でも受け取れる共有ペイロード型で送る
        SceneObjectDragDropPayload dndPayload;
        dndPayload.object = objItem->object;
        ImGui::SetDragDropPayload(kSceneObjectDragDropType, &dndPayload, sizeof(dndPayload));
        ImGui::Text("%s", objItem->name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags targetFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSceneObjectDragDropType, targetFlags)) {
            DropPosition dropPosition = DragAndDropTargetCommon();
            if (payload->IsDelivery()) {
                IM_ASSERT(payload->DataSize == sizeof(SceneObjectDragDropPayload));
                auto *dndPayload = static_cast<const SceneObjectDragDropPayload *>(payload->Data);
                // ObjectItem は毎フレーム作り直される一時構造体のためフレームをまたいで保持しない。
                // 実体（EmptyObject*）のみを保持し、必要な情報は ApplyDragAndDrop 側で都度取得する。
                dragDropPayload_.objectSource = dndPayload->object;
                dragDropPayload_.objectTarget = objItem->object;
                dragDropPayload_.position = dropPosition;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectHierarchy::ApplyDragAndDrop() {
    if (!dragDropPayload_.objectSource || !dragDropPayload_.objectTarget) return;

    EmptyObject *sourceObject = dragDropPayload_.objectSource;
    EmptyObject *targetObject = dragDropPayload_.objectTarget;
    DropPosition position = dragDropPayload_.position;

    size_t moveIndex = editorContext_->GetObjectIndex(targetObject);
    auto *targetTransform = targetObject->GetComponent<Transform>();
    auto *targetParent = targetTransform ? targetTransform->GetParentObject() : nullptr;
    auto *sourceTransform = sourceObject->GetComponent<Transform>();
    if (!sourceTransform) {
        sourceTransform = sourceObject->AddComponent<Transform>();
    }

    bool isParentSet = false;
    if (sourceTransform) {
        // Undo用に移動前の状態を保存しておく
        const JSON transformBefore = sourceObject->SaveComponentToJson(sourceTransform);
        const size_t indexBefore = editorContext_->GetObjectIndex(sourceObject);
        size_t indexAfter = MAXSIZE_T;

        switch (position) {
        case DropPosition::Above:
            isParentSet = sourceTransform->SetParentObject(targetParent);
            if (isParentSet) {
                indexAfter = moveIndex;
                editorContext_->MoveObject(sourceObject, indexAfter);
            }
            break;
        case DropPosition::Below:
            isParentSet = sourceTransform->SetParentObject(targetParent);
            if (isParentSet) {
                indexAfter = moveIndex + 1;
                editorContext_->MoveObject(sourceObject, indexAfter);
            }
            break;
        case DropPosition::Inside:
            isParentSet = sourceTransform->SetParentObject(targetObject);
            if (isParentSet) {
                indexAfter = moveIndex;
                editorContext_->MoveObject(sourceObject, indexAfter);
            }
            break;
        default:
            break;
        }

        // 親変更と並び替えをひとつの操作としてUndo履歴へ積む
        if (isParentSet && commands_) {
            const JSON transformAfter = sourceObject->SaveComponentToJson(sourceTransform);
            auto composite = std::make_unique<CompositeCommand>("Move Object: " + sourceObject->GetName());
            if (transformBefore != transformAfter) {
                composite->AddCommand(std::make_unique<ComponentEditCommand>(
                    sourceObject, sourceTransform, transformBefore, transformAfter));
            }
            composite->AddCommand(std::make_unique<MoveObjectCommand>(sourceObject, indexBefore, indexAfter));
            commands_->PushExecuted(std::move(composite));
        }
    }

    dragDropPayload_.objectSource = nullptr;
    dragDropPayload_.objectTarget = nullptr;
}

SceneObjectHierarchy::DropPosition SceneObjectHierarchy::DragAndDropTargetCommon() {
    ImVec2 itemRectMin = ImGui::GetItemRectMin();
    ImVec2 itemRectMax = ImGui::GetItemRectMax();
    float mouseY = ImGui::GetMousePos().y;
    float itemHeight = itemRectMax.y - itemRectMin.y;

    const float thresholdUpper = itemRectMin.y + itemHeight * 0.25f;
    const float thresholdLower = itemRectMax.y - itemHeight * 0.25f;
    DropPosition dropPosition;

    if (mouseY < thresholdUpper) {
        dropPosition = DropPosition::Above;
    } else if (mouseY > thresholdLower) {
        dropPosition = DropPosition::Below;
    } else {
        dropPosition = DropPosition::Inside;
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImU32 highlightColor = IM_COL32(0, 255, 0, 255);
    float lineThickness = 2.0f;

    if (dropPosition == DropPosition::Above) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMin.y), ImVec2(itemRectMax.x, itemRectMin.y), highlightColor, lineThickness);
    } else if (dropPosition == DropPosition::Below) {
        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMax.y), ImVec2(itemRectMax.x, itemRectMax.y), highlightColor, lineThickness);
    } else {
        drawList->AddRect(itemRectMin, itemRectMax, highlightColor, 0.0f, 0, lineThickness);
    }

    return dropPosition;
}

} // namespace KashipanEngine
