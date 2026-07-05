#pragma once
#include <algorithm>
#include <cstdint>

#include "Objects/ObjectComponentHeader.h"
#include "Graphics/ShadowMapBuffer.h"

namespace KashipanEngine {

/// @brief シャドウマップバッファを描画先として表すコンポーネント
class ShadowMapObject final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ShadowMapObject, 0xFF, )
    ~ShadowMapObject() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ShadowMapObject>();
        ptr->width_ = width_;
        ptr->height_ = height_;
        return ptr;
    }

    ShadowMapBuffer *GetShadowMapBuffer() const noexcept { return buffer_; }
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
    }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ShadowMapBuffer::Create(width_, height_);
    }
    void Finalize() override {
        if (buffer_ && ShadowMapBuffer::IsExist(buffer_)) {
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
        width_ = json.value("width", 2048u);
        height_ = json.value("height", 2048u);
        return true;
    }

private:
    ShadowMapBuffer *buffer_ = nullptr;
    std::uint32_t width_ = 2048;
    std::uint32_t height_ = 2048;
};

REGISTER_COMPONENT_OBJECT(ShadowMapObject);

} // namespace KashipanEngine
