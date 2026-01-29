#include "BackMonitorWithMenuScreen.h"
#include "BackMonitor.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include <algorithm>
#include <cmath>

namespace KashipanEngine {

BackMonitorWithMenuScreen::BackMonitorWithMenuScreen(ScreenBuffer* target, InputCommand* inputCommand)
    : BackMonitorRenderer("BackMonitorWithMenuScreen", target), inputCommand_(inputCommand) {}

BackMonitorWithMenuScreen::~BackMonitorWithMenuScreen() {}

void BackMonitorWithMenuScreen::Initialize() {
    auto target = GetTargetScreenBuffer();
    if (!target) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

    const float centerX = 0.0f;
    const float topY = 2.0f;
    const float bottomY = -2.0f;
    const float spacing = (topY - bottomY) / 3.0f;
    const float depth = 2.5f;
    const Vector3 scaleVec{ 1.0f, 1.0f, 1.0f };

    // Start（インデックス0）-> 上
    if (!menuStart_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuStart.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuStart");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        models_[0] = obj.get();
        menuStart_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuStart_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 0, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
    }
    if (auto *mat = menuStart_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    // Credit（インデックス1）
    if (!menuCredit_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuCredit.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuCredit");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        models_[1] = obj.get();
        menuCredit_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuCredit_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 1, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
    }
    if (auto *mat = menuCredit_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    // Title（インデックス2）
    if (!menuTitle_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuTitle.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuTitle");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        models_[2] = obj.get();
        menuTitle_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuTitle_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 2, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
    }
    if (auto *mat = menuTitle_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    // Quit（インデックス3）-> 下
    if (!menuQuit_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuQuit.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuQuit");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        models_[3] = obj.get();
        menuQuit_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuQuit_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 3, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
    }
    if (auto *mat = menuQuit_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    isSubmitted_ = false;
    selectedIndex_ = 0;
    // サイン波タイマーをリセット
    rotSineTime_ = 0.0f;

    for (int i = 0; i < 4; ++i) {
        if (auto *mat = models_[i]->GetComponent3D<Material3D>()) {
            if (i == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
            else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
        }
        // 初期のX座標を0にする
        if (auto *tr = models_[i]->GetComponent3D<Transform3D>()) {
            Vector3 p = tr->GetTranslate();
            p.x = 0.0f;
            tr->SetTranslate(p);
        }

        xAnimating_[i] = false;
        xElapsed_[i] = 0.0f;
        xDuration_[i] = 0.0f;
    }
}

void BackMonitorWithMenuScreen::Update() {
    static bool preIsActive = false;
    if (!IsActive()) {
        for (int i = 0; i < 4; ++i) {
            if (!models_[i]) continue;
            if (auto *mat = models_[i]->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 0.0f,0.0f,0.0f,0.0f });
            }
        }
        if (!preIsActive) {
            Initialize();
            preIsActive = true;
        }
        return;
    }
    preIsActive = false;

    if (!GetBackMonitor() || !GetBackMonitor()->IsReady()) return;

    //const float animDuration = 0.2f; // zアニメーションは短めに保つ

    //==================================================
    // 入力処理
    //==================================================

    if (!inputCommand_) return;

    /*if (inputCommand_->Evaluate("MoveUp").Triggered()) {
        selectedIndex_ = (selectedIndex_ - 1 + 4) % 4;
        for (int i = 0; i < 4; ++i) {
            if (!models_[i]) continue;
            if (i == selectedIndex_) {
                zStart_[i] = models_[i]->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[i] = 2.0f;
                zElapsed_[i] = 0.0f;
                zDuration_[i] = animDuration;
                zAnimating_[i] = true;
            } else {
                zStart_[i] = models_[i]->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[i] = 2.5f;
                zElapsed_[i] = 0.0f;
                zDuration_[i] = animDuration;
                zAnimating_[i] = true;
            }
        }
    }
    if (inputCommand_->Evaluate("MoveDown").Triggered()) {
        selectedIndex_ = (selectedIndex_ + 1) % 4;
        for (int i = 0; i < 4; ++i) {
            if (!models_[i]) continue;
            if (i == selectedIndex_) {
                zStart_[i] = models_[i]->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[i] = 2.0f;
                zElapsed_[i] = 0.0f;
                zDuration_[i] = animDuration;
                zAnimating_[i] = true;
            } else {
                zStart_[i] = models_[i]->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[i] = 2.5f;
                zElapsed_[i] = 0.0f;
                zDuration_[i] = animDuration;
                zAnimating_[i] = true;
            }
        }
    }*/
    if (!isSubmitted_ && inputCommand_->Evaluate("Submit").Triggered()) {
        isSubmitted_ = true;
        confirmedIndex_ = selectedIndex_;
        isConfirming_ = true;
        // 確定用アニメーションの準備
        if (models_[confirmedIndex_]) {
            auto *tr = models_[confirmedIndex_]->GetComponent3D<Transform3D>();
            confirmStartPos_ = tr->GetTranslate();
            confirmEndPos_ = Vector3{ 0.0f, 0.0f, 2.0f };
            confirmStartRot_ = tr->GetRotate();
            const float degToRad = M_PI / 180.0f;
            confirmEndRot_ = Vector3{ 360.0f * degToRad, confirmStartRot_.y, confirmStartRot_.z };
            confirmElapsed_ = 0.0f;
            // 確定アニメーション時間を0.5fに増加
            confirmDuration_ = 0.5f;
        }

        // 決定されていないモデルのX移動アニメを準備（上から下、0.05秒の遅延）
        const float baseXDuration = 0.3f;
        const float stagger = 0.05f;
        for (int i = 0; i < 4; ++i) {
            if (i == confirmedIndex_) continue;
            if (!models_[i]) continue;
            auto *tr = models_[i]->GetComponent3D<Transform3D>();
            xStart_[i] = tr->GetTranslate().x;
            xEnd_[i] = -16.0f;
            // 上（index 0）から下（index 3）の順で遅延を付与
            float delay = static_cast<float>(i) * stagger;
            // elapsedを負にして遅延を表現（負のelapsedはアニメ開始前の遅延）
            xElapsed_[i] = -delay; // negative elapsed acts as delay before animation starts
            xDuration_[i] = baseXDuration;
            xAnimating_[i] = true;
        }
    }

    //==================================================
    // アニメーション処理
    //==================================================

    const float dt = GetDeltaTime();

    // zアニメーションの更新
    for (int i = 0; i < 4; ++i) {
        if (!models_[i]) continue;
        if (zAnimating_[i]) {
            zElapsed_[i] += dt;
            float t = Normalize01(zElapsed_[i], 0.0f, zDuration_[i]);
            float eased = EaseOutCubic(zStart_[i], zEnd_[i], t);
            if (auto *tr = models_[i]->GetComponent3D<Transform3D>()) {
                Vector3 pos = tr->GetTranslate();
                pos.z = eased;
                tr->SetTranslate(pos);
            }
            if (t >= 1.0f) {
                zAnimating_[i] = false;
            }
        }

        // 選択に応じた色の更新
        if (auto *mat = models_[i]->GetComponent3D<Material3D>()) {
            if (i == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
            else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
        }
    }

    // X移動アニメーションの更新（決定後の非選択モデル用）
    for (int i = 0; i < 4; ++i) {
        if (!models_[i]) continue;
        if (xAnimating_[i]) {
            xElapsed_[i] += dt;
            float t = Normalize01(xElapsed_[i], 0.0f, xDuration_[i]);
            float easedX = EaseOutCubic(xStart_[i], xEnd_[i], t);
            if (auto *tr = models_[i]->GetComponent3D<Transform3D>()) {
                Vector3 pos = tr->GetTranslate();
                pos.x = easedX;
                tr->SetTranslate(pos);
            }
            if (t >= 1.0f) {
                xAnimating_[i] = false;
            }
        }
    }

    // 回転の更新：選択中のみサイン回転、それ以外は0にする。
    for (int i = 0; i < 4; ++i) {
        if (!models_[i]) continue;
        if (auto *tr = models_[i]->GetComponent3D<Transform3D>()) {
            if (!isSubmitted_) {
                if (i == selectedIndex_) {
                    // 選択中のみサイン回転を適用
                    rotSineTime_ += dt;
                    const float degToRad = M_PI / 180.0f;
                    const float amplitude = 15.0f * degToRad; // 15度をラジアンに変換
                    const float omega = 2.0f * static_cast<float>(M_PI) * rotSineFrequency_;
                    float sinVal = std::sin(rotSineTime_ * omega);
                    Vector3 rot = tr->GetRotate();
                    rot.x = sinVal * amplitude;
                    tr->SetRotate(rot);
                } else {
                    // 選択が外れたら回転を0に戻す
                    tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
                }
            }
        }
    }

    // 確定アニメーションの更新
    if (isConfirming_ && models_[confirmedIndex_]) {
        confirmElapsed_ += dt;
        float t = Normalize01(confirmElapsed_, 0.0f, confirmDuration_);
        auto *tr = models_[confirmedIndex_]->GetComponent3D<Transform3D>();
        Vector3 pos = EaseOutCubic(confirmStartPos_, confirmEndPos_, t);
        Vector3 rot = EaseOutCubic(confirmStartRot_, confirmEndRot_, t);
        tr->SetTranslate(pos);
        tr->SetRotate(rot);
        if (t >= 1.0f) {
            isConfirming_ = false;
        }
    }
}

} // namespace KashipanEngine
