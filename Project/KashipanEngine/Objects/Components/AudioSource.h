#pragma once
#include <algorithm>
#include <string>

#include "Assets/AudioManager.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Audio/SceneAudioPlayer.h"

namespace KashipanEngine {

/// @brief 音声を再生するコンポーネント
/// @details 使用する音声・ボリューム・ピッチ・ループ・Min/Max Distance・フィルターエフェクトの設定を持つ。
///          自動パン/ローパスフィルター(enableSpatialAudio)が有効な場合、使用中のAudioListenerとの
///          位置関係からSceneAudioPlayerが毎フレーム左右への振り分けと音の籠り具合を自動で更新する。
class AudioSource final : public IObjectComponent {
public:
    /// @brief 手動で設定するエフェクトの種別
    enum class EffectType { None, LowPass, HighPass, BandPass, Notch, Reverb };

    OBJECT_COMPONENT_CONSTRUCTOR(AudioSource, 0xFF, )
    COMPONENT_CATEGORY("Audio")
    ~AudioSource() override { Stop(); }

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AudioSource>();
        ptr->soundName_ = soundName_;
        ptr->volume_ = volume_;
        ptr->pitch_ = pitch_;
        ptr->loop_ = loop_;
        ptr->minDistance_ = minDistance_;
        ptr->maxDistance_ = maxDistance_;
        ptr->effectType_ = effectType_;
        ptr->effectFrequency_ = effectFrequency_;
        ptr->effectQ_ = effectQ_;
        ptr->reverbMix_ = reverbMix_;
        ptr->enableSpatialAudio_ = enableSpatialAudio_;
        return ptr;
    }

    //==================================================
    // 再生制御
    //==================================================

    /// @brief 音声を再生する（既に再生中の場合は停止してから再生し直す）
    /// @return 成功した場合は再生ハンドル、失敗した場合は AudioManager::kInvalidPlayHandle
    AudioManager::PlayHandle Play() {
        const auto soundHandle = ResolveSoundHandle();
        if (soundHandle == AudioManager::kInvalidSoundHandle) return AudioManager::kInvalidPlayHandle;

        Stop();

        AudioManager::PlayParams params{};
        params.sound = soundHandle;
        params.volume = volume_;
        params.pitch = pitch_;
        params.loop = loop_;
        currentPlayHandle_ = AudioManager::Play(params);
        if (currentPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::SetPan(currentPlayHandle_, 0.0f);
            ApplyEffects(0.0f);
        }
        return currentPlayHandle_;
    }

    /// @brief 再生を停止する
    void Stop() {
        if (currentPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(currentPlayHandle_);
            currentPlayHandle_ = AudioManager::kInvalidPlayHandle;
        }
    }
    /// @brief 一時停止する
    bool Pause() { return currentPlayHandle_ != AudioManager::kInvalidPlayHandle && AudioManager::Pause(currentPlayHandle_); }
    /// @brief 一時停止を解除する
    bool Resume() { return currentPlayHandle_ != AudioManager::kInvalidPlayHandle && AudioManager::Resume(currentPlayHandle_); }
    /// @brief 再生中かどうか
    bool IsPlaying() const { return currentPlayHandle_ != AudioManager::kInvalidPlayHandle && AudioManager::IsPlaying(currentPlayHandle_); }
    /// @brief 一時停止中かどうか
    bool IsPaused() const { return currentPlayHandle_ != AudioManager::kInvalidPlayHandle && AudioManager::IsPaused(currentPlayHandle_); }
    /// @brief 現在の再生ハンドルを取得（再生していない場合は kInvalidPlayHandle）
    AudioManager::PlayHandle GetCurrentPlayHandle() const noexcept { return currentPlayHandle_; }

    //==================================================
    // プロパティ
    //==================================================

    void SetSoundName(const std::string &soundName) {
        soundName_ = soundName;
        soundHandle_ = AudioManager::kInvalidSoundHandle;
    }
    const std::string &GetSoundName() const noexcept { return soundName_; }

    void SetVolume(float volume) { volume_ = std::clamp(volume, 0.0f, 1.0f); }
    float GetVolume() const noexcept { return volume_; }

    void SetPitch(float pitch) {
        pitch_ = pitch;
        if (currentPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::SetPitch(currentPlayHandle_, pitch_);
        }
    }
    float GetPitch() const noexcept { return pitch_; }

    void SetLoop(bool loop) { loop_ = loop; }
    bool GetLoop() const noexcept { return loop_; }

    void SetMinDistance(float minDistance) { minDistance_ = std::max(0.0f, minDistance); }
    float GetMinDistance() const noexcept { return minDistance_; }
    void SetMaxDistance(float maxDistance) { maxDistance_ = std::max(0.0f, maxDistance); }
    float GetMaxDistance() const noexcept { return maxDistance_; }

    void SetEffectType(EffectType effectType) { effectType_ = effectType; }
    EffectType GetEffectType() const noexcept { return effectType_; }
    void SetEffectFrequency(float effectFrequency) { effectFrequency_ = std::clamp(effectFrequency, 0.0005f, 1.0f); }
    float GetEffectFrequency() const noexcept { return effectFrequency_; }
    void SetEffectQ(float effectQ) { effectQ_ = std::clamp(effectQ, 0.0005f, 1.5f); }
    float GetEffectQ() const noexcept { return effectQ_; }
    /// @brief リバーブの送り量(ウェットレベル)を設定する（EffectType::Reverb選択時のみ有効）
    void SetReverbMix(float reverbMix) { reverbMix_ = std::clamp(reverbMix, 0.0f, 1.0f); }
    float GetReverbMix() const noexcept { return reverbMix_; }

    /// @brief AudioListenerとの位置関係から自動でパン/ローパスフィルターを適用するかどうかを設定する
    void SetEnableSpatialAudio(bool enable) { enableSpatialAudio_ = enable; }
    bool GetEnableSpatialAudio() const noexcept { return enableSpatialAudio_; }

    /// @brief ワールド座標を取得（Transform が無い場合は原点）
    Vector3 GetWorldPosition() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
        const Matrix4x4 &world = transform->GetWorldMatrix();
        return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
    }

    /// @brief 使用中のAudioListenerとの位置関係から音量減衰・パン・ローパスフィルターを更新する
    /// @details SceneAudioPlayer::Update() から毎フレーム呼ばれる。
    ///          listenerRight/listenerForwardはAudioListenerの向きを表す正規化済みの単位ベクトル
    void UpdateSpatial(Passkey<SceneAudioPlayer>, bool hasListener, const Vector3 &listenerPosition,
        const Vector3 &listenerRight, const Vector3 &listenerForward) {
        if (currentPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        if (!AudioManager::IsPlaying(currentPlayHandle_) && !AudioManager::IsPaused(currentPlayHandle_)) {
            currentPlayHandle_ = AudioManager::kInvalidPlayHandle;
            return;
        }

        float attenuation = 1.0f;
        float pan = 0.0f;
        float muffleAmount = 0.0f;

        if (hasListener) {
            const Vector3 toSource = GetWorldPosition() - listenerPosition;
            const float distance = toSource.Length();
            const float range = std::max(0.0001f, maxDistance_ - minDistance_);
            attenuation = std::clamp((maxDistance_ - distance) / range, 0.0f, 1.0f);

            if (enableSpatialAudio_) {
                Vector3 direction(0.0f, 0.0f, 0.0f);
                if (distance > 0.0001f) {
                    direction = toSource * (1.0f / distance);
                    pan = std::clamp(direction.Dot(listenerRight), -1.0f, 1.0f);
                }
                const float distanceMuffle = std::clamp((distance - minDistance_) / range, 0.0f, 1.0f);
                // 正面成分(-1:真後ろ ～ 1:正面)がマイナスに近いほど、音が背後にあるとみなし追加で籠らせる
                const float frontAmount = direction.Dot(listenerForward);
                const float behindMuffle = std::clamp(-frontAmount, 0.0f, 1.0f) * 0.5f;
                muffleAmount = std::clamp(distanceMuffle + behindMuffle, 0.0f, 1.0f);
            }
        }

        AudioManager::SetVolume(currentPlayHandle_, volume_ * attenuation);
        AudioManager::SetPan(currentPlayHandle_, pan);
        ApplyEffects(muffleAmount);
    }

protected:
    void Initialize() override {
        auto *player = GetOrAddSceneAudioPlayer();
        if (player) player->RegisterSource(this);
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *player = sceneContext ? sceneContext->GetComponent<SceneAudioPlayer>() : nullptr;
        if (player) player->UnregisterSource(this);
        Stop();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGuiCustom::SelectString("Sound", soundName_, AudioManager::GetLoadedSoundAssetPaths(), true)) {
            soundHandle_ = AudioManager::kInvalidSoundHandle;
        }
        ImGui::DragFloat("Volume", &volume_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Pitch(semitones)", &pitch_, 0.1f, -24.0f, 24.0f);
        ImGui::Checkbox("Loop", &loop_);
        ImGui::DragFloat("Min Distance", &minDistance_, 0.1f, 0.0f, maxDistance_);
        ImGui::DragFloat("Max Distance", &maxDistance_, 0.1f, minDistance_, 10000.0f);

        int effectType = static_cast<int>(effectType_);
        const char *effectItems[] = { "None", "LowPass", "HighPass", "BandPass", "Notch", "Reverb" };
        if (ImGui::Combo("Effect", &effectType, effectItems, 6)) effectType_ = static_cast<EffectType>(effectType);
        if (effectType_ == EffectType::Reverb) {
            ImGui::DragFloat("Reverb Mix", &reverbMix_, 0.005f, 0.0f, 1.0f);
        } else if (effectType_ != EffectType::None) {
            ImGui::DragFloat("Effect Frequency", &effectFrequency_, 0.005f, 0.0005f, 1.0f);
            ImGui::DragFloat("Effect Q", &effectQ_, 0.01f, 0.0005f, 1.5f);
        }

        ImGui::Checkbox("Auto Pan / Muffle (Spatial Audio)", &enableSpatialAudio_);

        ImGui::Separator();
        if (!IsPlaying()) {
            if (ImGui::Button("Play")) Play();
        } else if (IsPaused()) {
            if (ImGui::Button("Resume")) Resume();
        } else {
            if (ImGui::Button("Pause")) Pause();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsPlaying() && !IsPaused());
        if (ImGui::Button("Stop")) Stop();
        ImGui::EndDisabled();
    }
#endif

    JSON SaveToJson() const override {
        return JSON{
            {"soundName", soundName_}, {"volume", volume_}, {"pitch", pitch_}, {"loop", loop_},
            {"minDistance", minDistance_}, {"maxDistance", maxDistance_},
            {"effectType", static_cast<int>(effectType_)}, {"effectFrequency", effectFrequency_}, {"effectQ", effectQ_},
            {"reverbMix", reverbMix_}, {"enableSpatialAudio", enableSpatialAudio_}
        };
    }
    bool LoadFromJson(const JSON &json) override {
        soundName_ = json.value("soundName", std::string{});
        soundHandle_ = AudioManager::kInvalidSoundHandle;
        volume_ = json.value("volume", 1.0f);
        pitch_ = json.value("pitch", 0.0f);
        loop_ = json.value("loop", false);
        minDistance_ = json.value("minDistance", 1.0f);
        maxDistance_ = json.value("maxDistance", 25.0f);
        effectType_ = static_cast<EffectType>(json.value("effectType", 0));
        effectFrequency_ = json.value("effectFrequency", 1.0f);
        effectQ_ = json.value("effectQ", 1.0f);
        reverbMix_ = json.value("reverbMix", 0.3f);
        enableSpatialAudio_ = json.value("enableSpatialAudio", false);
        return true;
    }

private:
    SceneAudioPlayer *GetOrAddSceneAudioPlayer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *player = sceneContext->GetComponent<SceneAudioPlayer>();
        if (!player) {
            player = sceneContext->AddComponent<SceneAudioPlayer>();
        }
        return player;
    }

    AudioManager::SoundHandle ResolveSoundHandle() const {
        if (soundHandle_ == AudioManager::kInvalidSoundHandle && !soundName_.empty()) {
            soundHandle_ = AudioManager::GetSoundHandleFromAssetPath(soundName_);
            if (soundHandle_ == AudioManager::kInvalidSoundHandle) {
                soundHandle_ = AudioManager::GetSoundHandleFromFileName(soundName_);
            }
        }
        return soundHandle_;
    }

    static AudioManager::FilterType ToFilterType(EffectType type) {
        switch (type) {
        case EffectType::HighPass: return AudioManager::FilterType::HighPass;
        case EffectType::BandPass: return AudioManager::FilterType::BandPass;
        case EffectType::Notch:    return AudioManager::FilterType::Notch;
        case EffectType::LowPass:
        case EffectType::None:
        case EffectType::Reverb:
        default:                  return AudioManager::FilterType::LowPass;
        }
    }

    /// @brief 手動エフェクトの設定に、距離・向きによる籠り具合(muffleAmount, 0=クリア～1=最大)を重ねて適用する
    /// @details フィルター系エフェクト(LowPass/HighPass/BandPass/Notch)はmuffleAmountでカットオフ周波数を狭め、
    ///          Reverbは別経路(共有リバーブバス)への送り量として適用する。フィルターとリバーブは併用しない
    void ApplyEffects(float muffleAmount) const {
        if (currentPlayHandle_ == AudioManager::kInvalidPlayHandle) return;

        const bool isFilterEffect = effectType_ != EffectType::None && effectType_ != EffectType::Reverb;
        const AudioManager::FilterType filterType = isFilterEffect ? ToFilterType(effectType_) : AudioManager::FilterType::LowPass;
        const float baseFrequency = isFilterEffect ? effectFrequency_ : 1.0f;
        const float muffleFactor = 1.0f - muffleAmount * 0.95f;
        const float frequency = std::clamp(baseFrequency * muffleFactor, 0.0005f, 1.0f);
        const float oneOverQ = isFilterEffect ? effectQ_ : 1.0f;
        AudioManager::SetFilter(currentPlayHandle_, filterType, frequency, oneOverQ);

        AudioManager::SetReverbSend(currentPlayHandle_, effectType_ == EffectType::Reverb ? reverbMix_ : 0.0f);
    }

    std::string soundName_;
    mutable AudioManager::SoundHandle soundHandle_ = AudioManager::kInvalidSoundHandle;
    float volume_ = 1.0f;
    float pitch_ = 0.0f;
    bool loop_ = false;
    float minDistance_ = 1.0f;
    float maxDistance_ = 25.0f;
    EffectType effectType_ = EffectType::None;
    float effectFrequency_ = 1.0f;
    float effectQ_ = 1.0f;
    float reverbMix_ = 0.3f;
    bool enableSpatialAudio_ = false;

    AudioManager::PlayHandle currentPlayHandle_ = AudioManager::kInvalidPlayHandle;
};

REGISTER_COMPONENT_OBJECT(AudioSource)

} // namespace KashipanEngine
