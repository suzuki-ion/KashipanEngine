#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class AudioSource final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(AudioSource, 0xFF, )
    COMPONENT_CATEGORY("Audio")
    ~AudioSource() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AudioSource>();
        ptr->soundName_ = soundName_;
        ptr->volume_ = volume_;
        ptr->loop_ = loop_;
        return ptr;
    }
protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { ImGui::InputText("Sound", &soundName_); ImGui::DragFloat("Volume", &volume_, 0.01f, 0.0f, 1.0f); ImGui::Checkbox("Loop", &loop_); }
#endif
    JSON SaveToJson() const override { return JSON{ {"soundName", soundName_}, {"volume", volume_}, {"loop", loop_} }; }
    bool LoadFromJson(const JSON &json) override { soundName_ = json.value("soundName", std::string{}); volume_ = json.value("volume", 1.0f); loop_ = json.value("loop", false); return true; }
private:
    std::string soundName_;
    float volume_ = 1.0f;
    bool loop_ = false;
};

REGISTER_COMPONENT_OBJECT(AudioSource)

} // namespace KashipanEngine
