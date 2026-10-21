#include "GifPlayer.h"

#include <string>

#include "Graphics/GifTexture.h"

namespace KashipanEngine {

GifPlayer::GifPlayer(DirectXCommon *directXCommon, GifManager::GifHandle handle)
    : directXCommon_(directXCommon), handle_(handle) {
}

GifPlayer::~GifPlayer() {
    if (textureHandle_ != TextureManager::kInvalidHandle) {
        TextureManager::UnregisterExternalTexture(textureHandle_);
    }
}

bool GifPlayer::IsValid() const noexcept {
    const auto *animation = GifManager::GetGifAnimation(handle_);
    return animation && !animation->frames.empty();
}

bool GifPlayer::EnsureTexture() {
    const auto *animation = GifManager::GetGifAnimation(handle_);
    if (!animation || animation->frames.empty()) return false;

    // 想定と異なるサイズのテクスチャが残っている場合（通常は起こらないが念のため）は作り直す
    if (texture_ && (texture_->GetWidth() != animation->width || texture_->GetHeight() != animation->height)) {
        if (textureHandle_ != TextureManager::kInvalidHandle) {
            TextureManager::UnregisterExternalTexture(textureHandle_);
            textureHandle_ = TextureManager::kInvalidHandle;
        }
        texture_.reset();
    }
    if (texture_) return true;

    texture_ = GifManager::CreateGifTexture(Passkey<GifPlayer>{}, handle_);
    if (!texture_) return false;

    textureHandle_ = TextureManager::RegisterExternalTexture(
        "__GifPlayerTexture_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)), texture_.get());
    if (textureHandle_ == TextureManager::kInvalidHandle) {
        texture_.reset();
        return false;
    }
    return true;
}

void GifPlayer::UploadCurrentFrame() {
    const auto *animation = GifManager::GetGifAnimation(handle_);
    if (!animation || !texture_ || currentFrameIndex_ >= animation->frames.size()) return;
    const auto &frame = animation->frames[currentFrameIndex_];
    texture_->UploadFrame(frame.rgba.data(), frame.rgba.size());
}

bool GifPlayer::Play(bool loop) {
    if (!EnsureTexture()) return false;

    loop_ = loop;
    currentFrameIndex_ = 0;
    frameElapsed_ = 0.0f;
    isPlaying_ = true;
    isPaused_ = false;
    UploadCurrentFrame();
    return true;
}

void GifPlayer::Stop() {
    isPlaying_ = false;
    isPaused_ = false;
    currentFrameIndex_ = 0;
    frameElapsed_ = 0.0f;
    if (texture_) UploadCurrentFrame();
}

bool GifPlayer::Pause() {
    if (!isPlaying_) return false;
    isPaused_ = true;
    return true;
}

bool GifPlayer::Resume() {
    if (!isPlaying_ || !isPaused_) return false;
    isPaused_ = false;
    return true;
}

bool GifPlayer::ShowFirstFrame() {
    if (isPlaying_) return true;
    if (!EnsureTexture()) return false;

    currentFrameIndex_ = 0;
    frameElapsed_ = 0.0f;
    UploadCurrentFrame();
    return true;
}

void GifPlayer::Update(float deltaTime) {
    if (!isPlaying_ || isPaused_) return;
    const auto *animation = GifManager::GetGifAnimation(handle_);
    if (!animation || animation->frames.empty() || !texture_) return;
    if (currentFrameIndex_ >= animation->frames.size()) currentFrameIndex_ = 0;

    frameElapsed_ += deltaTime;
    bool frameChanged = false;
    while (frameElapsed_ >= animation->frames[currentFrameIndex_].delaySeconds) {
        frameElapsed_ -= animation->frames[currentFrameIndex_].delaySeconds;
        if (currentFrameIndex_ + 1 < animation->frames.size()) {
            ++currentFrameIndex_;
            frameChanged = true;
        } else if (loop_) {
            currentFrameIndex_ = 0;
            frameChanged = true;
        } else {
            isPlaying_ = false;
            frameElapsed_ = 0.0f;
            break;
        }
    }
    if (frameChanged) UploadCurrentFrame();
}

std::uint32_t GifPlayer::GetWidth() const noexcept {
    const auto *animation = GifManager::GetGifAnimation(handle_);
    return animation ? animation->width : 0;
}

std::uint32_t GifPlayer::GetHeight() const noexcept {
    const auto *animation = GifManager::GetGifAnimation(handle_);
    return animation ? animation->height : 0;
}

} // namespace KashipanEngine
