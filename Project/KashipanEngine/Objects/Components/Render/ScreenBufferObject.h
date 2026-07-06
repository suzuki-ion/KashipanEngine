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
        ptr->name_ = name_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->isShowViewer_ = isShowViewer_;
        return ptr;
    }

    ScreenBuffer *GetScreenBuffer() const noexcept { return buffer_; }
    /// @brief 管理用の名前を設定（TextureManagerへの登録名になる）
    void SetName(const std::string &name) {
        name_ = name;
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->SetRenderTargetName(name_);
        }
    }
    const std::string &GetName() const noexcept { return name_; }
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
    }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ScreenBuffer::Create(width_, height_, name_);
        // 自動生成された名前を保持する（保存時に確定した名前が残るようにする）
        if (buffer_ && name_.empty()) {
            name_ = buffer_->GetRenderTargetName();
        }
    }
    void Finalize() override {
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->DestroyNotify();
        }
        buffer_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGui::InputText("Name", &name_, ImGuiInputTextFlags_EnterReturnsTrue)) {
            SetName(name_);
        }
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGuiCustom::EditValue("Width", w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGuiCustom::EditValue("Height", h)) height_ = static_cast<std::uint32_t>(std::max(1, h));

        // 描画内容確認用ビューアウィンドウ
        if (ImGui::Button(isShowViewer_ ? "Close Viewer" : "Open Viewer")) {
            isShowViewer_ = !isShowViewer_;
        }
    }

    /// @brief 描画内容確認用のImGuiウィンドウ表示（ポーズ中も表示し続ける）
    void ShowPersistentImGui() override {
        ShowViewerWindow();
    }

    /// @brief 描画内容確認用のImGuiウィンドウ表示
    void ShowViewerWindow() {
        if (!isShowViewer_) return;
        if (!buffer_ || !ScreenBuffer::IsExist(buffer_)) return;

        const std::string windowTitle = "ScreenBuffer Viewer: " + name_;
        if (ImGui::Begin(windowTitle.c_str(), &isShowViewer_)) {
            ImGui::Text("Size: %ux%u", buffer_->GetWidth(), buffer_->GetHeight());

            const auto srvHandle = buffer_->GetSrvHandle();
            if (srvHandle.ptr != 0) {
                // アスペクト比を維持して表示領域にフィットさせる
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const float w = static_cast<float>(buffer_->GetWidth());
                const float h = static_cast<float>(buffer_->GetHeight());
                ImVec2 drawSize = avail;
                if (w > 0.0f && h > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
                    const float scale = std::min(avail.x / w, avail.y / h);
                    drawSize = ImVec2(w * scale, h * scale);
                }
                ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
            } else {
                ImGui::TextUnformatted("SRV not ready.");
            }
        }
        ImGui::End();
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["name"] = name_;
        json["width"] = width_;
        json["height"] = height_;
        json["isShowViewer"] = isShowViewer_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        name_ = json.value("name", std::string{});
        width_ = json.value("width", 1280u);
        height_ = json.value("height", 720u);
        isShowViewer_ = json.value("isShowViewer", false);
        return true;
    }

private:
    ScreenBuffer *buffer_ = nullptr;
    std::string name_;
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
    /// @brief ビューアウィンドウ表示フラグ（シリアライズされ、再起動後も維持される）
    bool isShowViewer_ = false;
};

REGISTER_COMPONENT_OBJECT(ScreenBufferObject);

} // namespace KashipanEngine
