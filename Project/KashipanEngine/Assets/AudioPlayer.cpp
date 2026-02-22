#include "AudioPlayer.h"

#include <algorithm>

namespace KashipanEngine {

AudioPlayer::AudioPlayer() {
    AudioManager::RegisterAudioPlayer({}, this);
}

AudioPlayer::~AudioPlayer() {
    AudioManager::UnregisterAudioPlayer({}, this);
    StopHandle(currentPlay_);
    StopHandle(nextPlay_);
}

void AudioPlayer::AddAudio(SoundHandle sound) {
    if (sound == AudioManager::kInvalidSoundHandle) return;
    sounds_.push_back(sound);
}

void AudioPlayer::AddAudios(const std::vector<SoundHandle>& sounds) {
    for (const auto sound : sounds) {
        AddAudio(sound);
    }
}

void AudioPlayer::RemoveAudio(SoundHandle sound) {
    if (sounds_.empty()) return;

    const bool removingCurrent = (currentIndex_ < sounds_.size()) && (sounds_[currentIndex_] == sound);
    const bool removingNext = (nextIndex_ < sounds_.size()) && (sounds_[nextIndex_] == sound);

    sounds_.erase(std::remove(sounds_.begin(), sounds_.end(), sound), sounds_.end());

    if (removingCurrent) {
        StopHandle(currentPlay_);
        currentIndex_ = kInvalidAudioIndex;
    }

    if (removingNext) {
        StopHandle(nextPlay_);
        nextIndex_ = kInvalidAudioIndex;
        isCrossFading_ = false;
    }

    if (currentIndex_ >= sounds_.size()) currentIndex_ = kInvalidAudioIndex;
    if (nextIndex_ >= sounds_.size()) nextIndex_ = kInvalidAudioIndex;
}

void AudioPlayer::RemoveAudios(const std::vector<SoundHandle>& sounds) {
    for (const auto sound : sounds) {
        RemoveAudio(sound);
    }
}

bool AudioPlayer::ChangeAudio(double crossFadeSec, size_t changeAudioIndex) {
    if (sounds_.empty()) return false;

    const size_t nextIndex = ResolveNextIndex(changeAudioIndex);
    if (nextIndex == kInvalidAudioIndex) return false;

    if (currentIndex_ == nextIndex && currentPlay_ != AudioManager::kInvalidPlayHandle && !isCrossFading_) {
        return false;
    }

    if (currentPlay_ == AudioManager::kInvalidPlayHandle || crossFadeSec <= 0.0) {
        StopHandle(currentPlay_);
        StopHandle(nextPlay_);
        currentIndex_ = nextIndex;
        nextIndex_ = kInvalidAudioIndex;
        isCrossFading_ = false;
        crossFadeElapsedSec_ = 0.0;
        crossFadeDurationSec_ = 0.0;
        currentPlay_ = AudioManager::Play(sounds_[currentIndex_], baseVolume_, 0.0f, true);
        lastUpdateTime_ = std::chrono::steady_clock::now();
        return currentPlay_ != AudioManager::kInvalidPlayHandle;
    }

    StopHandle(nextPlay_);

    nextIndex_ = nextIndex;
    nextPlay_ = AudioManager::Play(sounds_[nextIndex_], 0.0f, 0.0f, true);
    if (nextPlay_ == AudioManager::kInvalidPlayHandle) {
        nextIndex_ = kInvalidAudioIndex;
        return false;
    }

    isCrossFading_ = true;
    crossFadeDurationSec_ = std::max(0.0, crossFadeSec);
    crossFadeElapsedSec_ = 0.0;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    return true;
}

void AudioPlayer::Update(Passkey<AudioManager>) {
    const auto now = std::chrono::steady_clock::now();
    if (lastUpdateTime_ == std::chrono::steady_clock::time_point{}) {
        lastUpdateTime_ = now;
        return;
    }

    const double deltaSec = std::chrono::duration<double>(now - lastUpdateTime_).count();
    lastUpdateTime_ = now;

    if (!isCrossFading_) return;

    if (crossFadeDurationSec_ <= 0.0) {
        StopHandle(currentPlay_);
        currentPlay_ = nextPlay_;
        currentIndex_ = nextIndex_;
        nextPlay_ = AudioManager::kInvalidPlayHandle;
        nextIndex_ = kInvalidAudioIndex;
        isCrossFading_ = false;
        return;
    }

    crossFadeElapsedSec_ += std::max(0.0, deltaSec);
    const double t = std::clamp(crossFadeElapsedSec_ / crossFadeDurationSec_, 0.0, 1.0);

    ApplyVolume(currentPlay_, static_cast<float>((1.0 - t) * baseVolume_));
    ApplyVolume(nextPlay_, static_cast<float>(t * baseVolume_));

    if (t >= 1.0) {
        StopHandle(currentPlay_);
        currentPlay_ = nextPlay_;
        currentIndex_ = nextIndex_;
        nextPlay_ = AudioManager::kInvalidPlayHandle;
        nextIndex_ = kInvalidAudioIndex;
        isCrossFading_ = false;
    }
}

void AudioPlayer::StopHandle(PlayHandle& handle) {
    if (handle == AudioManager::kInvalidPlayHandle) return;
    AudioManager::Stop(handle);
    handle = AudioManager::kInvalidPlayHandle;
}

void AudioPlayer::ApplyVolume(PlayHandle handle, float volume) {
    if (handle == AudioManager::kInvalidPlayHandle) return;
    AudioManager::SetVolume(handle, volume);
}

size_t AudioPlayer::ResolveNextIndex(size_t changeAudioIndex) const {
    if (sounds_.empty()) return kInvalidAudioIndex;
    if (changeAudioIndex != kInvalidAudioIndex) {
        return (changeAudioIndex < sounds_.size()) ? changeAudioIndex : kInvalidAudioIndex;
    }

    if (currentIndex_ == kInvalidAudioIndex) return 0;
    return (currentIndex_ + 1) % sounds_.size();
}

} // namespace KashipanEngine
