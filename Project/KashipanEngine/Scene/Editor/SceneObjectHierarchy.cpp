#include "SceneObjectHierarchy.h"
#include <imgui_internal.h>
#include <algorithm>
#include <filesystem>
#include "ComponentSerialize/ComponentRegistry.h"
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabSyncUtility.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Utilities/Translation.h"
#include "Scene/Components/Script/EditorToolManager.h"
#include "Objects/Components/Comment.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/Components/Transform.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

namespace {
// ノード列の新ID割り当て・親子参照の張り替えはPrefabUtility::PrepareNodesForInstantiationを使用する
using PrefabUtility::PrepareNodesForInstantiation;

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

std::vector<UUID128> SceneObjectHierarchy::GetSelectedObjectIDs() const {
    std::vector<UUID128> ids;
    ids.reserve(selectedObjectIDs_.size());
    // 先頭にプライマリ（最後に操作したオブジェクト）を入れ、復元時にプライマリを維持する
    if (selectedObjectID_.IsValid()) ids.push_back(selectedObjectID_);
    for (const auto &id : selectedObjectIDs_) {
        if (id.IsValid() && id != selectedObjectID_) ids.push_back(id);
    }
    return ids;
}

void SceneObjectHierarchy::ValidateCachedObjects() {
    // selectedObject_ 等の生ポインタは「このフレーム用に解決済みのキャッシュ」であり、
    // 正であるUUID側から毎フレーム引き直す（プールのスロット再利用によるエイリアシング対策）。
    const auto isAlive = [this](EmptyObject *obj) {
        return obj && editorContext_ && editorContext_->GetSceneObject(obj) == obj;
    };

    selectedObjects_.clear();
    if (editorContext_) {
        for (const auto &id : selectedObjectIDs_) {
            if (auto *obj = editorContext_->GetSceneObject(id)) selectedObjects_.insert(obj);
        }
    }
    std::erase_if(selectedObjectIDs_, [&](const UUID128 &id) {
        return !editorContext_ || !editorContext_->GetSceneObject(id);
    });
    selectedObject_ = editorContext_ ? editorContext_->GetSceneObject(selectedObjectID_) : nullptr;
    if (!selectedObject_) {
        SetSelectedObject(selectedObjects_.empty() ? nullptr : *selectedObjects_.begin());
    }
    AddToSelectionSet(selectedObject_);

    selectionAnchorObject_ = editorContext_ ? editorContext_->GetSceneObject(selectionAnchorObjectID_) : nullptr;
    if (!selectionAnchorObject_) SetSelectionAnchor(selectedObject_);

    pendingRevertPrefabTarget_ = editorContext_ ? editorContext_->GetSceneObject(pendingRevertPrefabTargetID_) : nullptr;
    if (!pendingRevertPrefabTarget_) pendingRevertPrefabTargetID_ = UUID128();

    // 以下は同一フレーム内でのみ設定・消費されるため生ポインタのままで安全
    // （設定箇所・消費箇所のコメント参照。フレームをまたぐエイリアシングの窓が生じない）
    if (!isAlive(pendingRangeTarget_)) pendingRangeTarget_ = nullptr;
    if (!isAlive(pendingScrollToObject_)) pendingScrollToObject_ = nullptr;
    std::erase_if(forceOpenAncestors_, [&](EmptyObject *obj) { return !isAlive(obj); });

    if (!isAlive(dragDropPayload_.objectSource) || !isAlive(dragDropPayload_.objectTarget)) {
        dragDropPayload_ = {};
    }
    if (pendingPrefabDropParent_ && !isAlive(pendingPrefabDropParent_)) {
        pendingPrefabDropParent_ = nullptr;
    }
}

void SceneObjectHierarchy::RequestScrollTo(EmptyObject *obj) {
    pendingScrollToObject_ = obj;
    forceOpenAncestors_.clear();
    if (!obj) return;
    // 祖先を辿ってツリー開閉の強制対象に登録する（IsDescendantOfAnyと同じ辿り方）
    auto *transform = obj->GetComponent<Transform>();
    EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
    while (parent) {
        forceOpenAncestors_.insert(parent);
        auto *parentTransform = parent->GetComponent<Transform>();
        parent = parentTransform ? parentTransform->GetParentObject() : nullptr;
    }
}

void SceneObjectHierarchy::RestoreSelection(const std::vector<UUID128> &objectIDs) {
    ClearSelection();
    if (!editorContext_) return;
    for (const auto &id : objectIDs) {
        EmptyObject *obj = editorContext_->GetSceneObject(id);
        if (!obj) continue; // Undo/Redoで削除されたオブジェクトはスキップ
        AddToSelectionSet(obj);
        if (!selectedObject_) SetSelectedObject(obj); // 先頭（プライマリ）を維持する
    }
    SetSelectionAnchor(selectedObject_);
}

void SceneObjectHierarchy::ShowImGui() {
    ValidateCachedObjects();
    // このフレームの表示順はShowObjectItemの呼び出し毎に積み直す（Shift範囲選択の計算に使う）
    visibleOrderThisFrame_.clear();
    // Prefab Override判定用のPrefab単位索引キャッシュも毎フレーム作り直す（Apply/Revert等で
    // 内容が変わり得るため、フレームをまたいで持ち越さない）
    prefabOverrideCache_.nodeIndexByPrefab.clear();
    RebuildObjectItems();

    if (ImGui::Begin(TranslationLabel("editor.sceneobjecthierarchy.window"))) {
        HandleKeyboardShortcuts();

        if (EditorSettings::PersistentCollapsingHeader("Objects", "hierarchy.objects")) {
            size_t index = 0;
            for (size_t i = 0; i < objectItems_.size(); ++i) {
                ShowObjectItem(objectItems_[i], index);
                ++index;
            }

            // シーンビュー等からのスクロール要求はこの描画で消費し終わったのでクリアする
            // （ImGui自身が開閉状態をID単位で覚えているため、強制的に開いた祖先は以後も開いたまま残る）
            pendingScrollToObject_ = nullptr;
            forceOpenAncestors_.clear();
        }

        // Shift範囲選択はツリー全体の表示順（visibleOrderThisFrame_）が確定してからでないと
        // 対象範囲を計算できないため、全アイテムの描画後にまとめて適用する
        ApplyPendingRangeSelect();

        ShowHierarchyContextMenu();
        ApplyDragAndDrop();

        // ウィンドウの空き領域へのプレハブドロップ（ルート直下へ配置する）。
        // アイテム上のドロップはアイテム側のターゲット（矩形が小さい方）が優先される
        if (ImGuiWindow *window = ImGui::GetCurrentWindow(); window &&
            ImGui::BeginDragDropTargetCustom(window->InnerRect, window->ID)) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kPrefabAssetDragDropType)) {
                IM_ASSERT(payload->DataSize == sizeof(AssetDragDropPayload));
                const auto *assetPayload = static_cast<const AssetDragDropPayload *>(payload->Data);
                pendingPrefabDropPath_ = assetPayload->assetPath;
                pendingPrefabDropParent_ = nullptr;
                hasPendingPrefabDrop_ = true;
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::End();

    // プレハブのドロップ要求はツリーの描画が終わってから処理する
    if (hasPendingPrefabDrop_) {
        hasPendingPrefabDrop_ = false;
        InstantiatePrefabFile(pendingPrefabDropPath_, pendingPrefabDropParent_);
        pendingPrefabDropPath_.clear();
        pendingPrefabDropParent_ = nullptr;
    }

    ShowRevertPrefabConfirmModal();
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

        objectParentMap_.emplace(obj, std::vector<std::pair<EmptyObject *, size_t>>{});
        auto *transform = obj->GetComponent<Transform>();
        auto *parent = transform ? transform->GetParentObject() : nullptr;
        if (parent) {
            objectParentMap_[parent].push_back({ obj, i });
        } else {
            ObjectItem item{};
            item.object = obj;
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
    // シーンビュー等からの選択でスクロール対象の祖先にあたる場合、開閉状態（保存値）を書き換えず
    // このフレームだけ強制的に開く（ImGuiが以後もそのID分の開閉状態を覚えているため、
    // 一度開けば以後も自然に開いたままになる）
    const bool forceOpen = forceOpenAncestors_.contains(item.object);
    if (!item.children.empty()) {
        settingsKey = "hierarchy.object." + item.object->GetObjectID().ToString();
        storedOpen = EditorSettings::GetBool(settingsKey, true);
        if (forceOpen) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        } else {
            ImGui::SetNextItemOpen(storedOpen, ImGuiCond_Once);
        }
    }

    // 非アクティブなオブジェクトは灰色、EditorOnlyオブジェクト（祖先を含む）は水色、
    // Prefabインスタンスは青（Overrideがあればamber＋末尾に" *"）の文字で表示する
    const bool isInactive = !item.object->IsActive();
    const bool isEditorOnly = !isInactive && item.object->IsEditorOnlyInHierarchy();
    bool isPrefabMember = false;
    bool hasPrefabOverride = false;
    if (!isInactive && !isEditorOnly && PrefabSyncUtility::FindEnclosingPrefabInstanceRoot(item.object)) {
        isPrefabMember = true;
        hasPrefabOverride = PrefabSyncUtility::HasComponentOverrideCached(editorContext_, item.object, prefabOverrideCache_);
    }
    if (isInactive) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    } else if (isEditorOnly) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.8f, 1.0f, 1.0f));
    } else if (hasPrefabOverride) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.70f, 0.25f, 1.0f));
    } else if (isPrefabMember) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.75f, 1.0f, 1.0f));
    }
    // 色だけに頼らず判別できるよう、Overrideがある場合はラベル末尾に" *"を付与する
    const std::string label = hasPrefabOverride ? (item.name + " *") : item.name;
    const bool isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    if (isInactive || isEditorOnly || hasPrefabOverride || isPrefabMember) {
        ImGui::PopStyleColor();
    }
    // シーンビュー等からの選択でスクロール対象そのものの場合、この行が画面中央に来るようスクロールする
    if (pendingScrollToObject_ == item.object) {
        ImGui::SetScrollHereY(0.5f);
    }
    // Commentコンポーネントが付いている場合、カーソルを合わせた際にその内容をツールチップ表示する
    if (auto *comment = item.object->GetComponent<Comment>(); comment && !comment->GetComment().empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", comment->GetComment().c_str());
    }
    // forceOpenは一時的な表示上の強制展開のため、ユーザーが手動で折り畳んでいた保存済み設定は書き換えない
    if (!item.children.empty() && !forceOpen && isOpen != storedOpen) {
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
    const bool shiftHeld = ImGui::IsKeyDown(ImGuiMod_Shift);
    const bool ctrlHeld = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    // ImGuiのTreeNode/TreeNodeExは、Ctrl/Shiftキーを押しながらのクリックを開閉アイコン部分以外では
    // 内部的に無視する仕様になっている（複数選択パターンを実装するアプリ向けに、矢印以外のクリックは
    // 常にキー修飾を許可しない設計。arrow_hit_x1/x2の狭い範囲でしかIsItemDeactivated()が発火しない）。
    // そのため、Ctrl/Shift押下時のみ行全体で反応するよう、IsItemHovered()（キー修飾に関係なく機能する）と
    // マウスリリースの組み合わせで直接検知する
    const bool clickedThisItem = (shiftHeld || ctrlHeld)
        ? (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        : ImGui::IsItemDeactivated();
    if (clickedThisItem) {
        const ImGuiIO &io = ImGui::GetIO();
        const float dragThreshold = io.MouseDragThreshold;
        const bool wasDragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > (dragThreshold * dragThreshold);
        if (!wasDragged) {
            if (shiftHeld) {
                // Shift+クリック: 範囲選択。対象の並び順はツリー全体を描画し終えないと確定しないため、
                // ここでは要求のみ記録し、ApplyPendingRangeSelect() で確定させる。
                pendingRangeTarget_ = item.object;
            } else if (ctrlHeld) {
                // Ctrl+クリック: 個別に選択/選択解除をトグルする
                if (selectedObjects_.contains(item.object)) {
                    RemoveFromSelectionSet(item.object);
                    if (selectedObject_ == item.object) {
                        SetSelectedObject(selectedObjects_.empty() ? nullptr : *selectedObjects_.begin());
                    }
                } else {
                    AddToSelectionSet(item.object);
                    SetSelectedObject(item.object);
                }
                SetSelectionAnchor(item.object);
            } else {
                // 修飾キー無しのクリックは単一選択に置き換える
                ClearSelectionSet();
                AddToSelectionSet(item.object);
                SetSelectedObject(item.object);
                SetSelectionAnchor(item.object);
            }
        }
    }

    if (!item.children.empty() && isOpen) {
        for (const auto &child : item.children) {
            ShowObjectItem(child, ++index);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneObjectHierarchy::ShowObjectContextMenu(EmptyObject *obj) {
    if (ImGui::BeginPopupContextItem("ObjectContextMenu")) {
        // 右クリックしたオブジェクトが複数選択に含まれる場合、Copy/Clone/Deleteは選択中の全オブジェクトを対象にする
        // （Create/Pasteはあくまで右クリックした1点を基準にした挿入操作のため対象外）
        const std::vector<EmptyObject *> targets =
            (selectedObjects_.size() > 1 && selectedObjects_.contains(obj)) ? GetSelectionRoots() : std::vector<EmptyObject *>{ obj };

        if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.createobject"))) {
            // 右クリックしたオブジェクトと同じ階層かつ次のインデックス位置に作成する
            ShowCreateObjectMenu(obj, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.createchildobject"))) {
            // 右クリックしたオブジェクトの子オブジェクトとして最後尾に作成する
            ShowCreateObjectMenu(obj, true);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        const std::string copyLabel = (targets.size() > 1) ? (Translation("editor.hierarchy.copy.multiple.prefix") + std::to_string(targets.size()) + Translation("editor.hierarchy.copy.multiple.suffix")) : Translation("editor.hierarchy.copy");
        if (ImGui::MenuItem(copyLabel.c_str(), "Ctrl+C")) {
            CopyObjects(targets);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.paste"), "Ctrl+V", false, !clipboardNodes_.empty())) {
            auto *transform = obj->GetComponent<Transform>();
            EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
            const size_t index = editorContext_->GetObjectIndex(obj);
            const size_t insertIndex = (index == MAXSIZE_T) ? MAXSIZE_T : index + 1;
            PasteObject(parent, insertIndex);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.pastetochild"), "Ctrl+Shift+V", false, !clipboardNodes_.empty())) {
            PasteObject(obj, MAXSIZE_T);
        }
        const std::string cloneLabel = (targets.size() > 1) ? (Translation("editor.hierarchy.clone.multiple.prefix") + std::to_string(targets.size()) + Translation("editor.hierarchy.clone.multiple.suffix")) : Translation("editor.hierarchy.clone");
        if (ImGui::MenuItem(cloneLabel.c_str(), "Ctrl+D")) {
            CloneObjects(targets);
        }
        ImGui::Separator();
        const std::string deleteLabel = (targets.size() > 1) ? (Translation("editor.hierarchy.delete.multiple.prefix") + std::to_string(targets.size()) + Translation("editor.hierarchy.delete.multiple.suffix")) : Translation("editor.hierarchy.delete");
        if (ImGui::MenuItem(deleteLabel.c_str())) {
            DeleteObjects(targets);
        }

        // Prefabインスタンスのルート自身に対してのみ、Apply All/Revert Allを出す（Unity同様、途中階層には出さない）
        if (PrefabSyncUtility::FindEnclosingPrefabInstanceRoot(obj) == obj) {
            ImGui::Separator();
            if (ImGui::MenuItem(TranslationLabel("editor.prefab.applyall"))) {
                PrefabSyncUtility::ApplyAll(editorContext_, obj);
            }
            if (ImGui::MenuItem(TranslationLabel("editor.prefab.revertall"))) {
                SetPendingRevertPrefabTarget(obj);
                isRevertPrefabConfirmRequested_ = true;
            }
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowRevertPrefabConfirmModal() {
    if (isRevertPrefabConfirmRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.prefab.revert.title"));
        isRevertPrefabConfirmRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.prefab.revert.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s",
            TranslationC("editor.prefab.revert.warning1"));
        ImGui::TextUnformatted(TranslationC("editor.prefab.revert.warning2"));
        ImGui::TextUnformatted(TranslationC("editor.prefab.revert.warning3"));
        if (ImGui::Button(TranslationLabel("editor.prefab.revert"), ImVec2(120, 0))) {
            if (pendingRevertPrefabTarget_) {
                PrefabSyncUtility::RevertAll(editorContext_, commands_, pendingRevertPrefabTarget_);
            }
            SetPendingRevertPrefabTarget(nullptr);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            SetPendingRevertPrefabTarget(nullptr);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowHierarchyContextMenu() {
    // ImGuiPopupFlags_NoOpenOverItems を指定しないと、オブジェクト項目上での右クリックでも
    // この window レベルのメニューが同一フレームで開いてしまい、
    // オブジェクト自体の ObjectContextMenu を閉じてしまう（表示されないように見える）ため必須。
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.createobject"))) {
            ShowCreateObjectMenu(nullptr, false);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.paste"), "Ctrl+V", false, !clipboardNodes_.empty())) {
            PasteObject(nullptr, MAXSIZE_T);
        }
        // エディターツールスクリプトの[MenuItem("Hierarchy/...")]で追加された項目
        EditorToolManager::GetInstance().ShowHierarchyMenuItems();
        ImGui::EndPopup();
    }
}

void SceneObjectHierarchy::ShowCreateObjectMenu(EmptyObject *referenceObject, bool asChild) {
    if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.emptyobject"))) {
        CreateTemplateObject("EmptyObject", {}, referenceObject, asChild);
    }
    ImGui::Separator();
    if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.create.category.3d"))) {
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.meshobject"))) {
            CreateTemplateObject("Mesh Object", { "MeshFilter", "MeshRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.cameraobject"))) {
            CreateTemplateObject("Camera Object", { "Camera3D", "CameraRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.lightobject"))) {
            CreateTemplateObject("Light Object", { "Light", "LightRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.skinnedmeshobject"))) {
            CreateTemplateObject("Skinned Mesh Object", { "MeshFilter", "SkinnedMeshRenderer" }, referenceObject, asChild);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.create.category.2d"))) {
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.spriteobject"))) {
            CreateTemplateObject("Sprite Object", { "MeshFilter", "SpriteRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.textobject"))) {
            CreateTemplateObject("Text Object", { "TextRenderer" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.camera2dobject"))) {
            CreateTemplateObject("Camera 2D Object", { "Camera2D", "CameraRenderer" }, referenceObject, asChild);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(TranslationLabel("editor.hierarchy.create.category.rendertarget"))) {
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.windowobject"))) {
            CreateTemplateObject("Window Object", { "NormalWindowObject" }, referenceObject, asChild);
        }
        if (ImGui::MenuItem(TranslationLabel("editor.hierarchy.create.screenbufferobject"))) {
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
        auto composite = std::make_unique<CompositeCommand>(Translation("editor.command.create") + objectName);
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

    // Delete: 選択中オブジェクトを削除する（テキスト入力中の誤爆を避けるためIsAnyItemActive時は無視する）
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::IsAnyItemActive()) {
        if (!selectedObjects_.empty()) DeleteObjects(GetSelectionRoots());
        return;
    }

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
            auto composite = std::make_unique<CompositeCommand>(Translation("editor.command.deleteobjects.prefix") + std::to_string(objs.size()) + Translation("editor.command.deleteobjects.suffix"));
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
    PrefabUtility::CollectSubtreeNodes(editorContext_, obj, parentIndex, out);
}

void SceneObjectHierarchy::InstantiateNodes(const std::vector<PasteObjectCommand::Node> &nodes, const std::string &name,
    EmptyObject *attachParent, const Vector3 *worldPosition) {
    if (nodes.empty() || !editorContext_) return;
    auto prepared = PrepareNodesForInstantiation(nodes, /*preserveRootParent=*/false);
    if (worldPosition) {
        PrefabUtility::OffsetRootsToWorldPosition(prepared, *worldPosition);
    }
    ExecutePasteCommand(std::make_unique<PasteObjectCommand>(
        std::move(prepared), attachParent, MAXSIZE_T, name, "Instantiate Prefab"));
}

bool SceneObjectHierarchy::InstantiatePrefabFile(const std::string &filePath, EmptyObject *attachParent, const Vector3 *worldPosition) {
    if (filePath.empty() || !editorContext_) return false;
    const JSON prefabJson = LoadJSON(ProjectPaths::ToPhysical(filePath));
    if (!prefabJson.is_object()) {
        Log(Translation("engine.prefab.instantiate.load.failed") + filePath, LogSeverity::Warning);
        return false;
    }
    auto nodes = PrefabUtility::LoadPrefabNodes(prefabJson);
    if (nodes.empty()) {
        Log(Translation("engine.prefab.instantiate.nodes.empty") + filePath, LogSeverity::Warning);
        return false;
    }
    const std::string prefabName = prefabJson.value("name",
        std::filesystem::path(filePath).stem().string());
    InstantiateNodes(nodes, prefabName, attachParent, worldPosition);

    // 配置直後のルート（ExecutePasteCommandが選択状態にする）へPrefabInstanceComponentを付与し、
    // 元Prefabとのリンクを持たせる（Prefabファイル自体にはリンク情報を含めない設計のため、
    // 配置時にここで明示的に付与しないと同期対象にならない）
    const UUID128 prefabID = PrefabAssetManager::GetPrefabIDFromPath(filePath);
    if (prefabID.IsValid()) {
        for (auto *root : selectedObjects_) {
            if (commands_) {
                commands_->Execute(std::make_unique<AddComponentCommand>(root, "PrefabInstanceComponent"));
            } else {
                root->AddComponent(CreateObjectComponentByType("PrefabInstanceComponent"));
            }
            if (auto *comp = root->GetComponent<PrefabInstanceComponent>()) {
                comp->SetPrefabID(prefabID);
            }
        }
    }
    return true;
}

void SceneObjectHierarchy::ExecutePasteCommand(std::unique_ptr<PasteObjectCommand> command) {
    if (!command || !editorContext_) return;
    PasteObjectCommand *rawCommand = command.get();
    const bool succeeded = commands_ ? commands_->Execute(std::move(command)) : rawCommand->Execute(editorContext_);
    if (!succeeded) return;

    auto newRoots = rawCommand->GetRootObjects(editorContext_);
    if (!newRoots.empty()) {
        ClearSelectionSet();
        for (auto *obj : newRoots) AddToSelectionSet(obj);
        SetSelectedObject(newRoots.back());
        SetSelectionAnchor(newRoots.back());
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
        ClearSelectionSet();
        AddToSelectionSet(target);
        SetSelectedObject(target);
        SetSelectionAnchor(target);
        return;
    }

    size_t anchorIndex = static_cast<size_t>(std::distance(visibleOrderThisFrame_.begin(), anchorIt));
    size_t targetIndex = static_cast<size_t>(std::distance(visibleOrderThisFrame_.begin(), targetIt));
    if (anchorIndex > targetIndex) std::swap(anchorIndex, targetIndex);

    ClearSelectionSet();
    for (size_t i = anchorIndex; i <= targetIndex; ++i) {
        AddToSelectionSet(visibleOrderThisFrame_[i]);
    }
    SetSelectedObject(target);
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
        // Assetsウィンドウからのプレハブファイルのドロップも受け付ける（対象オブジェクトの子として配置する）。
        // ツリーの走査中にシーンへオブジェクトを追加しないよう、要求のみ記録してShowImGuiの最後で処理する
        if (const ImGuiPayload *prefabPayload = ImGui::AcceptDragDropPayload(kPrefabAssetDragDropType)) {
            IM_ASSERT(prefabPayload->DataSize == sizeof(AssetDragDropPayload));
            const auto *assetPayload = static_cast<const AssetDragDropPayload *>(prefabPayload->Data);
            pendingPrefabDropPath_ = assetPayload->assetPath;
            pendingPrefabDropParent_ = objItem->object;
            hasPendingPrefabDrop_ = true;
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
            auto composite = std::make_unique<CompositeCommand>(Translation("editor.command.moveobject.named") + sourceObject->GetName());
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
