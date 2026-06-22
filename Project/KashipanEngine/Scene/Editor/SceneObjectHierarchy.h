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
    Object3DBase *GetSelectedObject3D() const { return selectedObject3D_; }

private:
    template <typename T>
    struct ObjectItem {
        T *object = nullptr;
        std::string name = "";
        std::vector<ObjectItem<T>> children;
        bool isExpanded = true;
        size_t depth = 0;
    };
    using Object2DItem = ObjectItem<Object2DBase>;
    using Object3DItem = ObjectItem<Object3DBase>;

    void RebuildObject2DItems();
    void RebuildObject3DItems();

    void RecursivelyBuildObject2DItems(Object2DBase *obj, Object2DItem &item, size_t depth);
    void RecursivelyBuildObject3DItems(Object3DBase *obj, Object3DItem &item, size_t depth);

    void ShowObject2DItem(const Object2DItem &item, size_t &index);
    void ShowObject3DItem(const Object3DItem &item, size_t &index);

    SceneEditorContext *editorContext_;

    std::vector<Object2DItem> object2DItems_;
    std::vector<Object3DItem> object3DItems_;
    std::unordered_map<Object2DBase *, std::vector<Object2DBase *>> object2DParentMap_;
    std::unordered_map<Object3DBase *, std::vector<Object3DBase *>> object3DParentMap_;
    size_t selectedObject2DIndex_ = SIZE_MAX;
    size_t selectedObject3DIndex_ = SIZE_MAX;
    Object2DBase *selectedObject2D_ = nullptr;
    Object3DBase *selectedObject3D_ = nullptr;
    
    enum class SelectedObjectType {
        None,
        Object2D,
        Object3D
    } selectedObjectType_ = SelectedObjectType::None;
};

} // namespace KashipanEngine

#endif // USE_IMGUI