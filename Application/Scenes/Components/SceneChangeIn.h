#pragma once
#include <KashipanEngine.h>
#include <algorithm>
#include <memory>

namespace KashipanEngine {

class SceneChangeIn final : public ISceneComponent {
public:
    SceneChangeIn() : ISceneComponent("SceneChangeIn") {}
    ~SceneChangeIn() override = default;

    SceneChangeIn(const SceneChangeIn &) = delete;
    SceneChangeIn &operator=(const SceneChangeIn &) = delete;

    bool IsFinished() const noexcept { return finished_; }

    void Play() {
        if (playing_) return;
        playing_ = true;
        finished_ = false;
        elapsed_ = 0.0f;
    }

    void Initialize() override {
        elapsed_ = 0.0f;
        initialized_ = false;
        playing_ = false;
        finished_ = false;
    }

    void Finalize() override {
        blackSprite_ = nullptr;
        whiteSprite_ = nullptr;
        playing_ = false;
        finished_ = false;
    }

    void Update() override {
        if (!playing_) return;

        if (!initialized_) {
            InitializeSprites();
        }

        float dt = GetDeltaTime();
        if (dt >= 1.0f) dt = 0.0f;
        elapsed_ = std::min(elapsed_ + dt, kDuration);

        auto *window = Window::GetWindow("2301_CLUBOM");
        const float w = window ? static_cast<float>(window->GetClientWidth()) : 0.0f;
        const float h = window ? static_cast<float>(window->GetClientHeight()) : 0.0f;

        const float t = (kDuration > 0.0f) ? std::clamp(elapsed_ / kDuration, 0.0f, 1.0f) : 1.0f;

        // White: t=0.5..1.0 -> height initial -> 0
        {
            const float u = Normalize01(t, 0.5f, 1.0f);
            const float e = EaseOutCubic(h, 0.0f, u);
            ApplyScale(blackSprite_, w, e);
        }

        // Black: t=0.7..1.0 -> height initial -> 0
        {
            const float u = Normalize01(t, 0.7f, 1.0f);
            const float e = EaseOutCubic(h, 0.0f, u);
            ApplyScale(whiteSprite_, w, e);
        }

        if (elapsed_ >= kDuration) {
            playing_ = false;
            finished_ = true;
        }
    }

private:
    static constexpr float kDuration = 1.0f;

    void InitializeSprites() {
        if (!GetOwnerScene()) return;

        auto *window = Window::GetWindow("2301_CLUBOM");
        if (!window) return;

        const float w = static_cast<float>(window->GetClientWidth());
        const float h = static_cast<float>(window->GetClientHeight());

        auto whiteTexture = TextureManager::GetTextureFromFileName("white1x1.png");
        // White
        {
            auto obj = std::make_unique<Sprite>();
            obj->SetUniqueBatchKey();
            obj->SetName("SceneChangeIn_White");
            obj->SetAnchorPoint(0.5f, 0.0f);
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                mat->SetTexture(whiteTexture);
            }
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector2(w * 0.5f, h));
                tr->SetScale(Vector2(w, 0.0f));
            }
            obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
            whiteSprite_ = obj.get();
            if (auto *ctx = GetOwnerContext()) {
                ctx->AddObject2D(std::move(obj));
            }
        }

        // Black
        {
            auto obj = std::make_unique<Sprite>();
            obj->SetUniqueBatchKey();
            obj->SetName("SceneChangeIn_Black");
            obj->SetAnchorPoint(0.5f, 0.0f);
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
                mat->SetTexture(whiteTexture);
            }
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector2(w * 0.5f, h));
                tr->SetScale(Vector2(w, 0.0f));
            }
            obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
            blackSprite_ = obj.get();
            if (auto *ctx = GetOwnerContext()) {
                ctx->AddObject2D(std::move(obj));
            }
        }

        initialized_ = true;
    }

    static void ApplyScale(Sprite *sprite, float w, float h) {
        if (!sprite) return;
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            const float clampedH = std::max(0.0f, h);
            tr->SetScale(Vector2(w, clampedH));
        }
    }

    bool initialized_ = false;
    bool playing_ = false;
    bool finished_ = false;
    float elapsed_ = 0.0f;

    Sprite *blackSprite_ = nullptr;
    Sprite *whiteSprite_ = nullptr;
};

} // namespace KashipanEngine
