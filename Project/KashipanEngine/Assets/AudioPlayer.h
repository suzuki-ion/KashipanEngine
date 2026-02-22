#pragma once

#include "Assets/AudioManager.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <vector>

namespace KashipanEngine {

class AudioPlayer final {
public:
    using SoundHandle = AudioManager::SoundHandle;
    using PlayHandle = AudioManager::PlayHandle;

    static constexpr size_t kInvalidAudioIndex = std::numeric_limits<size_t>::max();

    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;

    void AddAudio(SoundHandle sound);
    void AddAudios(const std::vector<SoundHandle>& sounds);
    void RemoveAudio(SoundHandle sound);
    void RemoveAudios(const std::vector<SoundHandle>& sounds);

    bool ChangeAudio(double crossFadeSec, size_t changeAudioIndex = kInvalidAudioIndex);
    void Update(Passkey<AudioManager>);

    size_t GetCurrentAudioIndex() const noexcept { return currentIndex_; }
    PlayHandle GetCurrentPlayHandle() const noexcept { return currentPlay_; }

private:
    void StopHandle(PlayHandle& handle);
    void ApplyVolume(PlayHandle handle, float volume);
    size_t ResolveNextIndex(size_t changeAudioIndex) const;

    std::vector<SoundHandle> sounds_{};
    size_t currentIndex_{ kInvalidAudioIndex };
    size_t nextIndex_{ kInvalidAudioIndex };
    PlayHandle currentPlay_{ AudioManager::kInvalidPlayHandle };
    PlayHandle nextPlay_{ AudioManager::kInvalidPlayHandle };
    double crossFadeDurationSec_{ 0.0 };
    double crossFadeElapsedSec_{ 0.0 };
    bool isCrossFading_{ false };
    float baseVolume_{ 1.0f };
    std::chrono::steady_clock::time_point lastUpdateTime_{};
};

} // namespace KashipanEngine
