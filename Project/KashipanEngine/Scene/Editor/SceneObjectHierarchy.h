#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <unordered_map>
#include "Scene/Editor/SceneEditorContext.h"

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

    Object2DBase *GetSelectedObject2D() const { return selectedObject2D_; }
    EmptyObject *GetSelectedObject() const { return selectedObject_; }

private:
    template <typename T>
    struct ObjectItem {
        T *object = nullptr;
        std::string name = "";
        std::vector<ObjectItem<T>> children;
        bool isExpanded = true;
        size_t depth = 0;
        size_t originalIndex = SIZE_MAX;
    };
    using Object2DItem = ObjectItem<Object2DBase>;
    using ObjectItem = ObjectItem<EmptyObject>;

    enum class DropPosition {
        Above,
        Inside,
        Below
    };
    template <typename T>
    struct DragDropPayload {
        T *objectItemSource = nullptr;
        T *objectItemTarget = nullptr;
        DropPosition position = DropPosition::Inside;
    };

    void RebuildObject2DItems();
    void RebuildObjectItems();

    void RecursivelyBuildObject2DItems(Object2DBase *obj, Object2DItem &item, size_t depth);
    void RecursivelyBuildObjectItems(EmptyObject *obj, ObjectItem &item, size_t depth);

    void ShowObject2DItem(const Object2DItem &item, size_t &index);
    void ShowObjectItem(const ObjectItem &item, size_t &index);

    void ShowObject2DContextMenu(Object2DBase *obj);
    void ShowObjectContextMenu(EmptyObject *obj);

    void ShowHierarchyContextMenu();
    void ShowAddObject2DMenu(Object2DBase *parent = nullptr);
    void ShowAddObjectMenu(EmptyObject *parent = nullptr);

    void DragAndDropObject2D(Object2DItem *objItem);
    void DragAndDropObject(ObjectItem *objItem);
    void ApplyDragAndDrop2D();
    void ApplyDragAndDrop3D();
    DropPosition DragAndDropTargetCommon();

    SceneEditorContext *editorContext_;

    std::vector<Object2DItem> object2DItems_;
    std::vector<ObjectItem> object3DItems_;
    std::unordered_map<Object2DBase *, std::vector<std::pair<Object2DBase *, size_t>>> object2DParentMap_;
    std::unordered_map<EmptyObject *, std::vector<std::pair<EmptyObject *, size_t>>> object3DParentMap_;
    
    size_t selectedObject2DIndex_ = SIZE_MAX;
    size_t selectedObjectIndex_ = SIZE_MAX;
    Object2DBase *selectedObject2D_ = nullptr;
    EmptyObject *selectedObject_ = nullptr;

    DragDropPayload<Object2DItem> dragDropPayload2D_;
    DragDropPayload<ObjectItem> dragDropPayload3D_;

    enum class SelectedObjectType {
        None,
        Object2D,
        Object
    } selectedObjectType_ = SelectedObjectType::None;
};

} // namespace KashipanEngine

#endif // USE_IMGUI