#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class Animator final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Animator, 1, )
    COMPONENT_CATEGORY("Animation")
    ~Animator() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Animator>();
        ptr->animationName_ = animationName_;
        ptr->playOnStart_ = playOnStart_;
        return ptr;
    }
    void SetAnimationName(const std::string &name) { animationName_ = name; }
    const std::string &GetAnimationName() const noexcept { return animationName_; }
    void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
    bool GetPlayOnStart() const noexcept { return playOnStart_; }
protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { ImGui::InputText("Animation", &animationName_); ImGui::Checkbox("PlayOnStart", &playOnStart_); }
#endif
    JSON SaveToJson() const override { return JSON{ {"animationName", animationName_}, {"playOnStart", playOnStart_} }; }
    bool LoadFromJson(const JSON &json) override { animationName_ = json.value("animationName", std::string{}); playOnStart_ = json.value("playOnStart", true); return true; }
private:
    std::string animationName_;
    bool playOnStart_ = true;
};

REGISTER_COMPONENT_OBJECT(Animator)

} // namespace KashipanEngine
