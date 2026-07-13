#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Text final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Text, 1, )
    COMPONENT_CATEGORY("Render")
    ~Text() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Text>();
        ptr->text_ = text_;
        ptr->color_ = color_;
        return ptr;
    }

    void SetText(const std::string &text) { text_ = text; }
    const std::string &GetText() const noexcept { return text_; }
    void SetColor(const Vector4 &color) { color_ = color; }
    const Vector4 &GetColor() const noexcept { return color_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override { ImGui::InputTextMultiline("Text", &text_); ImGui::ColorEdit4("Color", &color_.x); }
#endif
    JSON SaveToJson() const override { return JSON{ {"text", text_}, {"color", ToJSON(color_)} }; }
    bool LoadFromJson(const JSON &json) override { text_ = json.value("text", std::string{}); if (json.contains("color")) color_ = FromJSON<Vector4>(json["color"]); return true; }
private:
    std::string text_;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
};

REGISTER_COMPONENT_OBJECT(Text)

} // namespace KashipanEngine
