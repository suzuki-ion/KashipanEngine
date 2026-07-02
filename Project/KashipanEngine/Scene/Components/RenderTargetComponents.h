#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Core/Window.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class WindowComponent final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(WindowComponent, 0xFF, )
    ~WindowComponent() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        auto ptr = std::make_unique<WindowComponent>();
        ptr->title_ = title_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->windowType_ = windowType_;
        return ptr;
    }

    Window *GetWindow() const noexcept { return window_; }
    void SetTitle(const std::string &title) { title_ = title; }
    void SetSize(std::uint32_t width, std::uint32_t height) { width_ = width; height_ = height; }
    void SetWindowType(WindowType type) { windowType_ = type; }

protected:
    void Initialize() override {
        if (window_) return;
        if (windowType_ == WindowType::Overlay) {
            window_ = Window::CreateOverlay(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_));
        } else {
            window_ = Window::CreateNormal(title_, static_cast<int32_t>(width_), static_cast<int32_t>(height_));
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
        ImGui::InputText("Title", &title_);
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGui::InputInt("Width", &w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGui::InputInt("Height", &h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
        int type = static_cast<int>(windowType_);
        const char *items[] = { "Normal", "Overlay" };
        if (ImGui::Combo("Type", &type, items, 2)) windowType_ = static_cast<WindowType>(type);
        ImGui::Text("Window: %p", static_cast<void *>(window_));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["title"] = title_;
        json["width"] = width_;
        json["height"] = height_;
        json["windowType"] = static_cast<int>(windowType_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        title_ = json.value("title", std::string{ "Window" });
        width_ = json.value("width", 1280u);
        height_ = json.value("height", 720u);
        windowType_ = static_cast<WindowType>(json.value("windowType", static_cast<int>(WindowType::Normal)));
        return true;
    }

private:
    Window *window_ = nullptr;
    std::string title_ = "Window";
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
    WindowType windowType_ = WindowType::Normal;
};

class ScreenBufferComponent final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(ScreenBufferComponent, 0xFF, )
    ~ScreenBufferComponent() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenBufferComponent>();
        ptr->name_ = name_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->attachToRenderer_ = attachToRenderer_;
        ptr->passName_ = passName_;
        return ptr;
    }

    ScreenBuffer *GetScreenBuffer() const noexcept { return buffer_; }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ScreenBuffer::Create(width_, height_);
        if (!buffer_) return;
        buffer_->SetRenderTargetName(name_);
        if (attachToRenderer_) {
            buffer_->AttachToRenderer(passName_);
        }
    }

    void Finalize() override {
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->DetachFromRenderer();
            ScreenBuffer::DestroyNotify(buffer_);
        }
        buffer_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::InputText("Name", &name_);
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGui::InputInt("Width", &w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGui::InputInt("Height", &h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
        ImGui::Checkbox("Attach", &attachToRenderer_);
        ImGui::InputText("PassName", &passName_);
        ImGui::Text("ScreenBuffer: %p", static_cast<void *>(buffer_));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["name"] = name_;
        json["width"] = width_;
        json["height"] = height_;
        json["attachToRenderer"] = attachToRenderer_;
        json["passName"] = passName_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        name_ = json.value("name", std::string{});
        width_ = json.value("width", 1920u);
        height_ = json.value("height", 1080u);
        attachToRenderer_ = json.value("attachToRenderer", false);
        passName_ = json.value("passName", name_);
        return true;
    }

private:
    ScreenBuffer *buffer_ = nullptr;
    std::string name_;
    std::uint32_t width_ = 1920;
    std::uint32_t height_ = 1080;
    bool attachToRenderer_ = false;
    std::string passName_;
};

class ShadowMapBufferComponent final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(ShadowMapBufferComponent, 0xFF, )
    ~ShadowMapBufferComponent() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        auto ptr = std::make_unique<ShadowMapBufferComponent>();
        ptr->name_ = name_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        return ptr;
    }

    ShadowMapBuffer *GetShadowMapBuffer() const noexcept { return buffer_; }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ShadowMapBuffer::Create(width_, height_);
        if (buffer_) {
            buffer_->SetRenderTargetName(name_);
        }
    }

    void Finalize() override {
        if (buffer_ && ShadowMapBuffer::IsExist(buffer_)) {
            ShadowMapBuffer::DestroyNotify(buffer_);
        }
        buffer_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::InputText("Name", &name_);
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        if (ImGui::InputInt("Width", &w)) width_ = static_cast<std::uint32_t>(std::max(1, w));
        if (ImGui::InputInt("Height", &h)) height_ = static_cast<std::uint32_t>(std::max(1, h));
        ImGui::Text("ShadowMapBuffer: %p", static_cast<void *>(buffer_));
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["name"] = name_;
        json["width"] = width_;
        json["height"] = height_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        name_ = json.value("name", std::string{});
        width_ = json.value("width", 2048u);
        height_ = json.value("height", 2048u);
        return true;
    }

private:
    ShadowMapBuffer *buffer_ = nullptr;
    std::string name_;
    std::uint32_t width_ = 2048;
    std::uint32_t height_ = 2048;
};

REGISTER_COMPONENT_SCENE(WindowComponent)
REGISTER_COMPONENT_SCENE(ScreenBufferComponent)
REGISTER_COMPONENT_SCENE(ShadowMapBufferComponent)

} // namespace KashipanEngine
