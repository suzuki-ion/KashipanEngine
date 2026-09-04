#include "SoundBeat.h"

#include <cmath>

#include "Debug/Logger.h"

namespace KashipanEngine {

SoundBeat::SoundBeat() {
    LogScope scope;
    playHandle_ = AudioManager::kInvalidPlayHandle;
    bpm_ = 0.0f;
    startOffsetSec_ = 0.0;
    currentBeatIndex_ = std::numeric_limits<uint64_t>::max();
    isUseManualTime_ = false;
    AudioManager::RegisterSoundBeat({}, this);
}

SoundBeat::SoundBeat(PlayHandle play, float bpm, double startOffsetSec) {
    LogScope scope;
    SetBeat(play, bpm, startOffsetSec);
    AudioManager::RegisterSoundBeat({}, this);
}

SoundBeat::~SoundBeat() {
    LogScope scope;
    AudioManager::UnregisterSoundBeat({}, this);
}

void SoundBeat::SetBeat(PlayHandle play, float bpm, double startOffsetSec) {
    LogScope scope;
    playHandle_ = play;
    bpm_ = (bpm > 0.0f) ? bpm : 0.0f;
    startOffsetSec_ = startOffsetSec;
    currentBeatIndex_ = std::numeric_limits<uint64_t>::max();

    if (playHandle_ == AudioManager::kInvalidPlayHandle) {
        isUseManualTime_ = false;
    } else {
        isUseManualTime_ = false;
    }
}

void SoundBeat::SetPlayHandle(PlayHandle play) noexcept {
    LogScope scope;
    playHandle_ = play;
    if (playHandle_ == AudioManager::kInvalidPlayHandle) {
        isUseManualTime_ = false;
    } else {
        isUseManualTime_ = false;
    }
}

void SoundBeat::StartManualBeat() {
    LogScope scope;
    if (playHandle_ != AudioManager::kInvalidPlayHandle) return;
    isUseManualTime_ = true;
    manualStartTime_ = std::chrono::steady_clock::now();
    currentBeatIndex_ = std::numeric_limits<uint64_t>::max();
    isOnBeatTriggered_ = false;
}

void SoundBeat::SetOnBeat(std::function<void(PlayHandle, uint64_t, double)> cb) {
    LogScope scope;
    onBeatCallback_ = std::move(cb);
}

void SoundBeat::Update(Passkey<AudioManager>) {
    LogScope scope;
    isOnBeatTriggered_ = false;
    if (!IsActive()) return;
    double posSec = 0.0;

    if (playHandle_ != AudioManager::kInvalidPlayHandle) {
        if (!AudioManager::GetPlayPositionSeconds(playHandle_, posSec)) return;
    } else {
        if (!isUseManualTime_) return;
        const auto now = std::chrono::steady_clock::now();
        posSec = std::chrono::duration<double>(now - manualStartTime_).count();
    }

    if (posSec < startOffsetSec_) return;

    const double interval = 60.0 / static_cast<double>(bpm_);
    if (interval <= 0.0) return;

    const uint64_t beatIndex = static_cast<uint64_t>(std::floor((posSec - startOffsetSec_) / interval));

    if (currentBeatIndex_ == std::numeric_limits<uint64_t>::max()) {
        currentBeatIndex_ = beatIndex;
        if (onBeatCallback_) onBeatCallback_(playHandle_, currentBeatIndex_, posSec);
        isOnBeatTriggered_ = true;
        return;
    }

    if (beatIndex != currentBeatIndex_) {
        currentBeatIndex_ = beatIndex;
        if (onBeatCallback_) onBeatCallback_(playHandle_, currentBeatIndex_, posSec);
        isOnBeatTriggered_ = true;
    }
}

float SoundBeat::GetBeatProgress() const {
    LogScope scope;
    if (!IsActive()) return 0.0f;
    double posSec = 0.0;
    if (playHandle_ != AudioManager::kInvalidPlayHandle) {
        if (!AudioManager::GetPlayPositionSeconds(playHandle_, posSec)) return 0.0f;
    } else {
        if (!isUseManualTime_) return 0.0f;
        const auto now = std::chrono::steady_clock::now();
        posSec = std::chrono::duration<double>(now - manualStartTime_).count();
    }
    if (posSec < startOffsetSec_) return 0.0f;

    const double interval = 60.0 / static_cast<double>(bpm_);
    if (interval <= 0.0) return 0.0f;

    const double t = std::fmod((posSec - startOffsetSec_), interval);
    const double prog = t / interval;
    if (prog < 0.0) return 0.0f;
    if (prog > 1.0) return 1.0f;
    return static_cast<float>(prog);
}

void SoundBeat::Reset() {
    LogScope scope;
    currentBeatIndex_ = std::numeric_limits<uint64_t>::max();
    if (isUseManualTime_) manualStartTime_ = std::chrono::steady_clock::now();
}

} // namespace KashipanEngine
