#include "Objects/GameObjects/2D/SpriteProressBar.h"

#include <algorithm>
#include <cmath>

#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

SpriteProressBar::SpriteProressBar()
    : Object2DBase("SpriteProressBar") {
    parentTransform_ = GetComponent2D<Transform2D>();

    auto makeChild = [this](const std::string &name, int transformPriorityOffset) {
        auto sprite = std::make_unique<Sprite>();
        sprite->SetName(name);
        sprite->SetUniqueBatchKey();
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            tr->SetParentTransform(parentTransform_);
            // Ensure visual depth matching or order
            tr->SetTranslate(Vector3{0, 0, static_cast<float>(transformPriorityOffset)});
        }
        return sprite;
    };

    frameSprite_ = makeChild("SpriteProressBarFrame", 3);
    backgroundSprite_ = makeChild("SpriteProressBarBackground", 0);
    animationBarSprite_ = makeChild("SpriteProressBarAnimationBar", 1);
    barSprite_ = makeChild("SpriteProressBarBar", 2);
    SyncSegmentSprites();

    UpdateVisuals();
    UpdateLayout();
}

void SpriteProressBar::SetProgress(float progress) {
    float newTarget = std::clamp(progress, 0.0f, 1.0f);

    if (newTarget < targetProgress_) {
        currentProgress_ = newTarget;
    } else if (newTarget > targetProgress_) {
        animationProgress_ = newTarget;
    }

    targetProgress_ = newTarget;
    UpdateLayout();
}

void SpriteProressBar::SetBarSize(const Vector2 &barSize) {
    barSize_ = Vector2{std::max(0.0f, barSize.x), std::max(0.0f, barSize.y)};
    UpdateLayout();
}

void SpriteProressBar::SetFillDirection(FillDirection direction) {
    fillDirection_ = direction;
    UpdateLayout();
}

void SpriteProressBar::SetFrameThickness(float frameThickness) {
    frameThickness_ = std::max(0.0f, frameThickness);
    UpdateLayout();
}

void SpriteProressBar::SetFrameColor(const Vector4 &color) {
    frameColor_ = color;
    UpdateVisuals();
}

void SpriteProressBar::SetBarColor(const Vector4 &color) {
    barColor_ = color;
    UpdateVisuals();
}

void SpriteProressBar::SetAnimationBarColor(const Vector4 &color) {
    animationBarColor_ = color;
    UpdateVisuals();
}

void SpriteProressBar::SetBackgroundColor(const Vector4 &color) {
    backgroundColor_ = color;
    UpdateVisuals();
}

void SpriteProressBar::SetFrameTexture(TextureManager::TextureHandle texture) {
    frameTexture_ = texture;
    UpdateVisuals();
}

void SpriteProressBar::SetBarTexture(TextureManager::TextureHandle texture) {
    barTexture_ = texture;
    UpdateVisuals();
}

void SpriteProressBar::SetAnimationBarTexture(TextureManager::TextureHandle texture) {
    animationBarTexture_ = texture;
    UpdateVisuals();
}

void SpriteProressBar::SetBackgroundTexture(TextureManager::TextureHandle texture) {
    backgroundTexture_ = texture;
    UpdateVisuals();
}

void SpriteProressBar::SetSegmentLineCount(int count) {
    segmentLineCount_ = std::max(0, count);
    SyncSegmentSprites();
    UpdateVisuals();
    UpdateLayout();
}

void SpriteProressBar::SetSegmentLineColor(const Vector4 &color) {
    segmentLineColor_ = color;
    UpdateVisuals();
}

void SpriteProressBar::SetSegmentLineThickness(float thickness) {
    segmentLineThickness_ = std::max(0.0f, thickness);
    UpdateLayout();
}

void SpriteProressBar::AttachToRenderer(Window *targetWindow, const std::string &pipelineName) {
    attachedWindow_ = targetWindow;
    attachedBuffer_ = nullptr;
    attachedPipelineName_ = pipelineName;

    if (frameSprite_) frameSprite_->AttachToRenderer(targetWindow, pipelineName);
    if (backgroundSprite_) backgroundSprite_->AttachToRenderer(targetWindow, pipelineName);
    if (animationBarSprite_) animationBarSprite_->AttachToRenderer(targetWindow, pipelineName);
    if (barSprite_) barSprite_->AttachToRenderer(targetWindow, pipelineName);
    for (auto &s : segmentSprites_) {
        if (s) s->AttachToRenderer(targetWindow, pipelineName);
    }
}

void SpriteProressBar::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    attachedWindow_ = nullptr;
    attachedBuffer_ = targetBuffer;
    attachedPipelineName_ = pipelineName;

    if (frameSprite_) frameSprite_->AttachToRenderer(targetBuffer, pipelineName);
    if (backgroundSprite_) backgroundSprite_->AttachToRenderer(targetBuffer, pipelineName);
    if (animationBarSprite_) animationBarSprite_->AttachToRenderer(targetBuffer, pipelineName);
    if (barSprite_) barSprite_->AttachToRenderer(targetBuffer, pipelineName);
    for (auto &s : segmentSprites_) {
        if (s) s->AttachToRenderer(targetBuffer, pipelineName);
    }
}

void SpriteProressBar::DetachFromRenderer() {
    if (frameSprite_) frameSprite_->DetachFromRenderer();
    if (backgroundSprite_) backgroundSprite_->DetachFromRenderer();
    if (animationBarSprite_) animationBarSprite_->DetachFromRenderer();
    if (barSprite_) barSprite_->DetachFromRenderer();
    for (auto &s : segmentSprites_) {
        if (s) s->DetachFromRenderer();
    }

    attachedWindow_ = nullptr;
    attachedBuffer_ = nullptr;
    attachedPipelineName_.clear();
}

void SpriteProressBar::OnUpdate() {
    float dt = GetDeltaTime();
    float t = std::clamp(3.0f * dt * GetGameSpeed(), 0.0f, 1.0f);
    bool changed = false;

    // 減る場合はアニメーションバーがゆっくり targetProgress に追従
    if (animationProgress_ > targetProgress_) {
        animationProgress_ = std::lerp(animationProgress_, targetProgress_, t);
        changed = true;
    }

    // 増える場合はメインのバーがゆっくり targetProgress に追従
    if (currentProgress_ < targetProgress_) {
        currentProgress_ = std::lerp(currentProgress_, targetProgress_, t);
        changed = true;
    }

    if (changed) {
        UpdateLayout();
    }

    if (frameSprite_) frameSprite_->Update();
    if (backgroundSprite_) backgroundSprite_->Update();
    if (animationBarSprite_) animationBarSprite_->Update();
    if (barSprite_) barSprite_->Update();
    for (auto &s : segmentSprites_) {
        if (s) s->Update();
    }
}

void SpriteProressBar::UpdateVisuals() {
    if (frameSprite_) {
        if (auto *mat = frameSprite_->GetComponent2D<Material2D>()) {
            mat->SetColor(frameColor_);
            mat->SetTexture(frameTexture_);
        }
    }

    if (backgroundSprite_) {
        if (auto *mat = backgroundSprite_->GetComponent2D<Material2D>()) {
            mat->SetColor(backgroundColor_);
            mat->SetTexture(backgroundTexture_);
        }
    }

    if (animationBarSprite_) {
        if (auto *mat = animationBarSprite_->GetComponent2D<Material2D>()) {
            mat->SetColor(animationBarColor_);
            mat->SetTexture(animationBarTexture_);
        }
    }

    if (barSprite_) {
        if (auto *mat = barSprite_->GetComponent2D<Material2D>()) {
            mat->SetColor(barColor_);
            mat->SetTexture(barTexture_);
        }
    }

    for (auto &s : segmentSprites_) {
        if (!s) continue;
        if (auto *mat = s->GetComponent2D<Material2D>()) {
            mat->SetColor(segmentLineColor_);
            mat->SetTexture(TextureManager::kInvalidHandle);
        }
    }
}

void SpriteProressBar::UpdateLayout() {
    const float barWidth = std::max(0.0f, barSize_.x);
    const float barHeight = std::max(0.0f, barSize_.y);

    if (frameSprite_) {
        if (auto *tr = frameSprite_->GetComponent2D<Transform2D>()) {
            // Z値を変えないために x, y のみ変更。必要であれば GetTranslate を使用
            Vector3 pos = tr->GetTranslate();
            pos.x = 0.0f;
            pos.y = 0.0f;
            tr->SetTranslate(pos);
            tr->SetScale(Vector3{barWidth + frameThickness_ * 2.0f, barHeight + frameThickness_ * 2.0f, 1.0f});
        }
    }

    if (backgroundSprite_) {
        if (auto *tr = backgroundSprite_->GetComponent2D<Transform2D>()) {
            Vector3 pos = tr->GetTranslate();
            pos.x = 0.0f;
            pos.y = 0.0f;
            tr->SetTranslate(pos);
            tr->SetScale(Vector3{barWidth, barHeight, 1.0f});
        }
    }

    auto updateBarSprite = [&](Sprite *sprite, float p) {
        if (!sprite) return;
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            float barPosX = 0.0f;
            float barPosY = 0.0f;
            float barScaleX = barWidth;
            float barScaleY = barHeight;
            Vector3 pos = tr->GetTranslate(); // Reserve Z position

            switch (fillDirection_) {
            case FillDirection::LeftToRight:
                sprite->SetPivotPoint(0.0f, 0.5f);
                barPosX = -barWidth * 0.5f;
                barScaleX = barWidth * p;
                break;
            case FillDirection::RightToLeft:
                sprite->SetPivotPoint(1.0f, 0.5f);
                barPosX = barWidth * 0.5f;
                barScaleX = barWidth * p;
                break;
            case FillDirection::BottomToTop:
                sprite->SetPivotPoint(0.5f, 1.0f);
                barPosY = -barHeight * 0.5f;
                barScaleY = barHeight * p;
                break;
            case FillDirection::TopToBottom:
                sprite->SetPivotPoint(0.5f, 0.0f);
                barPosY = barHeight * 0.5f;
                barScaleY = barHeight * p;
                break;
            }

            pos.x = barPosX;
            pos.y = barPosY;
            tr->SetTranslate(pos);
            tr->SetScale(Vector3{barScaleX, barScaleY, 1.0f});
        }
    };

    updateBarSprite(animationBarSprite_.get(), animationProgress_);
    updateBarSprite(barSprite_.get(), currentProgress_);

    if (!segmentSprites_.empty() && segmentLineCount_ > 0) {
        if (fillDirection_ == FillDirection::LeftToRight || fillDirection_ == FillDirection::RightToLeft) {
            const float step = barWidth / static_cast<float>(segmentLineCount_ + 1);
            for (int i = 0; i < segmentLineCount_; ++i) {
                auto *sprite = segmentSprites_[static_cast<std::size_t>(i)].get();
                if (!sprite) continue;
                if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
                    const float x = -barWidth * 0.5f + step * static_cast<float>(i + 1);
                    Vector3 pos = tr->GetTranslate();
                    pos.x = x;
                    pos.y = 0.0f;
                    tr->SetTranslate(pos);
                    tr->SetScale(Vector3{segmentLineThickness_, barHeight, 1.0f});
                }
            }
        } else {
            const float step = barHeight / static_cast<float>(segmentLineCount_ + 1);
            for (int i = 0; i < segmentLineCount_; ++i) {
                auto *sprite = segmentSprites_[static_cast<std::size_t>(i)].get();
                if (!sprite) continue;
                if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
                    const float y = -barHeight * 0.5f + step * static_cast<float>(i + 1);
                    Vector3 pos = tr->GetTranslate();
                    pos.x = 0.0f;
                    pos.y = y;
                    tr->SetTranslate(pos);
                    tr->SetScale(Vector3{barWidth, segmentLineThickness_, 1.0f});
                }
            }
        }
    }
}

void SpriteProressBar::SyncSegmentSprites() {
    const std::size_t targetCount = static_cast<std::size_t>(segmentLineCount_);
    if (segmentSprites_.size() > targetCount) {
        segmentSprites_.resize(targetCount);
    }

    while (segmentSprites_.size() < targetCount) {
        auto sprite = std::make_unique<Sprite>();
        sprite->SetName("SpriteProressBarSegment");
        sprite->SetUniqueBatchKey();
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            tr->SetParentTransform(parentTransform_);
            // 線の描画優先順位を最も高く設定
            tr->SetTranslate(Vector3{0, 0, 4.0f});
        }

        if (!attachedPipelineName_.empty()) {
            if (attachedBuffer_) {
                sprite->AttachToRenderer(attachedBuffer_, attachedPipelineName_);
            } else if (attachedWindow_) {
                sprite->AttachToRenderer(attachedWindow_, attachedPipelineName_);
            }
        }

        segmentSprites_.push_back(std::move(sprite));
    }
}

} // namespace KashipanEngine
