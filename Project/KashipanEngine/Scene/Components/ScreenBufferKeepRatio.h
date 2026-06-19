#pragma once

#include "Scene/Components/SceneComponentHeader.h"
#include "Scene/SceneBase.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Core/Window.h"
#include <vector>
#include <algorithm>

namespace KashipanEngine {

class ScreenBufferKeepRatio final : public ISceneComponent {
public:
    ScreenBufferKeepRatio() : ISceneComponent("ScreenBufferKeepRatio") {}
    ~ScreenBufferKeepRatio() override = default;

    ScreenBufferKeepRatio(const ScreenBufferKeepRatio &) = delete;
    ScreenBufferKeepRatio &operator=(const ScreenBufferKeepRatio &) = delete;

    void Initialize() override {}
    void Finalize() override { entries_.clear(); }

    void Update() override {
        if (entries_.empty()) return;

        auto *ctx = GetOwnerContext();
        for (auto &e : entries_) {
            if (e.spriteName.empty()) continue;
            Sprite *s = static_cast<Sprite *>(ctx->GetObject2D(e.spriteName));
            if (!s) continue;

            float dstW = e.targetWidth;
            float dstH = e.targetHeight;

            // ターゲットサイズが指定されていない場合はウィンドウサイズを使用
            if (dstW <= 0.0f || dstH <= 0.0f) {
                if (auto *w = Window::GetWindow("Main Window")) {
                    dstW = static_cast<float>(w->GetClientWidth());
                    dstH = static_cast<float>(w->GetClientHeight());
                }
            }

            if (dstW <= 0.0f || dstH <= 0.0f) continue;
            if (e.srcWidth <= 0.0f || e.srcHeight <= 0.0f) continue;

            float drawW = dstW;
            float drawH = dstH;

            const float srcAspect = e.srcWidth / e.srcHeight;
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

            if (auto *tr = s->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector2{ dstW * 0.5f, dstH * 0.5f });
                tr->SetScale(Vector2{ drawW, drawH });
            }
        }
    }
    
    void AddSprite(const std::string &spriteName, float srcW = 0.0f, float srcH = 0.0f, float targetW = 0.0f, float targetH = 0.0f) {
        if (spriteName.empty()) return;
        auto it = std::find_if(entries_.begin(), entries_.end(), [spriteName](const Entry &en){ return en.spriteName == spriteName; });
        if (it != entries_.end()) return;
        entries_.push_back(Entry{ spriteName, srcW, srcH, targetW, targetH });
    }

    void RemoveSprite(const std::string &spriteName) {
        if (spriteName.empty()) return;
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [spriteName](const Entry &en){ return en.spriteName == spriteName; }), entries_.end());
    }

    void ClearSprites() { entries_.clear(); }

    void SetSourceSize(const std::string &spriteName, float w, float h) {
        if (spriteName.empty()) return;
        auto it = std::find_if(entries_.begin(), entries_.end(), [spriteName](const Entry &en){ return en.spriteName == spriteName; });
        if (it != entries_.end()) { it->srcWidth = w; it->srcHeight = h; }
    }
    void SetTargetSize(const std::string &spriteName, float w, float h) {
        if (spriteName.empty()) return;
        auto it = std::find_if(entries_.begin(), entries_.end(), [spriteName](const Entry &en){ return en.spriteName == spriteName; });
        if (it != entries_.end()) { it->targetWidth = w; it->targetHeight = h; }
    }

private:
    struct Entry {
        std::string spriteName = "";
        float srcWidth = 0.0f;
        float srcHeight = 0.0f;
        float targetWidth = 0.0f;
        float targetHeight = 0.0f;
    };

    std::vector<Entry> entries_;
};

REGISTER_COMPONENT_SCENE(ScreenBufferKeepRatio)

} // namespace KashipanEngine
