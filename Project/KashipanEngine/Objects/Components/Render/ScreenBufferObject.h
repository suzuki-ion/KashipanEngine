#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Graphics/ScreenBuffer.h"

namespace KashipanEngine {

class ScreenBufferObject final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ScreenBufferObject, 0xFF, )
        ~ScreenBufferObject() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenBufferObject>();
        ptr->width_ = width_;
        ptr->height_ = height_;
        return ptr;
    }

    ScreenBuffer *GetScreenBuffer() const noexcept { return buffer_; }
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
    }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ScreenBuffer::Create(width_, height_);
    }
    void Finalize() override {
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->DestroyNotify();
        }
        buffer_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGuiCustom::EditValue("Width", w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGuiCustom::EditValue("Height", h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["width"] = width_;
        json["height"] = height_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        width_ = json.value("width", 1280u);
        height_ = json.value("height", 720u);
        return true;
    }

private:
    ScreenBuffer *buffer_ = nullptr;
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
};

REGISTER_COMPONENT_OBJECT(ScreenBufferObject);

} // namespace KashipanEngine