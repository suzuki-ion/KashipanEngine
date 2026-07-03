#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <unordered_map>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneObjectHierarchy final {
public:
    SceneObjectHierarchy(Passkey<SceneEditor>, SceneEditorContext *editorContext) : editorContext_(editorContext) {}
    ~SceneObjectHierarchy() = default;
    SceneObjectHierarchy(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy &operator=(const SceneObjectHierarchy &) = delete;
    SceneObjectHierarchy(SceneObjectHierarchy &&) = delete;
    SceneObjectHierarchy &operator=(SceneObjectHierarchy &&) = delete;
    
    void ShowImGui();

    EmptyObject *GetSelectedObject() const { return selectedObject_; }

private:
    struct ObjectItem {
        EmptyObject *object = nullptr;
        std::string name;
        std::vector<ObjectItem> children;
        size_t depth = 0;
        size_t originalIndex = SIZE_MAX;
    };

    enum class DropPosition {
        Above,
        Inside,
        Below
    };

    struct DragDropPayload {
        ObjectItem *objectItemSource = nullptr;
        ObjectItem *objectItemTarget = nullptr;
        DropPosition position = DropPosition::Inside;
    };

    void RebuildObjectItems();
    void RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth);
    void ShowObjectItem(const ObjectItem &item, size_t &index);
    void ShowObjectContextMenu(EmptyObject *obj);
    void ShowHierarchyContextMenu();
    void ShowAddObjectMenu(EmptyObject *parent = nullptr);
    void DragAndDropObject(ObjectItem *objItem);
    void ApplyDragAndDrop();
    DropPosition DragAndDropTargetCommon();

    SceneEditorContext *editorContext_ = nullptr;

    std::vector<ObjectItem> objectItems_;
    std::unordered_map<EmptyObject *, std::vector<std::pair<EmptyObject *, size_t>>> objectParentMap_;
    
    size_t selectedObjectIndex_ = SIZE_MAX;
    EmptyObject *selectedObject_ = nullptr;

    DragDropPayload dragDropPayload_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
