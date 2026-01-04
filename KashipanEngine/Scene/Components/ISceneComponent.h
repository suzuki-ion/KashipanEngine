#pragma once

namespace KashipanEngine {

class Scene;

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

    Scene *GetOwnerScene() const { return ownerScene_; }

private:
    friend class Scene;
    void SetOwnerScene(Scene *owner) { ownerScene_ = owner; }

    Scene *ownerScene_ = nullptr;
};

} // namespace KashipanEngine