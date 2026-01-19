#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Core/Window.h"

namespace KashipanEngine {

class ScreenBufferKeepRatio final : public ISceneComponent {
public:
    ScreenBufferKeepRatio() : ISceneComponent("ScreenBufferKeepRatio") {}
    ~ScreenBufferKeepRatio() override = default;

    ScreenBufferKeepRatio(const ScreenBufferKeepRatio &) = delete;
    ScreenBufferKeepRatio &operator=(const ScreenBufferKeepRatio &) = delete;

    void Initialize() override {}
    void Finalize() override { sprite_ = nullptr; }

    void Update() override {
        if (!sprite_) return;

        float dstW = targetWidth_;
        float dstH = targetHeight_;

        // ターゲットサイズが指定されていない場合はウィンドウサイズを使用
        if (dstW <= 0.0f || dstH <= 0.0f) {
            if (auto *w = Window::GetWindow("Main Window")) {
                dstW = static_cast<float>(w->GetClientWidth());
                dstH = static_cast<float>(w->GetClientHeight());
            }
        }

        if (dstW <= 0.0f || dstH <= 0.0f) return;
        if (srcWidth_ <= 0.0f || srcHeight_ <= 0.0f) return;

        float drawW = dstW;
        float drawH = dstH;

        const float srcAspect = srcWidth_ / srcHeight_;
        const float dstAspect = dstW / dstH;

        // アスペクト比を比較して、どちらを基準にするか決定
        if (dstAspect > srcAspect) {
            // 高さ基準
            drawH = dstH;
            drawW = drawH * srcAspect;
        } else {
            // 幅基準
            drawW = dstW;
            drawH = drawW / srcAspect;
        }

        if (auto *tr = sprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{ dstW * 0.5f, dstH * 0.5f });
            tr->SetScale(Vector2{ drawW, drawH });
        }
    }

    void SetSprite(Sprite *s) { sprite_ = s; }
    void SetSourceSize(float w, float h) { srcWidth_ = w; srcHeight_ = h; }
    void SetTargetSize(float w, float h) { targetWidth_ = w; targetHeight_ = h; }

private:
    Sprite *sprite_ = nullptr;
    float srcWidth_ = 0.0f;
    float srcHeight_ = 0.0f;
    float targetWidth_ = 0.0f;
    float targetHeight_ = 0.0f;
};

} // namespace KashipanEngine
