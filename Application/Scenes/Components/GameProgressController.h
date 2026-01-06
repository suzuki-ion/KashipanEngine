#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/Components/RailMovement.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/3D/Transform3D.h"

#include "Math/Easings.h"

#include "Scenes/Components/AttackGearCircularInside.h"
#include "Scenes/Components/AttackGearCircularOutside.h"
#include "Scenes/Components/AttackGearWallForward.h"
#include "Scenes/Components/AttackGearWallLeftSide.h"
#include "Scenes/Components/AttackGearWallRightSide.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/GameIntroLogoAnimation.h"

#include "Assets/AudioManager.h"

#include <string>
#include <vector>
#include <algorithm>
#include <random>

namespace KashipanEngine {

class GameProgressController final : public ISceneComponent {
public:
    GameProgressController(
        Camera3D *camera,
        DirectionalLight *light,
        Sphere *mover,
        Sprite *screenSprite,
        const std::vector<Object3DBase *> &rotatingPlanes = {})
        : ISceneComponent("GameProgressController", 1)
        , camera_(camera)
        , light_(light)
        , mover_(mover)
        , screenSprite_(screenSprite)
        , rotatingPlanes_(rotatingPlanes) {}
    ~GameProgressController() override = default;

    void Initialize() override {
        if (auto *ctx = GetOwnerContext()) {
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

    void Update() override {
        const float dt = std::max(0.0f, GetDeltaTime());
        elapsedSec_ += dt;

        // Intro 演出 (0..kIntroDurationSec)
        if (elapsedSec_ < kIntroDurationSec) {
            const float t = elapsedSec_;

            // t=3..5: ノイズを再生（時間外になったら停止）
            {
                const bool inNoise = (t >= 3.0f && t < 5.0f);
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

            // t=5.5: "gameIntroDoom.mp3" を再生
            if (!introDoomPlayed_ && t >= 5.5f) {
                const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroDoom.mp3");
                if (h != AudioManager::kInvalidSoundHandle) {
                    AudioManager::Play(h);
                }
                introDoomPlayed_ = true;
            }

            // t=6.0: カメラシェイク
            if (!introShakeTriggered_ && t >= 6.0f) {
                if (cameraController_) {
                    cameraController_->Shake(1.0f, 1.0f);
                }
                introShakeTriggered_ = true;
            }

            // t=3..5: ScreenBuffer用スプライトの点滅（点滅間隔はだんだん速くする）
            if (screenSprite_) {
                if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                    if (t >= 3.0f && t < 5.0f) {
                        introBlinkTimer_ += dt;

                        const float phase = Normalize01(t, 3.0f, 5.0f);
                        const float interval = Lerp(0.35f, 0.05f, phase);

                        if (introBlinkTimer_ >= nextIntroBlinkAt_) {
                            // 白黒点滅
                            static bool sWhite = false;
                            sWhite = !sWhite;
                            mat->SetColor(sWhite ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });

                            nextIntroBlinkAt_ = introBlinkTimer_ + std::max(0.01f, interval);
                        }
                    } else if (t >= 5.0f && t < 6.0f) {
                        mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                    }
                }
            }

            // t=6: 回転プレーンを出現させ、ScreenBuffer用スプライトを白に戻す
            if (t >= 6.0f && !introPlanesRaised_) {
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

            // t=6..8: ライト強度を 3.2 -> 1.6 (Lerp)
            if (light_ && t >= 6.0f && t < 8.0f) {
                const float u = Normalize01(t, 6.0f, 8.0f);
                const float intensity = Lerp(3.2f, 1.6f, u);
                light_->SetIntensity(intensity);
            }

            // t=8: ロゴ再生
            if (!introLogoPlayed_ && introLogoAnimation_ && t >= 8.0f) {
                introLogoAnimation_->Play();
                introLogoPlayed_ = true;
            }

            // t=0..1: カメラFOVを 0.1 -> 1.0 (EaseInOutCubic)
            if (cameraController_ && t < 1.0f) {
                const float u = Normalize01(t, 0.0f, 1.0f);
                const float fov = EaseInOutCubic(0.1f, 1.0f, u);
                cameraController_->SetTargetFovY(fov);
            }

            // t=6..7: カメラFOVを 1.0 -> 1.5 (EaseOutExpo)
            if (cameraController_ && t >= 6.0f && t < 7.0f) {
                const float u = Normalize01(t, 6.0f, 7.0f);
                const float fov = EaseOutExpo(1.0f, 1.5f, u);
                cameraController_->SetTargetFovY(fov);
            }

            // t=8..10: カメラFOVを 1.5 -> 0.7 (EaseInOutCubic)
            if (cameraController_ && t >= 8.0f) {
                const float u = Normalize01(t, 8.0f, 10.0f);
                const float fov = EaseInOutCubic(1.5f, 0.7f, u);
                cameraController_->SetTargetFovY(fov);
            }

            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // Intro 終了時にノイズが鳴りっぱなしにならないよう停止
        if (introNoisePlay_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(introNoisePlay_);
            introNoisePlay_ = AudioManager::kInvalidPlayHandle;
        }

        attackElapsedSec_ = elapsedSec_ - kIntroDurationSec;
        if (attackElapsedSec_ < 0.0f) attackElapsedSec_ = 0.0f;

        // 中間演出 (85-100s) は攻撃しない
        if (attackElapsedSec_ >= 85.0f && attackElapsedSec_ < 100.0f) {
            const float t = attackElapsedSec_ - 85.0f; // t=0.0 は 85 秒

            // t=5..7: ノイズを再生（時間外になったら停止）
            {
                const bool inNoise = (t >= 5.0f && t < 7.0f);
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

            // t=7.5: "gameIntroDoom.mp3" を再生
            if (!intermissionDoomPlayed_ && t >= 7.5f) {
                const auto h = AudioManager::GetSoundHandleFromFileName("gameIntroDoom.mp3");
                if (h != AudioManager::kInvalidSoundHandle) {
                    AudioManager::Play(h);
                }
                intermissionDoomPlayed_ = true;
            }

            // t=8.0: カメラシェイク
            if (!intermissionShakeTriggered_ && t >= 8.0f) {
                if (cameraController_) {
                    cameraController_->Shake(1.0f, 1.0f);
                }
                intermissionShakeTriggered_ = true;
            }

            // t=5..7: スプライトを白黒点滅（開始時演出と同じ点滅）。t=7 で黒にする。
            if (screenSprite_) {
                if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                    if (t >= 5.0f && t < 7.0f) {
                        intermissionBlinkTimer_ += dt;

                        const float phase = Normalize01(t, 5.0f, 7.0f);
                        const float interval = Lerp(0.35f, 0.05f, phase);

                        if (intermissionBlinkTimer_ >= nextIntermissionBlinkAt_) {
                            intermissionBlinkWhite_ = !intermissionBlinkWhite_;
                            mat->SetColor(intermissionBlinkWhite_ ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                            nextIntermissionBlinkAt_ = intermissionBlinkTimer_ + std::max(0.01f, interval);
                        }
                    } else if (t >= 7.0f && t < 8.0f) {
                        mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
                    } else if (t >= 8.0f && t < 9.0f) {
                        mat->SetColor(Vector4{ 1.0f, 0.5f, 0.5f, 1.0f });
                    }
                }
            }

            // t=5..7: FOV 0.7 -> 1.5 (EaseOutCubic)
            if (cameraController_ && t >= 5.0f && t < 7.0f) {
                const float u = Normalize01(t, 5.0f, 7.0f);
                const float fov = EaseOutCubic(0.7f, 1.5f, u);
                cameraController_->SetTargetFovY(fov);
            }

            // t=9..10: FOV 1.5 -> 0.7 (EaseInOutCubic)
            if (cameraController_ && t >= 9.0f && t < 10.0f) {
                const float u = Normalize01(t, 9.0f, 10.0f);
                const float fov = EaseInOutCubic(1.5f, 0.7f, u);
                cameraController_->SetTargetFovY(fov);
            }

            nextAttackAtSec_ = 100.0f;
            nextChaseInsideAtSec_ = std::max(nextChaseInsideAtSec_, 100.0f);
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // 中間演出終了時にノイズが鳴りっぱなしにならないよう停止
        if (intermissionNoisePlay_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(intermissionNoisePlay_);
            intermissionNoisePlay_ = AudioManager::kInvalidPlayHandle;
        }

        // 攻撃時間を超えたら何もしない（必要なら ResultScene 遷移などに拡張）
        if (attackElapsedSec_ > kAttackDurationSec) {
            finished_ = true;
            prevElapsedSec_ = elapsedSec_;
            return;
        }

        // 最後 5 秒は攻撃しない
        if (attackElapsedSec_ >= (kAttackDurationSec - 5.0f)) {
            if (!endFadeStarted_) {
                endFadeStarted_ = true;
                endFadeElapsed_ = 0.0f;
                if (screenSprite_) {
                    if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                        endFadeStartColor_ = mat->GetColor();
                    }
                }
            }

            endFadeElapsed_ += dt;

            // 最後の5秒が始まってから2秒待って、その後2秒かけて白に戻す
            if (screenSprite_) {
                if (auto *mat = screenSprite_->GetComponent2D<Material2D>()) {
                    const float u = Normalize01(endFadeElapsed_, 2.0f, 4.0f);
                    if (u >= 0.0f) {
                        mat->SetColor(Lerp(endFadeStartColor_, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, u));
                    }
                }
            }

            prevElapsedSec_ = elapsedSec_;
            return;
        }

        auto computeInterval = [&](float tSec) {
            // 0-10s: ゆっくり, 10-90s: 中程度に加速, 95s+: スパイク
            if (tSec < 10.0f) {
                return 4.0f; // だいぶゆっくり
            }
            if (tSec < 90.0f) {
                const float u = (tSec - 10.0f) / 80.0f; // 0..1
                return 4.0f + (1.6f - 4.0f) * std::clamp(u, 0.0f, 1.0f);
            }
            if (tSec < 100.0f) {
                return 1.6f; // 90-100 は据え置き（85-100 は中間演出で停止）
            }
            {
                // 100s+ : 中間演出後は少し頻度を落とす（強度を弱める）
                const float u = (tSec - 100.0f) / (kAttackDurationSec - 100.0f);
                const float base = 1.0f + (0.55f - 1.0f) * std::clamp(u, 0.0f, 1.0f);
                return base * 1.25f; // interval を 1/0.8 倍して頻度を下げる
            }
        };

        while (attackElapsedSec_ >= nextAttackAtSec_) {
            static thread_local std::mt19937 rng{ 98765u };

            // ターゲットは mover の進行(0,0,0)基準で良いが、追尾攻撃用にプレイヤーを読む
            Vector3 baseTarget{0.0f, 0.0f, 0.0f};

            // 追尾系: 時々プレイヤー位置を狙う AttackGearCircularInside
            const bool doChaseInside = (attackElapsedSec_ >= nextChaseInsideAtSec_);
            if (doChaseInside && attackGearCircularInside_) {
                // FollowTarget は player を想定して GameScene 側で設定されるので、ここでは mover から推定はしない。
                // mover_ の子で動くため、target は local XZ を指定する。
                Vector3 p{0.0f, 0.0f, 0.0f};
                if (cameraController_ && cameraController_->GetFollowTarget()) {
                    if (auto *tr = cameraController_->GetFollowTarget()->GetComponent3D<Transform3D>()) {
                        p = tr->GetTranslate();
                    }
                }
                attackGearCircularInside_->SetTargetPosition(p);
                attackGearCircularInside_->Attack();

                // 次の追尾は 12-20 秒後（後半は少し短く）
                const float base = (attackElapsedSec_ < 100.0f) ? 16.0f : 12.5f;
                const float jitter = (attackElapsedSec_ < 100.0f) ? 8.0f : 6.0f;
                nextChaseInsideAtSec_ = attackElapsedSec_ + (base + std::uniform_real_distribution<float>(-jitter, jitter)(rng));
            } else {
                // 通常攻撃: 時間で重み付け
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
                    // 旧 95s+ spike より少し弱める
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
                        attackGearCircularOutside_->SetTargetPosition(baseTarget);
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

            // 次攻撃時刻
            const float interval = computeInterval(attackElapsedSec_);
            const float jitter = interval * 0.15f;
            nextAttackAtSec_ = attackElapsedSec_ + std::max(0.15f, interval + std::uniform_real_distribution<float>(-jitter, jitter)(rng));

            // 初回の追尾攻撃を 18 秒前後に設定
            if (nextChaseInsideAtSec_ <= 0.0f) {
                nextChaseInsideAtSec_ = 18.0f;
            }
        }

        prevElapsedSec_ = elapsedSec_;
    }

    float GetElapsedSec() const { return elapsedSec_; }
    bool IsFinished() const { return finished_; }

    static constexpr float kIntroDurationSec = 10.0f;
    static constexpr float kAttackDurationSec = 180.0f;

private:
    Camera3D *camera_ = nullptr;
    DirectionalLight *light_ = nullptr;
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

    AttackGearCircularInside *attackGearCircularInside_ = nullptr;
    AttackGearCircularOutside *attackGearCircularOutside_ = nullptr;
    AttackGearWallForward *attackGearWallForward_ = nullptr;
    AttackGearWallLeftSide *attackGearWallLeftSide_ = nullptr;
    AttackGearWallRightSide *attackGearWallRightSide_ = nullptr;
    CameraController *cameraController_ = nullptr;
    GameIntroLogoAnimation *introLogoAnimation_ = nullptr;
};

} // namespace KashipanEngine