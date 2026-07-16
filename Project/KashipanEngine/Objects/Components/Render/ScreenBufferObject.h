#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Graphics/ScreenBuffer.h"
#include "Scene/RenderTargetCarryOverRegistry.h"

namespace KashipanEngine {

class ScreenBufferObject final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ScreenBufferObject, 0xFF, )
    COMPONENT_CATEGORY("Render", "RenderTarget")
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
    /// @brief バッファサイズを変更する（既存のバッファがあれば実際にリサイズされる）
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->Resize(width_, height_);
        }
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
        // シーン切り替え中は即座に破棄せず、次のシーンの同名コンポーネントへの
        // 引き継ぎ候補として預ける（切り替え中でなければ登録側が即座に破棄する）
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            RenderTargetCarryOverRegistry::Deposit(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, name_, buffer_,
                [b = buffer_] { if (ScreenBuffer::IsExist(b)) b->DestroyNotify(); });
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
        bool sizeChanged = false;
        if (ImGuiCustom::EditValue("Width", w)) { w = std::max(1, w); sizeChanged = true; }
        if (ImGuiCustom::EditValue("Height", h)) { h = std::max(1, h); sizeChanged = true; }
        if (sizeChanged) SetSize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));

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
        const std::string loadedName = json.value("name", std::string{});
        const std::uint32_t loadedWidth = json.value("width", 1280u);
        const std::uint32_t loadedHeight = json.value("height", 720u);
        isShowViewer_ = json.value("isShowViewer", false);

        // シーン切り替え中、前のシーンで同名のScreenBufferObjectが使用していたバッファが
        // 引き継ぎプールに残っていれば、新規作成せずそちらを再利用する
        if (!loadedName.empty()) {
            if (auto *carried = static_cast<ScreenBuffer *>(
                    RenderTargetCarryOverRegistry::Claim(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, loadedName))) {
                if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
                    buffer_->DestroyNotify();
                }
                buffer_ = carried;
                if (buffer_->GetWidth() != loadedWidth || buffer_->GetHeight() != loadedHeight) {
                    buffer_->Resize(loadedWidth, loadedHeight);
                }
                name_ = buffer_->GetRenderTargetName();
                width_ = loadedWidth;
                height_ = loadedHeight;
                return true;
            }
        }

        // AddComponent() の時点で Initialize() が先に呼ばれ、
        // この関数が読み込むより前にデフォルト値（空の名前・既定サイズ）で
        // バッファが作成・登録されてしまっている。
        // ここで単に name_/width_/height_ を上書きするだけだと、
        // 実際に TextureManager へ登録されている名前（自動生成された別名）と
        // 表示上の名前が食い違ったままになり、その名前を参照するマテリアル等が
        // テクスチャを見つけられなくなるため、バッファ側も正しい状態へ同期し直す。
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            if (buffer_->GetWidth() != loadedWidth || buffer_->GetHeight() != loadedHeight) {
                buffer_->DestroyNotify();
                buffer_ = ScreenBuffer::Create(loadedWidth, loadedHeight, loadedName);
                name_ = buffer_ ? buffer_->GetRenderTargetName() : loadedName;
            } else {
                name_ = loadedName;
                if (!name_.empty()) {
                    buffer_->SetRenderTargetName(name_);
                }
                name_ = buffer_->GetRenderTargetName();
            }
        } else {
            name_ = loadedName;
        }
        width_ = loadedWidth;
        height_ = loadedHeight;
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
