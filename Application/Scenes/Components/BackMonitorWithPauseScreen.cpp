#include "BackMonitorWithPauseScreen.h"
#include "BackMonitor.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include <algorithm>
#include <cmath>

namespace KashipanEngine {

BackMonitorWithPauseScreen::BackMonitorWithPauseScreen(ScreenBuffer *target, InputCommand *inputCommand)
    : BackMonitorRenderer("BackMonitorWithPauseScreen", target), inputCommand_(inputCommand) {}

BackMonitorWithPauseScreen::~BackMonitorWithPauseScreen() {}

void BackMonitorWithPauseScreen::Initialize() {
    auto target = GetTargetScreenBuffer();
    if (!target) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

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
        basePositions_.assign(modelCount_, Vector3{ 0.0f, 0.0f, 0.0f });
        baseRotations_.assign(modelCount_, Vector3{ 0.0f, 0.0f, 0.0f });
        isInitialized_ = true;

        soundHandleSelect_ = AudioManager::GetSoundHandleFromFileName("select.mp3");
        soundHandleSubmit_ = AudioManager::GetSoundHandleFromFileName("submit.mp3");
    }

    const float centerX = 0.0f;
    const float topY = 2.0f;
    const float bottomY = -2.0f;
    const float spacing = (topY - bottomY) / static_cast<float>(modelCount_ - 1);
    const float depth = 2.5f;
    const Vector3 scaleVec{ 1.0f, 1.0f, 1.0f };

    if (!menuContinue_) {
        //auto modelHandle = ModelManager::GetModelDataFromFileName("menuContinue.obj");
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuStart.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.PauseContinue");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        if (models_.size() > static_cast<size_t>(PauseModelIndex::Continue)) {
            models_[static_cast<size_t>(PauseModelIndex::Continue)] = obj.get();
        }
        menuContinue_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuContinue_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 0, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(PauseModelIndex::Continue)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(PauseModelIndex::Continue)] = tr->GetRotate();
    }
    if (auto *mat = menuContinue_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    if (!menuMenu_) {
        //auto modelHandle = ModelManager::GetModelDataFromFileName("menuMenu.obj");
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuStart.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.PauseMenu");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        if (models_.size() > static_cast<size_t>(PauseModelIndex::Menu)) {
            models_[static_cast<size_t>(PauseModelIndex::Menu)] = obj.get();
        }
        menuMenu_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuMenu_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 1, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(PauseModelIndex::Menu)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(PauseModelIndex::Menu)] = tr->GetRotate();
    }
    if (auto *mat = menuMenu_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    if (!menuTitle_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuTitle.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.PauseTitle");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        if (models_.size() > static_cast<size_t>(PauseModelIndex::Title)) {
            models_[static_cast<size_t>(PauseModelIndex::Title)] = obj.get();
        }
        menuTitle_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuTitle_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 2, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(PauseModelIndex::Title)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(PauseModelIndex::Title)] = tr->GetRotate();
    }
    if (auto *mat = menuTitle_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    if (!menuQuit_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("menuQuit.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor.PauseQuit");
        obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");
        if (models_.size() > static_cast<size_t>(PauseModelIndex::Quit)) {
            models_[static_cast<size_t>(PauseModelIndex::Quit)] = obj.get();
        }
        menuQuit_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
    if (auto *tr = menuQuit_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3{ centerX, topY - spacing * 3, depth });
        tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetScale(scaleVec);
        basePositions_[static_cast<size_t>(PauseModelIndex::Quit)] = tr->GetTranslate();
        baseRotations_[static_cast<size_t>(PauseModelIndex::Quit)] = tr->GetRotate();
    }
    if (auto *mat = menuQuit_->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
        mat->SetEnableShadowMapProjection(false);
    }

    isSubmitted_ = false;
    isConfirming_ = false;
    isConfirmed_ = false;
    isConfirmedTriggerd_ = false;
    selectedIndex_ = 0;
    rotSineTime_ = 0.0f;

    size_t idx = 0;
    for (const auto &m : models_) {
        if (m) {
            if (auto *mat = m->GetComponent3D<Material3D>()) {
                if (static_cast<int>(idx) == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
                else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
            }
            if (auto *tr = m->GetComponent3D<Transform3D>()) {
                Vector3 p = tr->GetTranslate();
                p.x = 0.0f;
                tr->SetTranslate(p);
            }
        }

        xAnimating_[idx] = false;
        xElapsed_[idx] = 0.0f;
        xDuration_[idx] = 0.0f;
        ++idx;
    }
}

void BackMonitorWithPauseScreen::Update() {
    if (!IsActive()) {
        if (wasActive_) {
            Initialize();
        }
        for (const auto &m : models_) {
            if (!m) continue;
            if (auto *mat = m->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 0.0f,0.0f,0.0f,0.0f });
            }
        }
        wasActive_ = false;
        return;
    }

    if (!wasActive_) {
        wasActive_ = true;
        const float baseXDuration = 0.3f;
        const float stagger = 0.05f;
        for (size_t idx = 0; idx < models_.size(); ++idx) {
            auto *m = models_[idx];
            if (!m) continue;
            auto *tr = m->GetComponent3D<Transform3D>();
            if (!tr) continue;
            const float startX = -16.0f;
            Vector3 pos = tr->GetTranslate();
            pos.x = startX;
            tr->SetTranslate(pos);

            xStart_[idx] = startX;
            xEnd_[idx] = basePositions_[idx].x;
            xElapsed_[idx] = -static_cast<float>(idx) * stagger;
            xDuration_[idx] = baseXDuration;
            xAnimating_[idx] = true;
        }
    }

    if (!GetBackMonitor() || !GetBackMonitor()->IsReady()) return;

    const float animDuration = 0.2f;

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
            } else {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.5f;
            }
            zElapsed_[idx] = 0.0f;
            zDuration_[idx] = animDuration;
            zAnimating_[idx] = true;
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
            } else {
                zStart_[idx] = m->GetComponent3D<Transform3D>()->GetTranslate().z;
                zEnd_[idx] = 2.5f;
            }
            zElapsed_[idx] = 0.0f;
            zDuration_[idx] = animDuration;
            zAnimating_[idx] = true;
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
        isMenuConfirmSliding_ = false;

        if (models_[confirmedIndex_]) {
            auto *tr = models_[confirmedIndex_]->GetComponent3D<Transform3D>();
            confirmStartPos_ = tr->GetTranslate();
            confirmEndPos_ = Vector3{ 0.0f, 0.0f, 2.0f };
            confirmStartRot_ = tr->GetRotate();
            const float degToRad = M_PI / 180.0f;
            confirmEndRot_ = Vector3{ 360.0f * degToRad, confirmStartRot_.y, confirmStartRot_.z };
            confirmElapsed_ = 0.0f;
            confirmDuration_ = 0.5f;
        }

        const float baseXDuration = 0.3f;
        const float stagger = 0.05f;
        size_t idx = 0;
        for (const auto &m : models_) {
            if (static_cast<int>(idx) == confirmedIndex_) { ++idx; continue; }
            if (!m) { ++idx; continue; }
            auto *tr = m->GetComponent3D<Transform3D>();
            xStart_[idx] = tr->GetTranslate().x;
            xEnd_[idx] = -16.0f;
            float delay = static_cast<float>(idx) * stagger;
            xElapsed_[idx] = -delay;
            xDuration_[idx] = baseXDuration;
            xAnimating_[idx] = true;
            ++idx;
        }
    }

    const float dt = GetDeltaTime();

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

        if (auto *mat = m->GetComponent3D<Material3D>()) {
            if (static_cast<int>(idx) == selectedIndex_) mat->SetColor(Vector4{ 1.0f,1.0f,1.0f,1.0f });
            else mat->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
        }
        ++idx;
    }

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

    idx = 0;
    for (const auto &m : models_) {
        if (!m) { ++idx; continue; }
        if (auto *tr = m->GetComponent3D<Transform3D>()) {
            if (!isSubmitted_) {
                if (static_cast<int>(idx) == selectedIndex_) {
                    rotSineTime_ += dt;
                    const float degToRad = M_PI / 180.0f;
                    const float amplitude = 15.0f * degToRad;
                    const float omega = 2.0f * static_cast<float>(M_PI) * rotSineFrequency_;
                    float sinVal = std::sin(rotSineTime_ * omega);
                    Vector3 rot = tr->GetRotate();
                    rot.x = sinVal * amplitude;
                    tr->SetRotate(rot);
                } else {
                    tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
                }
            }
        }
        ++idx;
    }

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
            if (confirmedIndex_ == static_cast<int>(PauseModelIndex::Menu)) {
                const float baseXDuration = 0.3f;
                auto *menuTr = models_[confirmedIndex_]->GetComponent3D<Transform3D>();
                xStart_[confirmedIndex_] = menuTr->GetTranslate().x;
                xEnd_[confirmedIndex_] = -16.0f;
                xElapsed_[confirmedIndex_] = 0.0f;
                xDuration_[confirmedIndex_] = baseXDuration;
                xAnimating_[confirmedIndex_] = true;
                isMenuConfirmSliding_ = true;
            } else {
                isConfirmed_ = true;
            }
        }
    }

    if (isMenuConfirmSliding_) {
        if (!xAnimating_[confirmedIndex_]) {
            isMenuConfirmSliding_ = false;
            isConfirmed_ = true;
        }
    }

    if (isSubmitted_ && !isConfirming_ && !isMenuConfirmSliding_) {
        isConfirmed_ = true;
    }
}

} // namespace KashipanEngine
