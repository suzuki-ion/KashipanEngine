#pragma once
#include <KashipanEngine.h>

#include "Objects/Components/RailMovement.h"

#include "Scenes/Components/AttackGearCircularInside.h"
#include "Scenes/Components/AttackGearCircularOutside.h"
#include "Scenes/Components/AttackGearWallForward.h"
#include "Scenes/Components/AttackGearWallLeftSide.h"
#include "Scenes/Components/AttackGearWallRightSide.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/GameIntroLogoAnimation.h"
#include "Scene/Components/SceneDefaultVariables.h"

#include <string>
#include <vector>
#include <algorithm>
#include <random>

namespace KashipanEngine {

class GameProgressController final : public ISceneComponent {
public:
    /// @brief コンストラクタ。各種参照オブジェクトを設定する
    /// @param sceneVars シーンのデフォルト変数コンポーネント（nullptr でも Initialize 内で取得する）
    /// @param mover 移動対象オブジェクト（Sphere 等）
    /// @param screenSprite スクリーン用スプライト
    /// @param rotatingPlanes 回転するプレーン群（イントロ演出用）
    GameProgressController(
        SceneDefaultVariables *sceneVars,
        Sphere *mover,
        Sprite *screenSprite,
        const std::vector<Object3DBase *> &rotatingPlanes = {},
        const std::vector<SpotLight *> &rotatingSpotLights = {})
        : ISceneComponent("GameProgressController", 1)
        , sceneVars_(sceneVars)
        , mover_(mover)
        , screenSprite_(screenSprite)
        , rotatingPlanes_(rotatingPlanes)
        , rotatingSpotLights_(rotatingSpotLights) {}
    ~GameProgressController() override = default;

    /// @brief 初期化処理。シーンコンポーネントの参照を取得し状態をリセットする
    void Initialize() override {
        if (auto *ctx = GetOwnerContext()) {
            // ensure we have SceneDefaultVariables pointer
            if (!sceneVars_) {
                sceneVars_ = ctx->GetComponent<SceneDefaultVariables>();
            }

            attackGearCircularInside_ = ctx->GetComponent<AttackGearCircularInside>();
            attackGearCircularOutside_ = ctx->GetComponent<AttackGearCircularOutside>();
            attackGearWallForward_ = ctx->GetComponent<AttackGearWallForward>();
            attackGearWallLeftSide_ = ctx->GetComponent<AttackGearWallLeftSide>();
            attackGearWallRightSide_ = ctx->GetComponent<AttackGearWallRightSide>();
            cameraController_ = ctx->GetComponent<CameraController>();
            introLogoAnimation_ = ctx->GetComponent<GameIntroLogoAnimation>();

            // Play は Intro 中にタイミングを見て呼ぶ（Update 側）

            if (mover_) {
                std::vector<Vector3> railPoints = {
                    Vector3(0.0f, 0.0f, 0.0f),
                    Vector3(0.0f, 0.0f, -32.0f),
                    Vector3(0.0f, 0.0f, 128.0f),
                    Vector3(0.0f, 0.0f, 256.0f),
                    Vector3(0.0f, 0.0f, 384.0f),
                    Vector3(0.0f, 0.0f, 500.0f),
                    Vector3(0.0f, 0.0f, 384.0f),
                    Vector3(0.0f, 0.0f, 256.0f),
                    Vector3(0.0f, 0.0f, 128.0f),
                    Vector3(0.0f, 0.0f, 0.0f),
                    Vector3(0.0f, 0.0f, 0.0f),
                };
                mover_->RegisterComponent<RailMovement>(railPoints, kIntroDurationSec + kAttackDurationSec);
            }
        }
        elapsedSec_ = 0.0f;
        prevElapsedSec_ = 0.0f;
        attackElapsedSec_ = 0.0f;
        nextAttackAtSec_ = 0.0f;
        nextChaseInsideAtSec_ = 0.0f;
        finished_ = false;
        introLogoPlayed_ = false;
        introPlanesRaised_ = false;
        introBlinkTimer_ = 0.0f;
        nextIntroBlinkAt_ = 0.0f;

        intermissionBlinkTimer_ = 0.0f;
        nextIntermissionBlinkAt_ = 0.0f;
        intermissionBlinkWhite_ = false;

        intermissionActive_ = false;
        savedDirectionalIntensity_ = 1.0f;
        savedDirectionalColor_ = Vector4{1.0f,1.0f,1.0f,1.0f};

        introShakeTriggered_ = false;
        introDoomPlayed_ = false;
        introNoisePlay_ = AudioManager::kInvalidPlayHandle;

        intermissionShakeTriggered_ = false;
        intermissionDoomPlayed_ = false;
        intermissionNoisePlay_ = AudioManager::kInvalidPlayHandle;

        endFadeStarted_ = false;
        endFadeElapsed_ = 0.0f;
        endFadeStartColor_ = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    /// @brief 毎フレーム更新処理。イントロ演出や攻撃スケジュールを制御する
    void Update() override {
        const float dt = std::max(0.0f, GetDeltaTime());
        elapsedSec_ += dt;

        // Intro 演出 (0..kIntroDurationSec)
        if (elapsedSec_ < kIntroDurationSec) {
            UpdateIntro(dt);
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // Intro 終了時にノイズが鳴りっぱなしにならないよう停止
        StopIntroNoise();

        attackElapsedSec_ = elapsedSec_ - kIntroDurationSec;
        if (attackElapsedSec_ < 0.0f) attackElapsedSec_ = 0.0f;

        // 中間演出 (intermissionStart..intermissionEnd) は攻撃しない
        if (attackElapsedSec_ >= intermissionStartAttackSec_ && attackElapsedSec_ < intermissionEndAttackSec_) {
            UpdateIntermission(dt);
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        if (intermissionActive_ && attackElapsedSec_ >= intermissionEndAttackSec_) {
            OnIntermissionEnd();
        }

        // 中間演出終了時にノイズが鳴りっぱなしにならないよう停止
        StopIntermissionNoise();

        // 攻撃時間を超えたら何もしない（必要なら ResultScene 遷移などに拡張）
        if (attackElapsedSec_ > kAttackDurationSec) {
            finished_ = true;
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // 最後の時間帯は攻撃しない（endFade）
        if (attackElapsedSec_ >= (kAttackDurationSec - endFadeTotalSec_)) {
            UpdateEndFade(dt);
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // 攻撃処理
        ProcessAttacks();

        prevElapsedSec_ = elapsedSec_;
    }

    float GetElapsedSec() const { return elapsedSec_; }
    bool IsFinished() const { return finished_; }

    static constexpr float kIntroDurationSec = 10.0f;
    static constexpr float kAttackDurationSec = 180.0f;

private:
    // --- configuration parameters (replace magic literals) ---
    // Intro
    float introNoiseStartSec_ = 3.0f;
    float introNoiseEndSec_ = 5.0f;
    float introDoomAtSec_ = 5.5f;
    float introShakeAtSec_ = 6.0f;
    float introBlinkStartSec_ = 3.0f;
    float introBlinkEndSec_ = 5.0f;
    float introBlinkIntervalStart_ = 0.35f;
    float introBlinkIntervalEnd_ = 0.05f;
    float introPlanesRaiseAtSec_ = 6.0f;
    float lightFadeStartSec_ = 6.0f;
    float lightFadeEndSec_ = 8.0f;
    float lightIntensityStart_ = 3.2f;
    float lightIntensityEnd_ = 1.6f;
    float introLogoAtSec_ = 8.0f;
    float fovIntroStartFrom_ = 0.1f;
    float fovIntroStartTo_ = 1.0f;
    float fov6to7From_ = 1.0f;
    float fov6to7To_ = 1.5f;
    float fov8to10From_ = 1.5f;
    float fov8to10To_ = 0.7f;

    // Intermission (relative to attackElapsedSec)
    float intermissionStartAttackSec_ = 85.0f;
    float intermissionEndAttackSec_ = 100.0f;
    float intermissionNoiseStartSec_ = 5.0f;
    float intermissionNoiseEndSec_ = 7.0f;
    float intermissionDoomAtSec_ = 7.5f;
    float intermissionShakeAtSec_ = 8.0f;
    float intermissionBlinkStartSec_ = 5.0f;
    float intermissionBlinkEndSec_ = 7.0f;
    float intermissionBlinkIntervalStart_ = 0.35f;
    float intermissionBlinkIntervalEnd_ = 0.05f;
    float intermissionFovStartA_ = 5.0f;
    float intermissionFovEndA_ = 7.0f;
    float intermissionFovStartB_ = 9.0f;
    float intermissionFovEndB_ = 10.0f;

    // End fade
    float endFadeTotalSec_ = 5.0f; // last X seconds no attacks
    float endFadeDelaySec_ = 2.0f;
    float endFadeFadeEndSec_ = 4.0f; // used as Normalize01 end

    // Attack scheduling
    float attackIntervalEarlyEnd_ = 10.0f;
    float attackIntervalMidEnd_ = 90.0f;
    float attackIntervalLateEnd_ = 100.0f;
    float attackIntervalEarlyVal_ = 4.0f;
    float attackIntervalMidVal_ = 1.6f;

    // jitter
    float attackJitterFrac_ = 0.15f;

    // Scene defaults container
    SceneDefaultVariables *sceneVars_ = nullptr;

    Sphere *mover_ = nullptr;
    Sprite *screenSprite_ = nullptr;

    float elapsedSec_ = 0.0f;
    float prevElapsedSec_ = 0.0f;

    float attackElapsedSec_ = 0.0f;
    float nextAttackAtSec_ = 0.0f;
    float nextChaseInsideAtSec_ = 0.0f;

    bool finished_ = false;
    bool introLogoPlayed_ = false;
    bool introPlanesRaised_ = false;

    float introBlinkTimer_ = 0.0f;
    float nextIntroBlinkAt_ = 0.0f;

    float intermissionBlinkTimer_ = 0.0f;
    float nextIntermissionBlinkAt_ = 0.0f;
    bool intermissionBlinkWhite_ = false;

    bool intermissionActive_ = false;
    float savedDirectionalIntensity_ = 1.0f;
    Vector4 savedDirectionalColor_ = Vector4{1.0f,1.0f,1.0f,1.0f};

    AudioManager::PlayHandle introNoisePlay_ = AudioManager::kInvalidPlayHandle;
    bool introDoomPlayed_ = false;
    bool introShakeTriggered_ = false;

    AudioManager::PlayHandle intermissionNoisePlay_ = AudioManager::kInvalidPlayHandle;
    bool intermissionDoomPlayed_ = false;
    bool intermissionShakeTriggered_ = false;

    bool endFadeStarted_ = false;
    float endFadeElapsed_ = 0.0f;
    Vector4 endFadeStartColor_{ 1.0f, 1.0f, 1.0f, 1.0f };

    std::vector<Object3DBase *> rotatingPlanes_;
    std::vector<SpotLight *> rotatingSpotLights_;

    AttackGearCircularInside *attackGearCircularInside_ = nullptr;
    AttackGearCircularOutside *attackGearCircularOutside_ = nullptr;
    AttackGearWallForward *attackGearWallForward_ = nullptr;
    AttackGearWallLeftSide *attackGearWallLeftSide_ = nullptr;
    AttackGearWallRightSide *attackGearWallRightSide_ = nullptr;
    CameraController *cameraController_ = nullptr;
    GameIntroLogoAnimation *introLogoAnimation_ = nullptr;

    // --- helper methods to split Update() ---
    void UpdateIntro(float dt) {
        const float t = elapsedSec_;

        // t = introNoiseStart..introNoiseEnd
        {
            const bool inNoise = (t >= introNoiseStartSec_ && t < introNoiseEndSec_);
            if (inNoise) {
                if (introNoisePlay_ == AudioManager::kInvalidPlayHandle) {
                    const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroNoise.mp3");
                    if (h != AudioManager::kInvalidSoundHandle) {
                        introNoisePlay_ = AudioManager::Play(h, 1.0f, 0.0f, true);
                    }
                }
            } else {
                if (introNoisePlay_ != AudioManager::kInvalidPlayHandle) {
                    AudioManager::Stop(introNoisePlay_);
                    introNoisePlay_ = AudioManager::kInvalidPlayHandle;
                }
            }
        }

        if (!introDoomPlayed_ && t >= introDoomAtSec_) {
            const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroDoom.mp3");
            if (h != AudioManager::kInvalidSoundHandle) {
                AudioManager::Play(h);
            }
            introDoomPlayed_ = true;
        }

        if (!introShakeTriggered_ && t >= introShakeAtSec_) {
            if (cameraController_) {
                cameraController_->Shake(1.0f, 1.0f);
            }
            introShakeTriggered_ = true;
        }

        if (screenSprite_) {
            if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                if (t >= introBlinkStartSec_ && t < introBlinkEndSec_) {
                    introBlinkTimer_ += dt;

                    const float phase = Normalize01(t, introBlinkStartSec_, introBlinkEndSec_);
                    const float interval = Lerp(introBlinkIntervalStart_, introBlinkIntervalEnd_, phase);

                    if (introBlinkTimer_ >= nextIntroBlinkAt_) {
                        static bool sWhite = false;
                        sWhite = !sWhite;
                        mat->SetColor(sWhite ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });

                        nextIntroBlinkAt_ = introBlinkTimer_ + std::max(0.01f, interval);
                    }
                } else if (t >= introBlinkEndSec_ && t < introPlanesRaiseAtSec_) {
                    mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                }
            }
        }

        if (t >= introPlanesRaiseAtSec_ && !introPlanesRaised_) {
            for (auto *p : rotatingPlanes_) {
                if (!p) continue;
                if (auto *tr = p->GetComponent3D<Transform3D>()) {
                    auto v = tr->GetTranslate();
                    v.y += 10000.0f;
                    tr->SetTranslate(v);
                }
            }
            if (screenSprite_) {
                if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                    mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
                }
            }
            introPlanesRaised_ = true;
        }

        // light from scene defaults
        if (sceneVars_) {
            if (auto *light = sceneVars_->GetDirectionalLight()) {
                if (t >= lightFadeStartSec_ && t < lightFadeEndSec_) {
                    const float u = Normalize01(t, lightFadeStartSec_, lightFadeEndSec_);
                    const float intensity = Lerp(lightIntensityStart_, lightIntensityEnd_, u);
                    light->SetIntensity(intensity);
                }
            }
        }

        if (!introLogoPlayed_ && introLogoAnimation_ && t >= introLogoAtSec_) {
            introLogoAnimation_->Play();
            introLogoPlayed_ = true;
        }

        if (cameraController_ && t < 1.0f) {
            const float u = Normalize01(t, 0.0f, 1.0f);
            const float fov = EaseInOutCubic(fovIntroStartFrom_, fovIntroStartTo_, u);
            cameraController_->SetTargetFovY(fov);
        }

        if (cameraController_ && t >= 6.0f && t < 7.0f) {
            const float u = Normalize01(t, 6.0f, 7.0f);
            const float fov = EaseOutExpo(fov6to7From_, fov6to7To_, u);
            cameraController_->SetTargetFovY(fov);
        }

        if (cameraController_ && t >= introLogoAtSec_) {
            const float u = Normalize01(t, introLogoAtSec_, introLogoAtSec_ + 2.0f);
            const float fov = EaseInOutCubic(fov8to10From_, fov8to10To_, u);
            cameraController_->SetTargetFovY(fov);
        }
    }

    void StopIntroNoise() {
        if (introNoisePlay_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(introNoisePlay_);
            introNoisePlay_ = AudioManager::kInvalidPlayHandle;
        }
    }

    void UpdateIntermission(float /*dt*/) {
        const float t = attackElapsedSec_ - intermissionStartAttackSec_; // 0..(intermissionEnd-intermissionStart)

        // t = intermissionNoiseStart..intermissionNoiseEnd
        {
            const bool inNoise = (t >= intermissionNoiseStartSec_ && t < intermissionNoiseEndSec_);
            if (inNoise) {
                if (intermissionNoisePlay_ == AudioManager::kInvalidPlayHandle) {
                    const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroNoise.mp3");
                    if (h != AudioManager::kInvalidSoundHandle) {
                        intermissionNoisePlay_ = AudioManager::Play(h, 1.0f, 0.0f, true);
                    }
                }
            } else {
                if (intermissionNoisePlay_ != AudioManager::kInvalidPlayHandle) {
                    AudioManager::Stop(intermissionNoisePlay_);
                    intermissionNoisePlay_ = AudioManager::kInvalidPlayHandle;
                }
            }
        }

        if (!intermissionDoomPlayed_ && t >= intermissionDoomAtSec_) {
            const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroDoom.mp3");
            if (h != AudioManager::kInvalidSoundHandle) {
                AudioManager::Play(h);
            }
            intermissionDoomPlayed_ = true;
        }

        if (!intermissionShakeTriggered_ && t >= intermissionShakeAtSec_) {
            if (cameraController_) {
                cameraController_->Shake(1.0f, 1.0f);
            }

            intermissionShakeTriggered_ = true;
        }

        if (screenSprite_) {
            if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                if (t >= intermissionBlinkStartSec_ && t < intermissionBlinkEndSec_) {
                    intermissionBlinkTimer_ += GetDeltaTime();

                    const float phase = Normalize01(t, intermissionBlinkStartSec_, intermissionBlinkEndSec_);
                    const float interval = Lerp(intermissionBlinkIntervalStart_, intermissionBlinkIntervalEnd_, phase);

                    if (intermissionBlinkTimer_ >= nextIntermissionBlinkAt_) {
                        intermissionBlinkWhite_ = !intermissionBlinkWhite_;
                        mat->SetColor(intermissionBlinkWhite_ ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                        nextIntermissionBlinkAt_ = intermissionBlinkTimer_ + std::max(0.01f, interval);
                    }
                } else if (t >= intermissionBlinkEndSec_ && t < intermissionShakeAtSec_) {
                    mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                } else if (t >= intermissionShakeAtSec_ && t < intermissionShakeAtSec_ + 1.0f) {
                    mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
                    if (sceneVars_) {
                        if (auto *dir = sceneVars_->GetDirectionalLight()) {
                            savedDirectionalIntensity_ = dir->GetIntensity();
                            savedDirectionalColor_ = dir->GetColor();
                            dir->SetIntensity(0.3f);
                            dir->SetColor(Vector4{ 0.7f, 0.7f, 1.0f, 1.0f });
                        }
                    }
                    for (auto *spotLight : rotatingSpotLights_) {
                        if (spotLight) spotLight->SetEnabled(true);
                    }
                    intermissionActive_ = true;
                }
            }
        }

        if (cameraController_ && t >= intermissionFovStartA_ && t < intermissionFovEndA_) {
            const float u = Normalize01(t, intermissionFovStartA_, intermissionFovEndA_);
            const float fov = EaseOutCubic(0.7f, 1.5f, u);
            cameraController_->SetTargetFovY(fov);
        }

        if (cameraController_ && t >= intermissionFovStartB_ && t < intermissionFovEndB_) {
            const float u = Normalize01(t, intermissionFovStartB_, intermissionFovEndB_);
            const float fov = EaseInOutCubic(1.5f, 0.7f, u);
            cameraController_->SetTargetFovY(fov);
        }

        nextAttackAtSec_ = intermissionEndAttackSec_;
        nextChaseInsideAtSec_ = std::max(nextChaseInsideAtSec_, intermissionEndAttackSec_);
    }

    void OnIntermissionEnd() {
        // restore directional light and enable gear point lights
        if (intermissionActive_) {
            if (sceneVars_) {
                if (auto *dir = sceneVars_->GetDirectionalLight()) {
                    dir->SetIntensity(savedDirectionalIntensity_);
                    dir->SetColor(savedDirectionalColor_);
                }
                // enable point light spawning on gear attacks
                if (attackGearCircularInside_) attackGearCircularInside_->SetSpawnWithPointLight(true);
                if (attackGearCircularOutside_) attackGearCircularOutside_->SetSpawnWithPointLight(true);
                if (attackGearWallForward_) attackGearWallForward_->SetSpawnWithPointLight(true);
                if (attackGearWallLeftSide_) attackGearWallLeftSide_->SetSpawnWithPointLight(true);
                if (attackGearWallRightSide_) attackGearWallRightSide_->SetSpawnWithPointLight(true);
            }
        }
        intermissionActive_ = false;
    }

    void StopIntermissionNoise() {
        if (intermissionNoisePlay_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(intermissionNoisePlay_);
            intermissionNoisePlay_ = AudioManager::kInvalidPlayHandle;
        }
    }

    void UpdateEndFade(float dt) {
        if (!endFadeStarted_) {
            endFadeStarted_ = true;
            endFadeElapsed_ = 0.0f;
            if (auto *light = sceneVars_ ? sceneVars_->GetDirectionalLight() : nullptr) {
                endFadeStartColor_ = light->GetColor();
            }

            for (auto *spotLight : rotatingSpotLights_) {
                if (spotLight) spotLight->SetEnabled(false);
            }
        }

        endFadeElapsed_ += dt;

        const float t = endFadeElapsed_;
        if (auto *light = sceneVars_ ? sceneVars_->GetDirectionalLight() : nullptr) {
            if (t >= endFadeDelaySec_) {
                const float u = Normalize01(t - endFadeDelaySec_, 0.0f, endFadeFadeEndSec_ - endFadeDelaySec_);
                const Vector4 c = Lerp(endFadeStartColor_, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, u);
                const float intensity = Lerp(0.3f, 1.6f, u);
                light->SetColor(c);
                light->SetIntensity(intensity);
            }
        }
    }

    float ComputeInterval(float tSec) const {
        // 0-10s: ゆっくり, 10-90s: 中程度に加速, 95s+: スパイク
        if (tSec < attackIntervalEarlyEnd_) {
            return attackIntervalEarlyVal_; // だいぶゆっくり
        }
        if (tSec < attackIntervalMidEnd_) {
            const float u = (tSec - attackIntervalEarlyEnd_) / (attackIntervalMidEnd_ - attackIntervalEarlyEnd_); // 0..1
            return attackIntervalEarlyVal_ + (attackIntervalMidVal_ - attackIntervalEarlyVal_) * std::clamp(u, 0.0f, 1.0f);
        }
        if (tSec < attackIntervalLateEnd_) {
            return attackIntervalMidVal_; // 90-100 は据え置き（intermission は別扱い）
        }
        {
            const float u = (tSec - attackIntervalLateEnd_) / (kAttackDurationSec - attackIntervalLateEnd_);
            const float base = 1.0f + (0.55f - 1.0f) * std::clamp(u, 0.0f, 1.0f);
            return base * 1.25f; // interval を 1/0.8 倍して頻度を下げる
        }
    }

    void ProcessAttacks() {
        while (attackElapsedSec_ >= nextAttackAtSec_) {
            static thread_local std::mt19937 rng{ 98765u };

            Vector3 baseTarget{0.0f, 0.0f, 0.0f};

            const bool doChaseInside = (attackElapsedSec_ >= nextChaseInsideAtSec_);
            if (doChaseInside && attackGearCircularInside_) {
                Vector3 p{0.0f, 0.0f, 0.0f};
                if (cameraController_ && cameraController_->GetFollowTarget()) {
                    if (auto *tr = cameraController_->GetFollowTarget()->GetComponent3D<Transform3D>()) {
                        p = tr->GetTranslate();
                    }
                }
                attackGearCircularInside_->SetTargetPosition(p);
                attackGearCircularInside_->Attack();

                const float base = (attackElapsedSec_ < 100.0f) ? 16.0f : 12.5f;
                const float jitter = (attackElapsedSec_ < 100.0f) ? 8.0f : 6.0f;
                nextChaseInsideAtSec_ = attackElapsedSec_ + (base + std::uniform_real_distribution<float>(-jitter, jitter)(rng));
            } else {
                float wCircIn = 1.0f;
                float wCircOut = 1.0f;
                float wWall = 1.0f;

                if (attackElapsedSec_ < 10.0f) {
                    wWall = 0.5f;
                } else if (attackElapsedSec_ < 90.0f) {
                    wWall = 1.0f;
                } else {
                    wWall = 1.4f;
                }
                if (attackElapsedSec_ >= 100.0f) {
                    wWall = 1.44f;
                    wCircOut = 1.0f;
                }

                const float sum = wCircIn + wCircOut + (wWall * 3.0f);
                const float r = std::uniform_real_distribution<float>(0.0f, sum)(rng);

                if (r < wCircIn) {
                    if (attackGearCircularInside_) {
                        attackGearCircularInside_->SetTargetPosition(baseTarget);
                        attackGearCircularInside_->Attack();
                    }
                } else if (r < wCircIn + wCircOut) {
                    if (attackGearCircularOutside_) {
                        // Spawn from one of four floor corners instead of center
                        // Floor spans x = [-5, 5], z = [-5, 5]
                        const int cornerIdx = std::uniform_int_distribution<int>(0, 3)(rng);
                        Vector3 cornerTarget{0.0f, 0.0f, 0.0f};
                        switch (cornerIdx) {
                            case 0: cornerTarget = Vector3{ -5.0f, 0.0f, 5.0f }; break; // left-奥 (left-back)
                            case 1: cornerTarget = Vector3{ 5.0f, 0.0f, 5.0f }; break;  // right-奥 (right-back)
                            case 2: cornerTarget = Vector3{ -5.0f, 0.0f, -5.0f }; break; // left-前 (left-front)
                            case 3: cornerTarget = Vector3{ 5.0f, 0.0f, -5.0f }; break;  // right-前 (right-front)
                        }
                        attackGearCircularOutside_->SetTargetPosition(cornerTarget);
                        attackGearCircularOutside_->Attack();
                    }
                } else {
                    const int w = std::uniform_int_distribution<int>(0, 2)(rng);
                    if (w == 0 && attackGearWallForward_) {
                        attackGearWallForward_->Attack();
                    } else if (w == 1 && attackGearWallLeftSide_) {
                        attackGearWallLeftSide_->Attack();
                    } else if (w == 2 && attackGearWallRightSide_) {
                        attackGearWallRightSide_->Attack();
                    }
                }
            }

            const float interval = ComputeInterval(attackElapsedSec_);
            const float jitter = interval * attackJitterFrac_;
            nextAttackAtSec_ = attackElapsedSec_ + std::max(0.15f, interval + std::uniform_real_distribution<float>(-jitter, jitter)(rng));

            if (nextChaseInsideAtSec_ <= 0.0f) {
                nextChaseInsideAtSec_ = 18.0f;
            }
        }
    }
};

} // namespace KashipanEngine