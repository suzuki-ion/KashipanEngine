#include "Objects/GameObjects/2D/SpriteProressBar.h"

#include <algorithm>

#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"

namespace KashipanEngine {

SpriteProressBar::SpriteProressBar()
    : Object2DBase("SpriteProressBar") {
    parentTransform_ = GetComponent2D<Transform2D>();

    auto makeChild = [this](const std::string &name) {
        auto sprite = std::make_unique<Sprite>();
        sprite->SetName(name);
        sprite->SetUniqueBatchKey();
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            tr->SetParentTransform(parentTransform_);
        }
        return sprite;
    };

    frameSprite_ = makeChild("SpriteProressBarFrame");
    backgroundSprite_ = makeChild("SpriteProressBarBackground");
    barSprite_ = makeChild("SpriteProressBarBar");

    if (barSprite_) {
        barSprite_->SetPivotPoint(0.0f, 0.5f);
    }

    UpdateVisuals();
    UpdateLayout();
}

void SpriteProressBar::SetProgress(float progress) {
    progress_ = std::clamp(progress, 0.0f, 1.0f);
    UpdateLayout();
}

void SpriteProressBar::SetBarSize(const Vector2 &barSize) {
    barSize_ = Vector2{std::max(0.0f, barSize.x), std::max(0.0f, barSize.y)};
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

void SpriteProressBar::SetBackgroundTexture(TextureManager::TextureHandle texture) {
    backgroundTexture_ = texture;
    UpdateVisuals();
}

void SpriteProressBar::AttachToRenderer(Window *targetWindow, const std::string &pipelineName) {
    if (frameSprite_) frameSprite_->AttachToRenderer(targetWindow, pipelineName);
    if (backgroundSprite_) backgroundSprite_->AttachToRenderer(targetWindow, pipelineName);
    if (barSprite_) barSprite_->AttachToRenderer(targetWindow, pipelineName);
}

void SpriteProressBar::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    if (frameSprite_) frameSprite_->AttachToRenderer(targetBuffer, pipelineName);
    if (backgroundSprite_) backgroundSprite_->AttachToRenderer(targetBuffer, pipelineName);
    if (barSprite_) barSprite_->AttachToRenderer(targetBuffer, pipelineName);
}

void SpriteProressBar::DetachFromRenderer() {
    if (frameSprite_) frameSprite_->DetachFromRenderer();
    if (backgroundSprite_) backgroundSprite_->DetachFromRenderer();
    if (barSprite_) barSprite_->DetachFromRenderer();
}

void SpriteProressBar::OnUpdate() {
    if (frameSprite_) frameSprite_->Update();
    if (backgroundSprite_) backgroundSprite_->Update();
    if (barSprite_) barSprite_->Update();
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

    if (barSprite_) {
        if (auto *mat = barSprite_->GetComponent2D<Material2D>()) {
            mat->SetColor(barColor_);
            mat->SetTexture(barTexture_);
        }
    }
}

void SpriteProressBar::UpdateLayout() {
    const float barWidth = std::max(0.0f, barSize_.x);
    const float barHeight = std::max(0.0f, barSize_.y);

    if (frameSprite_) {
        if (auto *tr = frameSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{barWidth + frameThickness_ * 2.0f, barHeight + frameThickness_ * 2.0f, 1.0f});
        }
    }

    if (backgroundSprite_) {
        if (auto *tr = backgroundSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{barWidth, barHeight, 1.0f});
        }
    }

    if (barSprite_) {
        if (auto *tr = barSprite_->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{-barWidth * 0.5f, 0.0f, 0.0f});
            tr->SetScale(Vector3{barWidth * progress_, barHeight, 1.0f});
        }
    }
}

} // namespace KashipanEngine
