#pragma once
#include <KashipanEngine.h>
#include <memory>
#include <algorithm>

namespace KashipanEngine {

class SceneFade final : public ISceneComponent {
public:
    SceneFade()
        : ISceneComponent("SceneFade")
    {}

    ~SceneFade() override = default;

    SceneFade(const SceneFade &) = delete;
    SceneFade &operator=(const SceneFade &) = delete;

    // Configure fade color (rgb) and duration (seconds)
    void SetColor(const Vector4 &c) { color_ = c; }
    void SetDuration(float d) { duration_ = std::max(0.0f, d); }
    // Delay before fade starts and after fade ends (seconds)
    void SetDelayBefore(float d) { delayBefore_ = std::max(0.0f, d); }
    void SetDelayAfter(float d) { delayAfter_ = std::max(0.0f, d); }

    bool IsFinished() const noexcept { return finished_; }

    // Play fade-in: color -> transparent
    void PlayIn() { Start(true); }
    // Play fade-out: transparent -> color
    void PlayOut() { Start(false); }

    void Initialize() override {
        elapsed_ = 0.0f;
        initialized_ = false;
        playing_ = false;
        finished_ = false;
    }

    void Finalize() override {
        sprite_ = nullptr;
        playing_ = false;
        finished_ = false;
    }

    void Update() override {
        if (!playing_) return;

        if (!initialized_) {
            InitializeSprite();
        }

        float dt = GetDeltaTime();
        if (dt >= 1.0f) dt = 0.0f;
        const float total = delayBefore_ + duration_ + delayAfter_;
        elapsed_ = std::min(elapsed_ + dt, total);

        auto *window = Window::GetWindow("2301_CLUBOM");
        const float w = window ? static_cast<float>(window->GetClientWidth()) : 0.0f;
        const float h = window ? static_cast<float>(window->GetClientHeight()) : 0.0f;

        float alpha = 0.0f;
        // Determine alpha according to delay/duration/post-delay
        if (elapsed_ < delayBefore_) {
            alpha = playingFadeIn_ ? 1.0f : 0.0f; // before fade keep start alpha
        } else if (elapsed_ <= (delayBefore_ + duration_)) {
            const float localT = Normalize01(elapsed_ - delayBefore_, 0.0f, duration_);
            alpha = playingFadeIn_ ? Lerp(1.0f, 0.0f, localT) : Lerp(0.0f, 1.0f, localT);
        } else {
            alpha = playingFadeIn_ ? 0.0f : 1.0f; // after fade keep final alpha
        }

        if (sprite_) {
            if (auto *mat = sprite_->GetComponent2D<Material2D>()) {
                Vector4 c = color_;
                c.w = alpha;
                mat->SetColor(c);
            }
            ApplyScale(sprite_, w, h);
        }

        if (elapsed_ >= total) {
            playing_ = false;
            finished_ = true;
        }
    }

private:
    void Start(bool fadeIn) {
        if (playing_) return;
        playing_ = true;
        finished_ = false;
        elapsed_ = 0.0f;
        playingFadeIn_ = fadeIn;
    }

    void InitializeSprite() {
        auto *scene = GetOwnerScene();
        if (!scene) return;

        auto *window = Window::GetWindow("2301_CLUBOM");
        if (!window) return;

        const float w = static_cast<float>(window->GetClientWidth());
        const float h = static_cast<float>(window->GetClientHeight());

        auto tex = TextureManager::GetTextureFromFileName("white1x1.png");
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("SceneFade_Solid");
        obj->SetAnchorPoint(0.5f, 0.5f);
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            Vector4 c = color_;
            // start alpha depending on mode
            c.w = playingFadeIn_ ? 1.0f : 0.0f;
            mat->SetColor(c);
            mat->SetTexture(tex);
        }
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2(w * 0.5f, h * 0.5f));
            tr->SetScale(Vector2(w, h));
        }
        obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
        sprite_ = obj.get();
        if (auto *ctx = GetOwnerContext()) {
            ctx->AddObject2D(std::move(obj));
        }

        initialized_ = true;
    }

    static void ApplyScale(Sprite *sprite, float w, float h) {
        if (!sprite) return;
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector2(w, h));
        }
    }

    Vector4 color_ = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    float duration_ = 1.0f;
    float delayBefore_ = 0.0f;
    float delayAfter_ = 0.0f;

    bool initialized_ = false;
    bool playing_ = false;
    bool finished_ = false;
    bool playingFadeIn_ = true; // true: color -> transparent
    float elapsed_ = 0.0f;

    Sprite *sprite_ = nullptr;
};

} // namespace KashipanEngine
