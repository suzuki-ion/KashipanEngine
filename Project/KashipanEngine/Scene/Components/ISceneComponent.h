#pragma once

#include <cassert>
#include <string>

namespace KashipanEngine {

class SceneContext;
class SceneBase;

/// @brief シーンコンポーネントインターフェースクラス
class ISceneComponent {
public:
    virtual ~ISceneComponent() = default;

    ISceneComponent(const ISceneComponent &) = delete;
    ISceneComponent &operator=(const ISceneComponent &) = delete;
    ISceneComponent(ISceneComponent &&) = delete;
    ISceneComponent &operator=(ISceneComponent &&) = delete;

    const std::string &GetComponentType() const { return kComponentType_; }
    size_t GetMaxComponentCountPerScene() const { return kMaxComponentCountPerScene_; }

    virtual void Initialize() {}
    virtual void Finalize() {}

    virtual void Update() {}

    int GetUpdatePriority() const { return updatePriority_; }
    void SetUpdatePriority(int priority) { updatePriority_ = priority; }

    void SetOwnerContext(SceneContext *context) {
        if (!context) assert(false && "Owner context cannot be null.");
        ownerContext_ = context;
    }

#if defined(USE_IMGUI)
    virtual void ShowImGui() {}
#endif

protected:
    ISceneComponent(const std::string &componentType, size_t maxComponentCountPerScene = 0xFF)
        : kComponentType_(componentType), kMaxComponentCountPerScene_(maxComponentCountPerScene) {}

    SceneContext *GetOwnerContext() const { return ownerContext_; }

    SceneBase *GetOwnerScene() const;

private:
    const std::string kComponentType_;
    const size_t kMaxComponentCountPerScene_ = 0xFF;

    int updatePriority_ = 1;

    SceneContext *ownerContext_ = nullptr;
};

} // namespace KashipanEngine