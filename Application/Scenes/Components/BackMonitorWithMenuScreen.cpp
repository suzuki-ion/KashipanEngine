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

    // Set model count and resize vectors
    if (!isInitialized_) {
        modelCount_ = 4;
        models_.assign(modelCount_, nullptr);
        zStart_.assign(modelCount_, 2.5f);
        zEnd_.assign(modelCount_, 2.5f);
        zElapsed_.assign(modelCount_, 0.0f);
        zDuration_.assign(modelCount_, 0.0f);
        zAnimating_.assign(modelCount_, false);
        rotOffsetX_.assign(modelCount_, 0.0f);
        xStart_.assign(modelCount_, 0.0f);
        xEnd_.assign(modelCount_, 0.0f);
        xElapsed_.assign(modelCount_, 0.0f);
        xDuration_.assign(modelCount_, 0.0f);
        xAnimating_.assign(modelCount_, false);
        basePositions_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        baseRotations_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        returnStartPos_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        returnEndPos_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        returnStartRot_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        returnEndRot_.assign(modelCount_, Vector3{0.0f, 0.0f, 0.0f});
        returnElapsed_.assign(modelCount_, 0.0f);
        returnDuration_.assign(modelCount_, 0.0f);
        returnAnimating_.assign(modelCount_, false);
        isInitialized_ = true;

        // Load sound handles
        soundHandleSelect_ = AudioManager::GetSoundHandleFromFileName("select.mp3");
        soundHandleSubmit_ = AudioManager::GetSoundHandleFromFileName("submit.mp3");
        soundHandleCancel_ = AudioManager::GetSoundHandleFromFileName("cancel.mp3");
    }

    const float centerX = 0.0f;
    const float topY = 2.0f;
    const float bottomY = -2.0f;
    const float spacing = (topY - bottomY) / static_cast<float>(modelCount_ - 1);
    const float depth = 2.5f;
    const Vector3 scaleVec{ 1.0f, 1.0f, 1.0f };

    // Start（インデックス0）-> 上
    if (!menuStart_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuStart.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.MenuStart");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        if (models_.size() > static_cast<size_t>(MenuModelIndex::Start))
            models_[static_cast<size_t>(MenuModelIndex::Start)] = obj.get();
        menuStart_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuStart_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 0, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(MenuModelIndex::Start)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(MenuModelIndex::Start)] = tr->GetRotate();
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
        if (models_.size() > static_cast<size_t>(MenuModelIndex::Credit))
            models_[static_cast<size_t>(MenuModelIndex::Credit)] = obj.get();
        menuCredit_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuCredit_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 1, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(MenuModelIndex::Credit)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(MenuModelIndex::Credit)] = tr->GetRotate();
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
        if (models_.size() > static_cast<size_t>(MenuModelIndex::Title))
            models_[static_cast<size_t>(MenuModelIndex::Title)] = obj.get();
        menuTitle_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuTitle_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 2, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(MenuModelIndex::Title)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(MenuModelIndex::Title)] = tr->GetRotate();
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
        if (models_.size() > static_cast<size_t>(MenuModelIndex::Quit))
            models_[static_cast<size_t>(MenuModelIndex::Quit)] = obj.get();
        menuQuit_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuQuit_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 3, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(MenuModelIndex::Quit)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(MenuModelIndex::Quit)] = tr->GetRotate();
    }
    if (auto *mat = menuQuit_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    isSubmitted_ = false;
    isConfirming_ = false;
    isConfirmed_ = false;
    isConfirmedTriggerd_ = false;
    isCreditMoving_ = false;
    isCreditMoved_ = false;
    isReturning_ = false;
    selectedIndex_ = 0;
    // サイン波タイマーをリセット
    rotSineTime_ = 0.0f;

    size_t idx = 0;
    for (const auto &m : models_) {
        if (m) {
            if (auto *mat = m->GetComponent3D<Material3D>()) {
                if (static_cast<int>(idx) == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
                else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
            }
            // 初期のX座標を0にする
            if (auto *tr = m->GetComponent3D<Transform3D>()) {
                Vector3 p = tr->GetTranslate();
                p.x = 0.0f;
                tr->SetTranslate(p);
            }
        }

        xAnimating_[idx] = false;
        xElapsed_[idx] = 0.0f;
        xDuration_[idx] = 0.0f;
        returnAnimating_[idx] = false;
        returnElapsed_[idx] = 0.0f;
        returnDuration_[idx] = 0.0f;
        ++idx;
    }
}

void BackMonitorWithMenuScreen::Update() {
    static bool preIsActive = false;
    if (!IsActive()) {
        for (const auto &m : models_) {
            if (!m) continue;
            if (auto *mat = m->GetComponent3D<Material3D>()) {
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

    const float animDuration = 0.2f; // zアニメーションは短めに保つ

    //==================================================
    // 入力処理
    //==================================================

    if (!inputCommand_) return;

    if (!isSubmitted_ && inputCommand_->Evaluate("MoveUp").Triggered()) {
        selectedIndex_ = (selectedIndex_ - 1 + modelCount_) % modelCount_;
        AudioManager::Play(soundHandleSelect_, 1.0f, 0.0f, false);
        size_t idx = 0;
        for (const auto &m : models_) {
            if (!m) { ++idx; continue; }
            if (static_cast<int>(idx) == selectedIndex_) {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.0f;
                zElapsed_[idx] = 0.0f;
                zDuration_[idx] = animDuration;
                zAnimating_[idx] = true;
            } else {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.5f;
                zElapsed_[idx] = 0.0f;
                zDuration_[idx] = animDuration;
                zAnimating_[idx] = true;
            }
            ++idx;
        }
    }
    if (!isSubmitted_ && inputCommand_->Evaluate("MoveDown").Triggered()) {
        selectedIndex_ = (selectedIndex_ + 1) % modelCount_;
        AudioManager::Play(soundHandleSelect_, 1.0f, 0.0f, false);
        size_t idx = 0;
        for (const auto &m : models_) {
            if (!m) { ++idx; continue; }
            if (static_cast<int>(idx) == selectedIndex_) {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.0f;
                zElapsed_[idx] = 0.0f;
                zDuration_[idx] = animDuration;
                zAnimating_[idx] = true;
            } else {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.5f;
                zElapsed_[idx] = 0.0f;
                zDuration_[idx] = animDuration;
                zAnimating_[idx] = true;
            }
            ++idx;
        }
    }
    if (!isSubmitted_ && inputCommand_->Evaluate("Submit").Triggered()) {
        AudioManager::Play(soundHandleSubmit_, 1.0f, 0.0f, false);
        isSubmitted_ = true;
        confirmedIndex_ = selectedIndex_;
        isConfirming_ = true;
        isConfirmed_ = false;
        isConfirmedTriggerd_ = false;
        isCreditMoving_ = false;
        isCreditMoved_ = false;
        isReturning_ = false;
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
        size_t idx = 0;
        for (const auto &m : models_) {
            if (static_cast<int>(idx) == confirmedIndex_) { ++idx; continue; }
            if (!m) { ++idx; continue; }
            auto *tr = m->GetComponent3D<Transform3D>();
            xStart_[idx] = tr->GetTranslate().x;
            xEnd_[idx] = -16.0f;
            // 上（index 0）から下（index 3）の順で遅延を付与
            float delay = static_cast<float>(idx) * stagger;
            // elapsedを負にして遅延を表現（負のelapsedはアニメ開始前の遅延）
            xElapsed_[idx] = -delay; // negative elapsed acts as delay before animation starts
            xDuration_[idx] = baseXDuration;
            xAnimating_[idx] = true;
            ++idx;
        }
    }

    if (isSubmitted_ && confirmedIndex_ == static_cast<int>(MenuModelIndex::Credit) && isCreditMoved_ && !isReturning_
        && inputCommand_->Evaluate("Submit").Triggered()) {
        AudioManager::Play(soundHandleCancel_, 1.0f, 0.0f, false);
        const float returnDuration = 0.5f;
        const float stagger = 0.05f;
        const float degToRad = M_PI / 180.0f;
        isReturning_ = true;
        isConfirmed_ = false;
        isConfirmedTriggerd_ = false;
        size_t idx = 0;
        for (const auto &m : models_) {
            if (!m) { ++idx; continue; }
            auto *tr = m->GetComponent3D<Transform3D>();
            returnStartPos_[idx] = tr->GetTranslate();
            returnEndPos_[idx] = basePositions_[idx];
            returnStartRot_[idx] = tr->GetRotate();
            returnEndRot_[idx] = baseRotations_[idx];
            if (static_cast<int>(idx) == static_cast<int>(MenuModelIndex::Quit)) {
                returnEndRot_[idx].x = baseRotations_[idx].x + 360.0f * degToRad;
            }
            float delay = 0.0f;
            if (static_cast<int>(idx) == static_cast<int>(MenuModelIndex::Title)) {
                delay = stagger;
            } else if (static_cast<int>(idx) == static_cast<int>(MenuModelIndex::Quit)) {
                delay = stagger * 2.0f;
            }
            returnElapsed_[idx] = -delay;
            returnDuration_[idx] = returnDuration;
            returnAnimating_[idx] = true;
            xAnimating_[idx] = false;
            ++idx;
        }
    }

    //==================================================
    // アニメーション処理
    //==================================================

    const float dt = GetDeltaTime();

    // zアニメーションの更新
    size_t idx = 0;
    for (const auto &m : models_) {
        if (!m) { ++idx; continue; }
        if (zAnimating_[idx]) {
            zElapsed_[idx] += dt;
            float t = Normalize01(zElapsed_[idx], 0.0f, zDuration_[idx]);
            float eased = EaseOutCubic(zStart_[idx], zEnd_[idx], t);
            if (auto *tr = m->GetComponent3D<Transform3D>()) {
                Vector3 pos = tr->GetTranslate();
                pos.z = eased;
                tr->SetTranslate(pos);
            }
            if (t >= 1.0f) {
                zAnimating_[idx] = false;
            }
        }

        // 選択に応じた色の更新
        if (auto *mat = m->GetComponent3D<Material3D>()) {
            if (static_cast<int>(idx) == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
            else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
        }
        ++idx;
    }

    // X移動アニメーションの更新（決定後の非選択モデル用）
    idx = 0;
    for (const auto &m : models_) {
        if (!m) { ++idx; continue; }
        if (xAnimating_[idx]) {
            xElapsed_[idx] += dt;
            float t = Normalize01(xElapsed_[idx], 0.0f, xDuration_[idx]);
            float easedX = EaseOutCubic(xStart_[idx], xEnd_[idx], t);
            if (auto *tr = m->GetComponent3D<Transform3D>()) {
                Vector3 pos = tr->GetTranslate();
                pos.x = easedX;
                tr->SetTranslate(pos);
            }
            if (t >= 1.0f) {
                xAnimating_[idx] = false;
            }
        }
        ++idx;
    }

    // 回転の更新：選択中のみサイン回転、それ以外は0にする。
    idx = 0;
    for (const auto &m : models_) {
        if (!m) { ++idx; continue; }
        if (auto *tr = m->GetComponent3D<Transform3D>()) {
            if (!isSubmitted_) {
                if (static_cast<int>(idx) == selectedIndex_) {
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
        ++idx;
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
            if (confirmedIndex_ == static_cast<int>(MenuModelIndex::Credit)) {
                creditMoveStartPos_ = tr->GetTranslate();
                creditMoveEndPos_ = Vector3{ creditMoveStartPos_.x, 2.0f, creditMoveStartPos_.z };
                creditMoveElapsed_ = 0.0f;
                creditMoveDuration_ = 0.5f;
                isCreditMoving_ = true;
            } else {
                isConfirmed_ = true;
            }
        }
    }

    if (isCreditMoving_ && confirmedIndex_ == static_cast<int>(MenuModelIndex::Credit) && models_[confirmedIndex_]) {
        creditMoveElapsed_ += dt;
        float t = Normalize01(creditMoveElapsed_, 0.0f, creditMoveDuration_);
        auto *tr = models_[confirmedIndex_]->GetComponent3D<Transform3D>();
        Vector3 pos = EaseOutCubic(creditMoveStartPos_, creditMoveEndPos_, t);
        tr->SetTranslate(pos);
        if (t >= 1.0f) {
            isCreditMoving_ = false;
            isCreditMoved_ = true;
            isConfirmed_ = true;
        }
    }

    bool hasReturnAnimating = false;
    if (isReturning_) {
        size_t returnIdx = 0;
        for (const auto &m : models_) {
            if (!m) { ++returnIdx; continue; }
            if (returnAnimating_[returnIdx]) {
                returnElapsed_[returnIdx] += dt;
                float t = Normalize01(returnElapsed_[returnIdx], 0.0f, returnDuration_[returnIdx]);
                Vector3 pos = EaseOutCubic(returnStartPos_[returnIdx], returnEndPos_[returnIdx], t);
                Vector3 rot = EaseOutCubic(returnStartRot_[returnIdx], returnEndRot_[returnIdx], t);
                if (auto *tr = m->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(pos);
                    tr->SetRotate(rot);
                }
                if (t >= 1.0f) {
                    returnAnimating_[returnIdx] = false;
                }
            }
            if (returnAnimating_[returnIdx]) {
                hasReturnAnimating = true;
            }
            ++returnIdx;
        }
        if (!hasReturnAnimating) {
            isReturning_ = false;
            isSubmitted_ = false;
            isConfirmed_ = false;
            isConfirmedTriggerd_ = false;
            isCreditMoved_ = false;
            confirmedIndex_ = -1;
        }
    }

    if (isSubmitted_ && !isConfirming_ && confirmedIndex_ != static_cast<int>(MenuModelIndex::Credit) && !isReturning_) {
        isConfirmed_ = true;
    }
}

void BackMonitorWithMenuScreen::MenuCreditUpdate() {
}

} // namespace KashipanEngine
