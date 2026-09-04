#include "SceneAudioPlayer.h"

#include <algorithm>

#include "Debug/Logger.h"
#include "Math/Vector3.h"
#include "Objects/Components/AudioListener.h"
#include "Objects/Components/AudioSource.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneAudioPlayer::RegisterListener(AudioListener *listener) {
    LogScope scope;
    if (!listener) return;
    if (std::find(listeners_.begin(), listeners_.end(), listener) != listeners_.end()) return;
    listeners_.push_back(listener);
}

void SceneAudioPlayer::UnregisterListener(const AudioListener *listener) {
    LogScope scope;
    auto it = std::find(listeners_.begin(), listeners_.end(), listener);
    if (it != listeners_.end()) listeners_.erase(it);
}

void SceneAudioPlayer::RegisterSource(AudioSource *source) {
    LogScope scope;
    if (!source) return;
    if (std::find(sources_.begin(), sources_.end(), source) != sources_.end()) return;
    sources_.push_back(source);
}

void SceneAudioPlayer::UnregisterSource(const AudioSource *source) {
    LogScope scope;
    auto it = std::find(sources_.begin(), sources_.end(), source);
    if (it != sources_.end()) sources_.erase(it);
}

void SceneAudioPlayer::NotifyListenerUsed(Passkey<AudioListener>, AudioListener *usedListener) {
    LogScope scope;
    for (auto *listener : listeners_) {
        if (listener && listener != usedListener) {
            listener->SetUsed(false);
        }
    }
}

AudioListener *SceneAudioPlayer::GetUsedListener() const {
    LogScope scope;
    for (auto *listener : listeners_) {
        if (listener && listener->IsActive() && listener->GetUsed()) return listener;
    }
    return nullptr;
}

void SceneAudioPlayer::Update() {
    LogScope scope;
    AudioListener *usedListener = GetUsedListener();
    const bool hasListener = usedListener != nullptr;
    const Vector3 listenerPosition = hasListener ? usedListener->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);
    const Vector3 listenerRight = hasListener ? usedListener->GetWorldRightDirection() : Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 listenerForward = hasListener ? usedListener->GetWorldForwardDirection() : Vector3(0.0f, 0.0f, 1.0f);

    for (auto *source : sources_) {
        if (!source || !source->IsActive()) continue;
        source->UpdateSpatial(Passkey<SceneAudioPlayer>(), hasListener, listenerPosition, listenerRight, listenerForward);
    }
}

#if defined(USE_IMGUI)
void SceneAudioPlayer::ShowImGui() {
    LogScope scope;
    ImGui::Text("%s%d", TranslationC("editor.sceneaudioplayer.listeners"), static_cast<int>(listeners_.size()));
    ImGui::Text("%s%d", TranslationC("editor.sceneaudioplayer.sources"), static_cast<int>(sources_.size()));
}
#endif

} // namespace KashipanEngine
