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
    AudioManager::PlayParams params{};
    params.sound = sound;
    sounds_.push_back(params);
}

void AudioPlayer::AddAudio(const AudioManager::PlayParams& params) {
    if (params.sound == AudioManager::kInvalidSoundHandle) return;
    sounds_.push_back(params);
}

void AudioPlayer::AddAudios(const std::vector<SoundHandle>& sounds) {
    for (const auto sound : sounds) {
        AddAudio(sound);
    }
}

void AudioPlayer::AddAudios(const std::vector<AudioManager::PlayParams>& paramsList) {
    for (const auto& params : paramsList) {
        AddAudio(params);
    }
}

void AudioPlayer::RemoveAudio(SoundHandle sound) {
    if (sounds_.empty()) return;

    const SoundHandle currentSound = (currentIndex_ < sounds_.size()) ? sounds_[currentIndex_].sound
        : AudioManager::kInvalidSoundHandle;
    const SoundHandle nextSound = (nextIndex_ < sounds_.size()) ? sounds_[nextIndex_].sound
        : AudioManager::kInvalidSoundHandle;

    sounds_.erase(std::remove_if(sounds_.begin(), sounds_.end(), [&](const AudioManager::PlayParams& params) {
        return params.sound == sound;
    }), sounds_.end());

    if (currentSound == sound) {
        StopHandle(currentPlay_);
        currentIndex_ = kInvalidAudioIndex;
    } else if (currentSound != AudioManager::kInvalidSoundHandle) {
        auto it = std::find_if(sounds_.begin(), sounds_.end(), [&](const AudioManager::PlayParams& params) {
            return params.sound == currentSound;
        });
        currentIndex_ = (it != sounds_.end()) ? static_cast<size_t>(std::distance(sounds_.begin(), it)) : kInvalidAudioIndex;
    }

    if (nextSound == sound) {
        StopHandle(nextPlay_);
        nextIndex_ = kInvalidAudioIndex;
        isCrossFading_ = false;
    } else if (nextSound != AudioManager::kInvalidSoundHandle) {
        auto it = std::find_if(sounds_.begin(), sounds_.end(), [&](const AudioManager::PlayParams& params) {
            return params.sound == nextSound;
        });
        nextIndex_ = (it != sounds_.end()) ? static_cast<size_t>(std::distance(sounds_.begin(), it)) : kInvalidAudioIndex;
    }
}

void AudioPlayer::RemoveAudios(const std::vector<SoundHandle>& sounds) {
    for (const auto sound : sounds) {
        RemoveAudio(sound);
    }
}

bool AudioPlayer::ChangeAudio(double crossFadeSec, size_t changeAudioIndex) {
    return ChangeAudioInternal(crossFadeSec, nullptr, changeAudioIndex);
}

bool AudioPlayer::ChangeAudio(double crossFadeSec, const AudioManager::PlayParams& params, size_t changeAudioIndex) {
    return ChangeAudioInternal(crossFadeSec, &params, changeAudioIndex);
}

bool AudioPlayer::ChangeAudioInternal(double crossFadeSec, const AudioManager::PlayParams* overrideParams,
    size_t changeAudioIndex) {
    if (sounds_.empty()) return false;

    const size_t nextIndex = ResolveNextIndex(changeAudioIndex, overrideParams);
    if (nextIndex == kInvalidAudioIndex) return false;

    AudioManager::PlayParams params = sounds_[nextIndex];
    if (overrideParams) {
        params = *overrideParams;
        if (params.sound == AudioManager::kInvalidSoundHandle) {
            params.sound = sounds_[nextIndex].sound;
        } else {
            params.sound = sounds_[nextIndex].sound;
        }
        sounds_[nextIndex] = params;
    } else {
        params.sound = sounds_[nextIndex].sound;
    }

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
        currentVolumeTarget_ = std::clamp(params.volume, 0.0f, 1.0f);
        currentPlay_ = AudioManager::Play(params);
        lastUpdateTime_ = std::chrono::steady_clock::now();
        return currentPlay_ != AudioManager::kInvalidPlayHandle;
    }

    StopHandle(nextPlay_);

    nextIndex_ = nextIndex;
    nextVolumeTarget_ = std::clamp(params.volume, 0.0f, 1.0f);
    AudioManager::PlayParams nextParams = params;
    nextParams.volume = 0.0f;
    nextPlay_ = AudioManager::Play(nextParams);
    if (nextPlay_ == AudioManager::kInvalidPlayHandle) {
        nextIndex_ = kInvalidAudioIndex;
        return false;
    }

    currentVolumeTarget_ = 1.0f;
    if (currentIndex_ < sounds_.size()) {
        currentVolumeTarget_ = std::clamp(sounds_[currentIndex_].volume, 0.0f, 1.0f);
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

    ApplyVolume(currentPlay_, static_cast<float>((1.0 - t) * currentVolumeTarget_));
    ApplyVolume(nextPlay_, static_cast<float>(t * nextVolumeTarget_));

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

size_t AudioPlayer::ResolveNextIndex(size_t changeAudioIndex, const AudioManager::PlayParams* params) const {
    if (sounds_.empty()) return kInvalidAudioIndex;
    if (changeAudioIndex != kInvalidAudioIndex) {
        return (changeAudioIndex < sounds_.size()) ? changeAudioIndex : kInvalidAudioIndex;
    }

    if (params && params->sound != AudioManager::kInvalidSoundHandle) {
        auto it = std::find_if(sounds_.begin(), sounds_.end(), [&](const AudioManager::PlayParams& entry) {
            return entry.sound == params->sound;
        });
        if (it != sounds_.end()) {
            return static_cast<size_t>(std::distance(sounds_.begin(), it));
        }
    }

    if (currentIndex_ == kInvalidAudioIndex) return 0;
    return (currentIndex_ + 1) % sounds_.size();
}

} // namespace KashipanEngine
