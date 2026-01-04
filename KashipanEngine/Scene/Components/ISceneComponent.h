#pragma once

namespace KashipanEngine {

class SceneBase;

class ISceneComponent {
public:
    virtual ~ISceneComponent() = default;

    ISceneComponent(const ISceneComponent &) = delete;
    ISceneComponent &operator=(const ISceneComponent &) = delete;
    ISceneComponent(ISceneComponent &&) = delete;
    ISceneComponent &operator=(ISceneComponent &&) = delete;

    virtual void Initialize() {}
    virtual void Finalize() {}

    virtual void Update() {}

protected:
    ISceneComponent() = default;

    SceneBase *GetOwnerScene() const { return ownerScene_; }

private:
    friend class SceneBase;
    void SetOwnerScene(SceneBase *owner) { ownerScene_ = owner; }

    SceneBase *ownerScene_ = nullptr;
};

} // namespace KashipanEngine