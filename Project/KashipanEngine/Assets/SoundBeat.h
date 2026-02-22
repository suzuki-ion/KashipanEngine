#pragma once

#include "Assets/AudioManager.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>

namespace KashipanEngine {

class SoundBeat final {
public:
    using PlayHandle = AudioManager::PlayHandle;

    SoundBeat();
    SoundBeat(PlayHandle play, float bpm, double startOffsetSec);
    ~SoundBeat();

    SoundBeat(const SoundBeat&) = delete;
    SoundBeat& operator=(const SoundBeat&) = delete;
    SoundBeat(SoundBeat&&) = delete;
    SoundBeat& operator=(SoundBeat&&) = delete;

    void Update(Passkey<AudioManager>);

    void SetBeat(PlayHandle play, float bpm, double startOffsetSec);
    void SetPlayHandle(PlayHandle play) noexcept;
    void SetBPM(float bpm) noexcept { bpm_ = bpm; }
    void SetStartOffsetSec(double startOffsetSec) noexcept { startOffsetSec_ = startOffsetSec; }
    void SetOnBeat(std::function<void(PlayHandle, uint64_t, double)> cb);

    float GetBeatProgress() const;
    uint64_t GetCurrentBeat() const noexcept { return currentBeatIndex_; }
    void Reset();

    bool IsActive() const noexcept { return bpm_ > 0.0f; }
    bool IsOnBeatTriggered() const noexcept { return isOnBeatTriggered_; }

    void StartManualBeat();
    void StopManualBeat() { isUseManualTime_ = false; }

private:
    PlayHandle playHandle_{ AudioManager::kInvalidPlayHandle };
    float bpm_{ 0.0f };
    double startOffsetSec_{ 0.0 };
    bool isOnBeatTriggered_{ false };

    uint64_t currentBeatIndex_{ std::numeric_limits<uint64_t>::max() };

    std::function<void(PlayHandle, uint64_t, double)> onBeatCallback_;

    bool isUseManualTime_{ false };
    std::chrono::steady_clock::time_point manualStartTime_{};
};

} // namespace KashipanEngine
