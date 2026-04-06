#pragma once

#include <string>

#include "Assets/TextureManager.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/Sprite.h"

namespace KashipanEngine {

class ScreenBuffer;
class Window;

class SpriteProressBar final : public Object2DBase {
public:
    SpriteProressBar();
    ~SpriteProressBar() override = default;

    void SetProgress(float progress);
    float GetProgress() const { return progress_; }

    void SetBarSize(const Vector2 &barSize);
    const Vector2 &GetBarSize() const { return barSize_; }

    void SetFrameThickness(float frameThickness);
    float GetFrameThickness() const { return frameThickness_; }

    void SetFrameColor(const Vector4 &color);
    const Vector4 &GetFrameColor() const { return frameColor_; }

    void SetBarColor(const Vector4 &color);
    const Vector4 &GetBarColor() const { return barColor_; }

    void SetBackgroundColor(const Vector4 &color);
    const Vector4 &GetBackgroundColor() const { return backgroundColor_; }

    void SetFrameTexture(TextureManager::TextureHandle texture);
    TextureManager::TextureHandle GetFrameTexture() const { return frameTexture_; }

    void SetBarTexture(TextureManager::TextureHandle texture);
    TextureManager::TextureHandle GetBarTexture() const { return barTexture_; }

    void SetBackgroundTexture(TextureManager::TextureHandle texture);
    TextureManager::TextureHandle GetBackgroundTexture() const { return backgroundTexture_; }

    void AttachToRenderer(Window *targetWindow, const std::string &pipelineName);
    void AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName);
    void DetachFromRenderer();

protected:
    void OnUpdate() override;

private:
    void UpdateVisuals();
    void UpdateLayout();

    std::unique_ptr<Sprite> frameSprite_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> barSprite_;

    Transform2D *parentTransform_ = nullptr;

    float progress_ = 0.0f;
    Vector2 barSize_ = Vector2{256.0f, 24.0f};
    float frameThickness_ = 4.0f;

    Vector4 frameColor_ = Vector4{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 barColor_ = Vector4{0.2f, 0.85f, 0.3f, 1.0f};
    Vector4 backgroundColor_ = Vector4{0.08f, 0.08f, 0.08f, 0.8f};

    TextureManager::TextureHandle frameTexture_ = TextureManager::kInvalidHandle;
    TextureManager::TextureHandle barTexture_ = TextureManager::kInvalidHandle;
    TextureManager::TextureHandle backgroundTexture_ = TextureManager::kInvalidHandle;
};

} // namespace KashipanEngine
