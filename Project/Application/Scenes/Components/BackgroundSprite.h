#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class BackgroundSprite final : public ISceneComponent {
public:
    BackgroundSprite()
        : ISceneComponent("BackgroundSprite", 1) {}
    ~BackgroundSprite() override = default;

    void Initialize() override {
        auto* ctx = GetOwnerContext();
        if (!ctx) return;

        auto* defaults = ctx ? ctx->GetComponent<SceneDefaultVariables>() : nullptr;
        auto* screenBuffer2D = defaults ? defaults->GetScreenBuffer2D() : nullptr;
        auto* window = defaults ? defaults->GetMainWindow() : Window::GetWindow("Main Window");

        float cx = window ? static_cast<float>(window->GetClientWidth()) * 0.5f : 960.0f;
        float cy = window ? static_cast<float>(window->GetClientHeight()) * 0.5f : 540.0f;

        auto sprite = std::make_unique<Sprite>();
        sprite->SetUniqueBatchKey();
        sprite->SetName("BackgroundSprite");
        sprite->SetPivotPoint(0.5f, 0.5f);

        if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3(cx, cy, 0.0f));
            tr->SetScale(Vector3(2048.0f, 2048.0f, 1.0f));
        }

        if (auto* mat = sprite->GetComponent2D<Material2D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("background.png"));
            Material2D::UVTransform uv{};
            uv.translate = Vector3(0.0f, 0.0f, 0.0f);
            uv.scale = Vector3(1.0f, 1.0f, 1.0f);
            uv.rotate = Vector3(0.0f, 0.0f, 0.0f);
            SamplerManager::SamplerHandle sampler = SamplerManager::GetSampler(DefaultSampler::LinearWrap);
            mat->SetSampler(sampler);
            mat->SetUVTransform(uv);
            backgroundMaterial_ = mat;
        }

        if (screenBuffer2D) {
            sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        } else if (window) {
            sprite->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
        }

        backgroundSprite_ = sprite.get();
        ctx->AddObject2D(std::move(sprite));
    }

    void Update() override {
        if (!backgroundMaterial_) return;

        auto uv = backgroundMaterial_->GetUVTransform();
        uv.translate.y += std::max(0.0f, GetDeltaTime()) * uvScrollSpeed_;
        if (uv.translate.y > 1.0f) {
            uv.translate.y -= 1.0f;
        }
        backgroundMaterial_->SetUVTransform(uv);
    }

    void SetUVScrollSpeed(float speed) { uvScrollSpeed_ = speed; }
    float GetUVScrollSpeed() const { return uvScrollSpeed_; }

private:
    Sprite* backgroundSprite_ = nullptr;
    Material2D* backgroundMaterial_ = nullptr;
    float uvScrollSpeed_ = 0.1f;
};

} // namespace KashipanEngine
