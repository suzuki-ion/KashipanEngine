#pragma once
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class AudioListener;
class AudioSource;

/// @brief シーン内のAudioListener/AudioSourceを収集して音声再生を管理するシーンコンポーネント
/// @details 使用中(GetUsed()==true)のAudioListenerの位置・向きを基準に、
///          登録済みの各AudioSourceの距離減衰・パン・ローパスフィルターを毎フレーム更新する。
///          AudioListener/AudioSourceそれぞれのInitialize/Finalizeから自動でRegister/Unregisterされる。
class SceneAudioPlayer final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(SceneAudioPlayer, 1, SetUpdatePriority(500);)
    COMPONENT_CATEGORY("Audio")
    ~SceneAudioPlayer() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneAudioPlayer>();
    }

    //==================================================
    // AudioListener/AudioSourceの登録
    //==================================================

    void RegisterListener(AudioListener *listener);
    void UnregisterListener(const AudioListener *listener);
    void RegisterSource(AudioSource *source);
    void UnregisterSource(const AudioSource *source);

    /// @brief 指定リスナー以外の使用フラグをオフにする（AudioListener::SetUsed(true)から呼ばれる）
    void NotifyListenerUsed(Passkey<AudioListener>, AudioListener *usedListener);

    /// @brief 現在使用中のAudioListenerを取得（存在しない場合は nullptr）
    AudioListener *GetUsedListener() const;

    const std::vector<AudioListener *> &GetListeners() const noexcept { return listeners_; }
    const std::vector<AudioSource *> &GetSources() const noexcept { return sources_; }

protected:
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    std::vector<AudioListener *> listeners_;
    std::vector<AudioSource *> sources_;
};

REGISTER_COMPONENT_SCENE(SceneAudioPlayer)

} // namespace KashipanEngine
