#pragma once
#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class ICollider : public IObjectComponent {
public:
    enum class Shape {
        Box,
        Sphere,
        Capsule,
    };

    Shape GetShape() const noexcept { return shape_; }
    bool IsTrigger() const noexcept { return isTrigger_; }
    void SetTrigger(bool isTrigger) noexcept { isTrigger_ = isTrigger; }

protected:
    ICollider(const std::string &typeName, Shape shape, size_t componentTypeID)
        : IObjectComponent(typeName, 0xFF, componentTypeID), shape_(shape) {}

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Checkbox("IsTrigger", &isTrigger_);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["isTrigger"] = isTrigger_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        isTrigger_ = json.value("isTrigger", false);
        return true;
    }

private:
    Shape shape_;
    bool isTrigger_ = false;
};

} // namespace KashipanEngine
