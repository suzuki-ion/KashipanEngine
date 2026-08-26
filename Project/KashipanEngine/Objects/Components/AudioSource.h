#pragma once
#include <algorithm>
#include <string>

#include "Assets/AudioManager.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Audio/SceneAudioPlayer.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 音声を再生するコンポーネント
/// @details 使用する音声・ボリューム・ピッチ・ループ・Min/Max Distance・各種エフェクトの設定を持つ。
///          エフェクトはフィルター/リバーブ/エコー/イコライザー/リミッターを個別に有効化でき、複数同時にかけられる。
///          自動パン/ローパスフィルター(enableSpatialAudio)が有効な場合、使用中のAudioListenerとの
///          位置関係からSceneAudioPlayerが毎フレーム左右への振り分けと音の籠り具合を自動で更新する。
class AudioSource final : public IObjectComponent {
public:
    /// @brief フィルターエフェクトの種別
    enum class FilterKind { LowPass, HighPass, BandPass, Notch };

    /// @brief フィルターエフェクト（特定の周波数帯域を削る）の設定
    struct FilterEffect final {
        bool enabled = false;
        FilterKind kind = FilterKind::LowPass;
        /// @brief 正規化カットオフ周波数（0に近いほど強くかかり、1.0でほぼ無効）
        float frequency = 1.0f;
        /// @brief 共鳴の鋭さ(1/Q)。小さいほどカットオフ付近が強調される
        float q = 1.0f;
    };

    /// @brief リバーブ（残響）エフェクトの設定
    struct ReverbEffect final {
        bool enabled = false;
        /// @brief 共有リバーブバスへの送り量（0.0:残響無し ～ 1.0:最大）
        float mix = 0.3f;
    };

    /// @brief エコー（やまびこ）エフェクトの設定
    struct EchoEffect final {
        bool enabled = false;
        AudioManager::EchoParams params{};
    };

    /// @brief 4バンドイコライザーエフェクトの設定
    struct EqEffect final {
        bool enabled = false;
        AudioManager::EqParams params{};
    };

    /// @brief マスタリングリミッター（音割れ防止の圧縮）エフェクトの設定
    struct LimiterEffect final {
        bool enabled = false;
        AudioManager::LimiterParams params{};
    };

    // 直接書き込み時もセッター/ImGui編集時と同じ副作用（クランプ・再生中音声への反映）がかかるようにする
    OBJECT_COMPONENT_CONSTRUCTOR(AudioSource, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(soundName_, [this] { soundHandle_ = AudioManager::kInvalidSoundHandle; });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(volume_, [this] { volume_ = std::clamp(volume_, 0.0f, 1.0f); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(pitch_, [this] { SetPitch(pitch_); });
        ADD_MEMBER_VARIABLE(loop_);
        ADD_MEMBER_VARIABLE(playOnAwake_);
        ADD_MEMBER_VARIABLE(minDistance_);
        ADD_MEMBER_VARIABLE(maxDistance_);
        ADD_MEMBER_VARIABLE(enableSpatialAudio_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(filter_.enabled, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(filter_.frequency, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(filter_.q, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(reverb_.enabled, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(reverb_.mix, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(echo_.enabled, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(echo_.params.wetDryMix, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(echo_.params.feedback, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(echo_.params.delayMs, [this] { ClampEffectParams(); ReapplyEffects(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(eq_.enabled, [this] { ClampEffectParams(); ReapplyEffects(); });
        for (int band = 0; band < 4; ++band) {
            AddMemberVariable("eq_.frequencyCenter[" + std::to_string(band) + "]", &eq_.params.frequencyCenter[band],
                [this] { ClampEffectParams(); ReapplyEffects(); });
            AddMemberVariable("eq_.gain[" + std::to_string(band) + "]", &eq_.params.gain[band],
                [this] { ClampEffectParams(); ReapplyEffects(); });
        }
    )
    COMPONENT_CATEGORY("Audio")
    ~AudioSource() override { Stop(); }

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AudioSource>();
        ptr->soundName_ = soundName_;
        ptr->volume_ = volume_;
        ptr->pitch_ = pitch_;
        ptr->loop_ = loop_;
        ptr->playOnAwake_ = playOnAwake_;
        ptr->minDistance_ = minDistance_;
        ptr->maxDistance_ = maxDistance_;
        ptr->filter_ = filter_;
        ptr->reverb_ = reverb_;
        ptr->echo_ = echo_;
        ptr->eq_ = eq_;
        ptr->limiter_ = limiter_;
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
            lastMuffleAmount_ = 0.0f;
            ApplyChainEffects();
            ApplyFilterAndReverb(0.0f);
        }
        return currentPlayHandle_;
    }

    /// @brief 外部（VideoSourceの動画音声など）が既に再生中のPlayHandleに対して、
    ///        Spatial Audio（距離減衰・パン・籠り）とエフェクトだけを適用する
    /// @details 対象ハンドルの生成・破棄はこちら側では行わない。Stop()を呼んでも
    ///          AudioManager::Stop()は呼ばれず、単にこのAudioSourceからの参照を外すだけになる
    ///          （映像と音声の同期用マスタークロックとして使われている場合等に、誤って
    ///          その音声自体を止めてしまわないようにするため）
    void AttachExternalPlayHandle(AudioManager::PlayHandle handle) {
        Stop();
        if (handle == AudioManager::kInvalidPlayHandle) return;
        currentPlayHandle_ = handle;
        isExternalHandle_ = true;
        AudioManager::SetPan(currentPlayHandle_, 0.0f);
        lastMuffleAmount_ = 0.0f;
        SetPitch(pitch_);
        ApplyChainEffects();
        ApplyFilterAndReverb(0.0f);
    }

    /// @brief 再生を停止する（外部ハンドルにアタッチ中の場合は、そのハンドル自体は止めず参照を外すだけ）
    void Stop() {
        if (currentPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            if (!isExternalHandle_) {
                AudioManager::Stop(currentPlayHandle_);
            }
            currentPlayHandle_ = AudioManager::kInvalidPlayHandle;
            isExternalHandle_ = false;
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

    /// @brief シーン開始時（Initialize時）に自動で再生を開始するかどうかを設定する
    void SetPlayOnAwake(bool playOnAwake) { playOnAwake_ = playOnAwake; }
    bool GetPlayOnAwake() const noexcept { return playOnAwake_; }

    void SetMinDistance(float minDistance) { minDistance_ = std::max(0.0f, minDistance); }
    float GetMinDistance() const noexcept { return minDistance_; }
    void SetMaxDistance(float maxDistance) { maxDistance_ = std::max(0.0f, maxDistance); }
    float GetMaxDistance() const noexcept { return maxDistance_; }

    //==================================================
    // エフェクト（複数同時にかけられる。各エフェクトはenabledで個別に有効/無効を切り替える）
    //==================================================

    void SetFilterEffect(const FilterEffect &effect) { filter_ = effect; ClampEffectParams(); ReapplyEffects(); }
    const FilterEffect &GetFilterEffect() const noexcept { return filter_; }

    void SetReverbEffect(const ReverbEffect &effect) { reverb_ = effect; ClampEffectParams(); ReapplyEffects(); }
    const ReverbEffect &GetReverbEffect() const noexcept { return reverb_; }

    void SetEchoEffect(const EchoEffect &effect) { echo_ = effect; ClampEffectParams(); ReapplyEffects(); }
    const EchoEffect &GetEchoEffect() const noexcept { return echo_; }

    void SetEqEffect(const EqEffect &effect) { eq_ = effect; ClampEffectParams(); ReapplyEffects(); }
    const EqEffect &GetEqEffect() const noexcept { return eq_; }

    void SetLimiterEffect(const LimiterEffect &effect) { limiter_ = effect; ClampEffectParams(); ReapplyEffects(); }
    const LimiterEffect &GetLimiterEffect() const noexcept { return limiter_; }

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
        lastMuffleAmount_ = muffleAmount;
        ApplyFilterAndReverb(muffleAmount);
    }

protected:
    void Initialize() override {
        auto *player = GetOrAddSceneAudioPlayer();
        if (player) player->RegisterSource(this);
        // Initialize()はシーン読み込み・エディター編集時にも走る（ゲームループ開始前）ため、
        // ここで即座にPlay()すると再生開始（PlayStart）前に鳴ってしまい、実際の再生開始時には
        // 既に再生済み/終了済みになってしまう。実際にゲームが動き出すまで待つため、
        // 最初のUpdate()（ゲームループが実際に回っているときのみ呼ばれる）まで再生を遅延させる
        pendingAutoPlay_ = playOnAwake_;
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *player = sceneContext ? sceneContext->GetComponent<SceneAudioPlayer>() : nullptr;
        if (player) player->UnregisterSource(this);
        Stop();
    }

    void Update() override {
        if (pendingAutoPlay_) {
            pendingAutoPlay_ = false;
            Play();
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        auto tooltip = [](const char *text) {
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
        };

        if (ImGuiCustom::SelectString(TranslationLabel("component.audiosource.sound"), soundName_, AudioManager::GetLoadedSoundAssetPaths(), true)) {
            soundHandle_ = AudioManager::kInvalidSoundHandle;
        }
        ImGui::DragFloat(TranslationLabel("component.audiosource.volume"), &volume_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat(TranslationLabel("component.audiosource.pitch_semitones"), &pitch_, 0.1f, -24.0f, 24.0f);
        ImGui::Checkbox(TranslationLabel("component.audiosource.loop"), &loop_);
        ImGui::Checkbox(TranslationLabel("component.audiosource.play_on_awake"), &playOnAwake_);
        ImGui::DragFloat(TranslationLabel("component.audiosource.min_distance"), &minDistance_, 0.1f, 0.0f, maxDistance_);
        ImGui::DragFloat(TranslationLabel("component.audiosource.max_distance"), &maxDistance_, 0.1f, minDistance_, 10000.0f);

        ImGui::Separator();
        ImGui::Text("%s", TranslationC("component.audiosource.effects"));
        tooltip("各エフェクトは個別に有効/無効を切り替えられ、複数同時にかけられる");
        bool changed = false;

        // フィルター
        changed |= ImGui::Checkbox(TranslationLabel("component.audiosource.filter"), &filter_.enabled);
        tooltip("特定の周波数帯域を削るフィルター。音を籠らせたり、軽くしたりできる");
        if (filter_.enabled) {
            ImGui::Indent();
            int kind = static_cast<int>(filter_.kind);
            const char *kindItems[] = { "LowPass", "HighPass", "BandPass", "Notch" };
            if (ImGui::Combo(TranslationLabel("component.audiosource.filter_type"), &kind, kindItems, 4)) {
                filter_.kind = static_cast<FilterKind>(kind);
                changed = true;
            }
            tooltip("LowPass: 高音を削って籠らせる\nHighPass: 低音を削って軽くする\nBandPass: カットオフ周波数付近のみを通す\nNotch: カットオフ周波数付近のみを削る");
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.frequency"), &filter_.frequency, 0.005f, 0.0005f, 1.0f);
            tooltip("正規化カットオフ周波数。0に近いほど強くかかり、1.0でほぼ無効");
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.q_resonance"), &filter_.q, 0.01f, 0.0005f, 1.5f);
            tooltip("共鳴の鋭さ(1/Q)。小さいほどカットオフ周波数付近の音が強調される");
            ImGui::Unindent();
        }

        // リバーブ
        changed |= ImGui::Checkbox(TranslationLabel("component.audiosource.reverb"), &reverb_.enabled);
        tooltip("残響。ホールや洞窟の中で鳴っているような響きを付加する");
        if (reverb_.enabled) {
            ImGui::Indent();
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.reverb_mix"), &reverb_.mix, 0.005f, 0.0f, 1.0f);
            tooltip("残響への送り量。大きいほど響きが強くなる（0.0: 残響無し ～ 1.0: 最大）");
            ImGui::Unindent();
        }

        // エコー
        changed |= ImGui::Checkbox(TranslationLabel("component.audiosource.echo"), &echo_.enabled);
        tooltip("やまびこ。遅延した音を繰り返し重ねる");
        if (echo_.enabled) {
            ImGui::Indent();
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.wet_dry_mix"), &echo_.params.wetDryMix, 0.005f, 0.0f, 1.0f);
            tooltip("原音とエコー音の混合比（0.0: 原音のみ ～ 1.0: エコー音のみ）");
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.feedback"), &echo_.params.feedback, 0.005f, 0.0f, 1.0f);
            tooltip("エコーの繰り返し量。大きいほど長く繰り返される（1.0で減衰しない）");
            changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.delay_ms"), &echo_.params.delayMs, 1.0f, 1.0f, 2000.0f);
            tooltip("エコーの遅延時間（ミリ秒）");
            ImGui::Unindent();
        }

        // イコライザー
        changed |= ImGui::Checkbox(TranslationLabel("component.audiosource.equalizer"), &eq_.enabled);
        tooltip("4バンドイコライザー。周波数帯域ごとに音量を増減できる");
        if (eq_.enabled) {
            ImGui::Indent();
            for (int band = 0; band < 4; ++band) {
                ImGui::PushID(band);
                ImGui::Text(TranslationC("component.audiosource.band_d"), band + 1);
                changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.frequency_hz"), &eq_.params.frequencyCenter[band], 10.0f, 20.0f, 20000.0f);
                tooltip("このバンドの中心周波数（Hz）");
                changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.gain"), &eq_.params.gain[band], 0.01f, 0.126f, 7.94f);
                tooltip("このバンドの増幅率。1.0で等倍、小さいほど削り、大きいほど持ち上げる");
                changed |= ImGui::DragFloat(TranslationLabel("component.audiosource.bandwidth"), &eq_.params.bandwidth[band], 0.01f, 0.1f, 2.0f);
                tooltip("このバンドの帯域幅（中心周波数を基準としたオクターブ幅）。大きいほど広い範囲に影響する");
                ImGui::PopID();
            }
            ImGui::Unindent();
        }

        // リミッター
        changed |= ImGui::Checkbox(TranslationLabel("component.audiosource.limiter"), &limiter_.enabled);
        tooltip("マスタリングリミッター。音量が大きくなりすぎないよう圧縮し、音割れを防ぐ");
        if (limiter_.enabled) {
            ImGui::Indent();
            int release = static_cast<int>(limiter_.params.release);
            if (ImGui::DragInt(TranslationLabel("component.audiosource.release"), &release, 0.1f, 1, 20)) {
                limiter_.params.release = static_cast<uint32_t>(release);
                changed = true;
            }
            tooltip("圧縮を解除するまでの時間。大きいほどゆっくり戻る");
            int loudness = static_cast<int>(limiter_.params.loudness);
            if (ImGui::DragInt(TranslationLabel("component.audiosource.loudness"), &loudness, 1.0f, 1, 1800)) {
                limiter_.params.loudness = static_cast<uint32_t>(loudness);
                changed = true;
            }
            tooltip("音量の閾値。小さいほど強く圧縮される");
            ImGui::Unindent();
        }

        if (changed) {
            ClampEffectParams();
            ReapplyEffects();
        }

        ImGui::Checkbox(TranslationLabel("component.audiosource.auto_pan_muffle_spatial_audio"), &enableSpatialAudio_);

        ImGui::Separator();
        if (!IsPlaying()) {
            if (ImGui::Button(TranslationLabel("component.audiosource.play"))) Play();
        } else if (IsPaused()) {
            if (ImGui::Button(TranslationLabel("component.audiosource.resume"))) Resume();
        } else {
            if (ImGui::Button(TranslationLabel("component.audiosource.pause"))) Pause();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsPlaying() && !IsPaused());
        if (ImGui::Button(TranslationLabel("component.audiosource.stop"))) Stop();
        ImGui::EndDisabled();
    }
#endif

    JSON SaveToJson() const override {
        JSON effects = JSON::object();
        effects["filter"] = JSON{
            {"enabled", filter_.enabled}, {"kind", static_cast<int>(filter_.kind)},
            {"frequency", filter_.frequency}, {"q", filter_.q}
        };
        effects["reverb"] = JSON{ {"enabled", reverb_.enabled}, {"mix", reverb_.mix} };
        effects["echo"] = JSON{
            {"enabled", echo_.enabled}, {"wetDryMix", echo_.params.wetDryMix},
            {"feedback", echo_.params.feedback}, {"delayMs", echo_.params.delayMs}
        };
        effects["eq"] = JSON{
            {"enabled", eq_.enabled},
            {"frequencyCenter", std::vector<float>(eq_.params.frequencyCenter, eq_.params.frequencyCenter + 4)},
            {"gain", std::vector<float>(eq_.params.gain, eq_.params.gain + 4)},
            {"bandwidth", std::vector<float>(eq_.params.bandwidth, eq_.params.bandwidth + 4)}
        };
        effects["limiter"] = JSON{
            {"enabled", limiter_.enabled},
            {"release", limiter_.params.release}, {"loudness", limiter_.params.loudness}
        };

        return JSON{
            {"soundName", soundName_}, {"volume", volume_}, {"pitch", pitch_}, {"loop", loop_},
            {"playOnAwake", playOnAwake_},
            {"minDistance", minDistance_}, {"maxDistance", maxDistance_},
            {"effects", effects}, {"enableSpatialAudio", enableSpatialAudio_}
        };
    }

    bool LoadFromJson(const JSON &json) override {
        soundName_ = json.value("soundName", std::string{});
        soundHandle_ = AudioManager::kInvalidSoundHandle;
        volume_ = json.value("volume", 1.0f);
        pitch_ = json.value("pitch", 0.0f);
        loop_ = json.value("loop", false);
        playOnAwake_ = json.value("playOnAwake", true);
        minDistance_ = json.value("minDistance", 1.0f);
        maxDistance_ = json.value("maxDistance", 25.0f);
        enableSpatialAudio_ = json.value("enableSpatialAudio", false);

        filter_ = FilterEffect{};
        reverb_ = ReverbEffect{};
        echo_ = EchoEffect{};
        eq_ = EqEffect{};
        limiter_ = LimiterEffect{};

        if (json.contains("effects")) {
            const auto &effects = json.at("effects");
            if (effects.contains("filter")) {
                const auto &filterJson = effects.at("filter");
                filter_.enabled = filterJson.value("enabled", false);
                filter_.kind = static_cast<FilterKind>(filterJson.value("kind", 0));
                filter_.frequency = filterJson.value("frequency", 1.0f);
                filter_.q = filterJson.value("q", 1.0f);
            }
            if (effects.contains("reverb")) {
                const auto &reverbJson = effects.at("reverb");
                reverb_.enabled = reverbJson.value("enabled", false);
                reverb_.mix = reverbJson.value("mix", 0.3f);
            }
            if (effects.contains("echo")) {
                const auto &echoJson = effects.at("echo");
                echo_.enabled = echoJson.value("enabled", false);
                echo_.params.wetDryMix = echoJson.value("wetDryMix", 0.5f);
                echo_.params.feedback = echoJson.value("feedback", 0.5f);
                echo_.params.delayMs = echoJson.value("delayMs", 500.0f);
            }
            if (effects.contains("eq")) {
                const auto &eqJson = effects.at("eq");
                eq_.enabled = eqJson.value("enabled", false);
                LoadFloat4(eqJson, "frequencyCenter", eq_.params.frequencyCenter);
                LoadFloat4(eqJson, "gain", eq_.params.gain);
                LoadFloat4(eqJson, "bandwidth", eq_.params.bandwidth);
            }
            if (effects.contains("limiter")) {
                const auto &limiterJson = effects.at("limiter");
                limiter_.enabled = limiterJson.value("enabled", false);
                limiter_.params.release = limiterJson.value("release", 6u);
                limiter_.params.loudness = limiterJson.value("loudness", 1000u);
            }
        } else if (json.contains("effectType")) {
            // 旧形式（単一エフェクト）からの移行: 0=None, 1=LowPass, 2=HighPass, 3=BandPass, 4=Notch, 5=Reverb
            const int legacyType = json.value("effectType", 0);
            if (legacyType >= 1 && legacyType <= 4) {
                filter_.enabled = true;
                filter_.kind = static_cast<FilterKind>(legacyType - 1);
                filter_.frequency = json.value("effectFrequency", 1.0f);
                filter_.q = json.value("effectQ", 1.0f);
            } else if (legacyType == 5) {
                reverb_.enabled = true;
                reverb_.mix = json.value("reverbMix", 0.3f);
            }
        }

        ClampEffectParams();

        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前のplayOnAwake_
        // （デフォルト値）で呼ばれてしまっているため、ここで読み込んだ実際の値を使って改めて反映する。
        // 非アクティブな場合はSetActive(true)時のInitialize()に任せる
        if (IsActive()) {
            pendingAutoPlay_ = playOnAwake_;
        }
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

    static AudioManager::FilterType ToFilterType(FilterKind kind) {
        switch (kind) {
        case FilterKind::HighPass: return AudioManager::FilterType::HighPass;
        case FilterKind::BandPass: return AudioManager::FilterType::BandPass;
        case FilterKind::Notch:    return AudioManager::FilterType::Notch;
        case FilterKind::LowPass:
        default:                   return AudioManager::FilterType::LowPass;
        }
    }

    static void LoadFloat4(const JSON &json, const char *key, float (&out)[4]) {
        if (!json.contains(key) || !json.at(key).is_array()) return;
        const auto &arrayJson = json.at(key);
        for (size_t i = 0; i < 4 && i < arrayJson.size(); ++i) {
            if (arrayJson[i].is_number()) out[i] = arrayJson[i].get<float>();
        }
    }

    /// @brief 各エフェクトのパラメータを有効範囲へクランプする
    void ClampEffectParams() {
        filter_.frequency = std::clamp(filter_.frequency, 0.0005f, 1.0f);
        filter_.q = std::clamp(filter_.q, 0.0005f, 1.5f);
        reverb_.mix = std::clamp(reverb_.mix, 0.0f, 1.0f);
        echo_.params.wetDryMix = std::clamp(echo_.params.wetDryMix, 0.0f, 1.0f);
        echo_.params.feedback = std::clamp(echo_.params.feedback, 0.0f, 1.0f);
        echo_.params.delayMs = std::clamp(echo_.params.delayMs, 1.0f, 2000.0f);
        for (int band = 0; band < 4; ++band) {
            eq_.params.frequencyCenter[band] = std::clamp(eq_.params.frequencyCenter[band], 20.0f, 20000.0f);
            eq_.params.gain[band] = std::clamp(eq_.params.gain[band], 0.126f, 7.94f);
            eq_.params.bandwidth[band] = std::clamp(eq_.params.bandwidth[band], 0.1f, 2.0f);
        }
        limiter_.params.release = std::clamp(limiter_.params.release, 1u, 20u);
        limiter_.params.loudness = std::clamp(limiter_.params.loudness, 1u, 1800u);
    }

    /// @brief フィルターとリバーブの設定に、距離・向きによる籠り具合(muffleAmount, 0=クリア～1=最大)を重ねて適用する
    /// @details フィルター無効時もspatial audioの籠り表現のためLowPassとして適用する。
    ///          リバーブは別経路(共有リバーブバス)への送り量として適用するため、フィルターと併用できる
    void ApplyFilterAndReverb(float muffleAmount) const {
        if (currentPlayHandle_ == AudioManager::kInvalidPlayHandle) return;

        const AudioManager::FilterType filterType = filter_.enabled ? ToFilterType(filter_.kind) : AudioManager::FilterType::LowPass;
        const float baseFrequency = filter_.enabled ? filter_.frequency : 1.0f;
        const float muffleFactor = 1.0f - muffleAmount * 0.95f;
        const float frequency = std::clamp(baseFrequency * muffleFactor, 0.0005f, 1.0f);
        const float oneOverQ = filter_.enabled ? filter_.q : 1.0f;
        AudioManager::SetFilter(currentPlayHandle_, filterType, frequency, oneOverQ);

        AudioManager::SetReverbSend(currentPlayHandle_, reverb_.enabled ? reverb_.mix : 0.0f);
    }

    /// @brief エフェクトチェーン系(EQ/エコー/リミッター)の設定を適用する
    /// @details 毎フレーム呼ぶ必要はなく、再生開始時と設定変更時のみ呼べばよい
    void ApplyChainEffects() const {
        if (currentPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        AudioManager::SetEqEffect(currentPlayHandle_, eq_.enabled, eq_.params);
        AudioManager::SetEchoEffect(currentPlayHandle_, echo_.enabled, echo_.params);
        AudioManager::SetLimiterEffect(currentPlayHandle_, limiter_.enabled, limiter_.params);
    }

    /// @brief 再生中であれば全エフェクトの設定を反映し直す（設定変更時用）
    void ReapplyEffects() const {
        if (currentPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        ApplyChainEffects();
        ApplyFilterAndReverb(lastMuffleAmount_);
    }

    std::string soundName_;
    mutable AudioManager::SoundHandle soundHandle_ = AudioManager::kInvalidSoundHandle;
    float volume_ = 1.0f;
    float pitch_ = 0.0f;
    bool loop_ = false;
    bool playOnAwake_ = true;
    float minDistance_ = 1.0f;
    float maxDistance_ = 25.0f;
    FilterEffect filter_;
    ReverbEffect reverb_;
    EchoEffect echo_;
    EqEffect eq_;
    LimiterEffect limiter_;
    bool enableSpatialAudio_ = false;
    /// @brief 直近のspatial audioによる籠り具合（設定変更時の再適用でフィルターへ重ねるために保持する）
    float lastMuffleAmount_ = 0.0f;
    /// @brief 次のUpdate()でplayOnAwakeによる再生を行うかどうか（ゲームループが実際に開始してから再生するため）
    bool pendingAutoPlay_ = false;

    AudioManager::PlayHandle currentPlayHandle_ = AudioManager::kInvalidPlayHandle;
    /// @brief currentPlayHandle_がAttachExternalPlayHandle()経由の外部所有ハンドルかどうか
    /// @details trueの場合、Stop()はAudioManager::Stop()を呼ばず参照を外すだけになる
    bool isExternalHandle_ = false;
};

REGISTER_COMPONENT_OBJECT(AudioSource)

} // namespace KashipanEngine
