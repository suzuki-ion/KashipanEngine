#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Core/Window.h"

namespace KashipanEngine {

class NormalWindowObject final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(NormalWindowObject, 0xFF, )
    COMPONENT_CATEGORY("Render", "RenderTarget")
        ~NormalWindowObject() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<NormalWindowObject>();
        ptr->title_ = title_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        return ptr;
    }

    Window *GetWindow() const noexcept { return window_; }
    void SetTitle(const std::string &title) {
        if (window_ && Window::IsExist(window_)) window_->SetWindowTitle(title);
        title_ = title;
    }
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
    }

protected:
    void Initialize() override {
        if (window_) return;
        window_ = Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_));
        // 位置やサイズの適用のために一回Updateを呼ぶ
        Update();
    }
    void Update() override {
        if (!window_ || !Window::IsExist(window_)) return;
        auto *tr = GetOwnerObjectContext()->GetComponent<Transform>();
        if (!tr) return;
        // ワールド行列から位置とスケールを取得してウィンドウに反映
        auto worldMatrix = tr->GetWorldMatrix();
        Vector3 pos(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
        Vector3 scale(
            Vector3(worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]).Length(), 
            Vector3(worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]).Length(),
            Vector3(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]).Length()
        );
        int32_t posX = static_cast<int32_t>(pos.x);
        int32_t posY = static_cast<int32_t>(pos.y);
        int32_t width = static_cast<int32_t>(width_) * static_cast<int32_t>(scale.x);
        int32_t height = static_cast<int32_t>(height_) * static_cast<int32_t>(scale.y);
        if (width < 1) width = 1;
        if (height < 1) height = 1;
        if (window_->GetWindowPosition().x != posX || window_->GetWindowPosition().y != posY) {
            window_->SetWindowPosition(posX, posY);
        }
        if (window_->GetClientWidth() != width || window_->GetClientHeight() != height) {
            window_->SetWindowSize(width, height);
        }
    }
    void Finalize() override {
        if (window_ && Window::IsExist(window_)) {
            window_->DestroyNotify();
        }
        window_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGuiCustom::EditValue("Title", title_)) SetTitle(title_);
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGuiCustom::EditValue("Width", w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGuiCustom::EditValue("Height", h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
        if (!Window::IsExist(window_)) {
            if (ImGui::Button("Respawn Window")) {
                window_ = nullptr;
                Initialize();
            }
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["title"] = title_;
        json["width"] = width_;
        json["height"] = height_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        title_ = json.value("title", std::string{ "Normal Window" });
        width_ = json.value("width", 1280u);
        height_ = json.value("height", 720u);
        return true;
    }

private:
    Window *window_ = nullptr;
    std::string title_ = "Normal Window";
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
};

REGISTER_COMPONENT_OBJECT(NormalWindowObject);

} // namespace KashipanEngine