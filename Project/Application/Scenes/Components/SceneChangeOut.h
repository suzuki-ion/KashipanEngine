#pragma once
#include <KashipanEngine.h>
#include <algorithm>
#include <memory>

namespace KashipanEngine {

class SceneChangeOut final : public ISceneComponent {
public:
    SceneChangeOut() : ISceneComponent("SceneChangeOut") {}
    ~SceneChangeOut() override = default;

    SceneChangeOut(const SceneChangeOut &) = delete;
    SceneChangeOut &operator=(const SceneChangeOut &) = delete;

    /// @brief トランジションが完了しているかを取得する
    /// @return 完了していれば true
    bool IsFinished() const noexcept { return finished_; }

    /// @brief トランジション再生を開始する
    void Play() {
        if (playing_) return;
        playing_ = true;
        finished_ = false;
        elapsed_ = 0.0f;
    }

    /// @brief コンポーネント初期化処理
    void Initialize() override {
        elapsed_ = 0.0f;
        initialized_ = false;
        playing_ = false;
        finished_ = false;
    }

    /// @brief コンポーネント終了処理
    void Finalize() override {
        blackSprite_ = nullptr;
        whiteSprite_ = nullptr;
        playing_ = false;
        finished_ = false;
    }

    /// @brief 毎フレーム更新処理
    void Update() override {
        if (!playing_) return;

        if (!initialized_) {
            InitializeSprites();
        }

        float dt = GetDeltaTime();
        if (dt >= 1.0f) dt = 0.0f;
        elapsed_ = std::min(elapsed_ + dt, kDuration);

        auto *window = Window::GetWindow("Main Window");
        const float w = window ? static_cast<float>(window->GetClientWidth()) : 0.0f;
        const float h = window ? static_cast<float>(window->GetClientHeight()) : 0.0f;

        const float t = (kDuration > 0.0f) ? std::clamp(elapsed_ / kDuration, 0.0f, 1.0f) : 1.0f;

        // White: t=0.0..0.5 -> height 0 -> initial
        {
            const float u = Normalize01(t, 0.0f, 0.5f);
            const float e = EaseOutCubic(0.0f, h, u);
            ApplyScale(whiteSprite_, w, e);
        }

        // Black: t=0.2..0.5 -> height 0 -> initial
        {
            const float u = Normalize01(t, 0.2f, 0.5f);
            const float e = EaseOutCubic(0.0f, h, u);
            ApplyScale(blackSprite_, w, e);
        }

        if (elapsed_ >= kDuration) {
            playing_ = false;
            finished_ = true;
        }
    }

private:
    static constexpr float kDuration = 1.0f;

    void InitializeSprites() {
        auto *scene = GetOwnerScene();
        if (!scene) return;

        auto *window = Window::GetWindow("Main Window");
        if (!window) return;

        const float w = static_cast<float>(window->GetClientWidth());
        
        auto whiteTexture = TextureManager::GetTextureFromFileName("white1x1.png");
        // White
        {
            auto obj = std::make_unique<Sprite>();
            obj->SetUniqueBatchKey();
            obj->SetName("SceneChangeOut_White");
            obj->SetAnchorPoint(0.5f, 1.0f);
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                mat->SetTexture(whiteTexture);
            }
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector2(w * 0.5f, 0.0f));
                tr->SetScale(Vector2(w, -0.0f));
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
            obj->SetName("SceneChangeOut_Black");
            obj->SetAnchorPoint(0.5f, 1.0f);
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
                mat->SetTexture(whiteTexture);
            }
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector2(w * 0.5f, 0.0f));
                tr->SetScale(Vector2(w, -0.0f));
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
