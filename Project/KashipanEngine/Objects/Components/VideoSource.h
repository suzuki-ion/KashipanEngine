#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#include "Assets/AudioManager.h"
#include "Assets/MaterialManager.h"
#include "Assets/VideoManager.h"
#include "Assets/VideoPlayer.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 動画を再生するコンポーネント（Unityの Video Player に相当）
/// @details 使用する動画・ループ・音量・自動再生の設定を持つ。同じオブジェクトに
///          SpriteRenderer/MeshRendererが付いている場合、再生開始時にそのレンダラーが
///          使用しているマテリアルを複製し、複製したマテリアルのテクスチャを再生中の動画へ
///          差し替えてレンダラーへ割り当てる（複製するのは、複数オブジェクトで共有されている
///          可能性のある元のマテリアルアセット自体を書き換えないため）。停止時は元のマテリアルへ戻す。
class VideoSource final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(VideoSource, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(videoAssetPath_, [this] { videoHandle_ = VideoManager::kInvalidHandle; });
        ADD_MEMBER_VARIABLE(loop_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(volume_, [this] { volume_ = std::clamp(volume_, 0.0f, 1.0f); });
        ADD_MEMBER_VARIABLE(playOnAwake_);
        ADD_MEMBER_VARIABLE(routeAudioToAudioSource_);
    )
    COMPONENT_CATEGORY("Video")
    // 破棄時にStop()を呼ぶとプレースホルダー用の新しいプレイヤーを作ってしまい、
    // それが破棄されずに残ってしまうため、後始末のみ行うDestroyCurrentPlayer()を呼ぶ
    ~VideoSource() override { DestroyCurrentPlayer(); }

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<VideoSource>();
        ptr->videoAssetPath_ = videoAssetPath_;
        ptr->loop_ = loop_;
        ptr->volume_ = volume_;
        ptr->playOnAwake_ = playOnAwake_;
        ptr->routeAudioToAudioSource_ = routeAudioToAudioSource_;
        return ptr;
    }

    //==================================================
    // 再生制御
    //==================================================

    /// @brief 再生する（既に再生中の場合は停止してから再生し直す）
    /// @return 成功した場合 true
    bool Play() {
        const auto videoHandle = ResolveVideoHandle();
        if (videoHandle == VideoManager::kInvalidHandle) return false;

        DestroyCurrentPlayer();

        currentPlayer_ = VideoManager::CreatePlayer(videoHandle);
        if (!currentPlayer_) return false;
        if (!currentPlayer_->Play(loop_, volume_)) {
            VideoManager::DestroyPlayer(currentPlayer_);
            currentPlayer_ = nullptr;
            // 再生に失敗した場合も、動画自体は指定されているので最初の1フレームは表示しておく
            ShowFirstFramePreview();
            return false;
        }

        ApplyToRenderer();
        if (routeAudioToAudioSource_) {
            if (auto *audioSource = ResolveAudioSource()) {
                audioSource->AttachExternalPlayHandle(currentPlayer_->GetAudioPlayHandle());
            }
        }
        return true;
    }

    /// @brief 停止する（レンダラーには、動画の最初の1フレームを表示し続ける）
    void Stop() {
        DestroyCurrentPlayer();
        ShowFirstFramePreview();
    }

    /// @brief 一時停止する
    bool Pause() {
        if (!currentPlayer_) return false;
        currentPlayer_->Pause();
        return true;
    }
    /// @brief 一時停止を解除する
    bool Resume() {
        if (!currentPlayer_) return false;
        currentPlayer_->Resume();
        return true;
    }
    /// @brief 再生中かどうか
    bool IsPlaying() const { return currentPlayer_ && currentPlayer_->IsPlaying(); }
    /// @brief 一時停止中かどうか
    bool IsPaused() const { return currentPlayer_ && currentPlayer_->IsPaused(); }

    //==================================================
    // プロパティ
    //==================================================

    void SetVideoAssetPath(const std::string &videoAssetPath) {
        videoAssetPath_ = videoAssetPath;
        videoHandle_ = VideoManager::kInvalidHandle;
    }
    const std::string &GetVideoAssetPath() const noexcept { return videoAssetPath_; }

    void SetLoop(bool loop) { loop_ = loop; }
    bool GetLoop() const noexcept { return loop_; }

    void SetVolume(float volume) { volume_ = std::clamp(volume, 0.0f, 1.0f); }
    float GetVolume() const noexcept { return volume_; }

    void SetPlayOnAwake(bool playOnAwake) { playOnAwake_ = playOnAwake; }
    bool GetPlayOnAwake() const noexcept { return playOnAwake_; }

    /// @brief 動画の音声を、同じオブジェクトのAudioSourceへ出力する（距離減衰・パン・エフェクトを適用したい場合）かどうかを設定する
    /// @details 再生開始時に、同じオブジェクトのAudioSource::AttachExternalPlayHandle()へ
    ///          動画の音声再生ハンドルを渡す。AudioSourceが無い場合は何もしない
    void SetRouteAudioToAudioSource(bool routeAudioToAudioSource) { routeAudioToAudioSource_ = routeAudioToAudioSource; }
    bool GetRouteAudioToAudioSource() const noexcept { return routeAudioToAudioSource_; }

protected:
    void Initialize() override {
        // Initialize()はシーン読み込み・エディター編集時にも走る（ゲームループ開始前）ため、
        // ここで即座にPlay()すると再生開始（PlayStart）前に鳴り始めてしまい、実際の再生開始時には
        // 既に再生済み/終了済みになってしまう。実際にゲームが動き出すまで待つため、
        // 最初のUpdate()（ゲームループが実際に回っているときのみ呼ばれる）まで再生を遅延させる。
        // 再生されるまでの間は、動画の最初の1フレームをプレースホルダーとして表示しておく
        pendingAutoPlay_ = playOnAwake_;
        ShowFirstFramePreview();
    }

    void Finalize() override {
        DestroyCurrentPlayer();
    }

    void Update() override {
        if (pendingAutoPlay_) {
            pendingAutoPlay_ = false;
            Play();
        }
        RetryApplyToRendererIfNeeded();
    }

#if defined(USE_IMGUI)
    /// @brief ゲームループが停止/一時停止中でも毎フレーム呼ばれる（Update()は呼ばれないため、
    ///        エディターで停止中にレンダラーへのプレビュー反映をリトライするのに使う）
    void ShowPersistentImGui() override {
        RetryApplyToRendererIfNeeded();
    }

    void ShowImGui() override {
        std::vector<std::string> videoPaths = VideoManager::GetLoadedVideoAssetPaths();
        bool videoChanged = false;
        if (ImGuiCustom::SelectString(TranslationLabel("component.videosource.video"), videoAssetPath_, videoPaths, true)) {
            videoHandle_ = VideoManager::kInvalidHandle;
            videoChanged = true;
        }
        // Assetsウィンドウからの動画ファイルドラッグ&ドロップも受け付ける
        if (std::string droppedPath; AcceptAssetDragDropTarget(kVideoAssetDragDropType, droppedPath)) {
            videoAssetPath_ = droppedPath;
            videoHandle_ = VideoManager::kInvalidHandle;
            videoChanged = true;
        }
        // 再生中でなければ、選び直した動画の最初の1フレームへプレビューを更新する
        if (videoChanged && !IsPlaying() && !IsPaused()) {
            DestroyCurrentPlayer();
            ShowFirstFramePreview();
        }

        ImGui::Checkbox(TranslationLabel("component.videosource.loop"), &loop_);
        ImGui::DragFloat(TranslationLabel("component.videosource.volume"), &volume_, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox(TranslationLabel("component.videosource.play_on_awake"), &playOnAwake_);
        ImGui::Checkbox(TranslationLabel("component.videosource.route_audio_to_audiosource"), &routeAudioToAudioSource_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.videosource.route_audio_to_audiosource_desc"));
        }

        ImGui::Separator();
        if (!IsPlaying()) {
            if (ImGui::Button(TranslationLabel("component.videosource.play"))) Play();
        } else if (IsPaused()) {
            if (ImGui::Button(TranslationLabel("component.videosource.resume"))) Resume();
        } else {
            if (ImGui::Button(TranslationLabel("component.videosource.pause"))) Pause();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsPlaying() && !IsPaused());
        if (ImGui::Button(TranslationLabel("component.videosource.stop"))) Stop();
        ImGui::EndDisabled();
    }
#endif

    JSON SaveToJson() const override {
        return JSON{
            {"videoAssetPath", videoAssetPath_}, {"loop", loop_},
            {"volume", volume_}, {"playOnAwake", playOnAwake_},
            {"routeAudioToAudioSource", routeAudioToAudioSource_}
        };
    }

    bool LoadFromJson(const JSON &json) override {
        videoAssetPath_ = json.value("videoAssetPath", std::string{});
        videoHandle_ = VideoManager::kInvalidHandle;
        loop_ = json.value("loop", false);
        volume_ = json.value("volume", 1.0f);
        playOnAwake_ = json.value("playOnAwake", true);
        routeAudioToAudioSource_ = json.value("routeAudioToAudioSource", false);

        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前の
        // videoAssetPath_/playOnAwake_（空文字・デフォルト値）で呼ばれてしまっているため、
        // ここで読み込んだ実際の値を使って改めてプレビュー表示・自動再生予約をやり直す。
        // 非アクティブな場合はSetActive(true)時のInitialize()に任せる
        if (IsActive()) {
            pendingAutoPlay_ = playOnAwake_;
            DestroyCurrentPlayer();
            ShowFirstFramePreview();
        }
        return true;
    }

private:
    /// @brief 同じオブジェクトのAudioSourceを取得する（無ければnullptr）
    AudioSource *ResolveAudioSource() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext ? objectContext->GetComponent<AudioSource>() : nullptr;
    }

    VideoManager::VideoHandle ResolveVideoHandle() const {
        if (videoHandle_ == VideoManager::kInvalidHandle && !videoAssetPath_.empty()) {
            videoHandle_ = VideoManager::GetVideoHandleFromAssetPath(videoAssetPath_);
            if (videoHandle_ == VideoManager::kInvalidHandle) {
                videoHandle_ = VideoManager::GetVideoHandleFromFileName(videoAssetPath_);
            }
        }
        return videoHandle_;
    }

    /// @brief 再生中のプレイヤーを破棄し、レンダラーのマテリアルも元へ戻す（プレースホルダーの再表示は行わない）
    void DestroyCurrentPlayer() {
        RevertRenderer();
        if (routeAudioToAudioSource_) {
            if (auto *audioSource = ResolveAudioSource()) {
                audioSource->Stop();
            }
        }
        if (currentPlayer_) {
            VideoManager::DestroyPlayer(currentPlayer_);
            currentPlayer_ = nullptr;
        }
    }

    /// @brief 動画が指定されていて、かつ何も再生・表示していない場合に、最初の1フレームを
    ///        プレースホルダーとしてレンダラーへ表示する
    /// @details ゲームループ開始前（エディター編集中等）や、再生停止後に、動画の最初の1フレームを
    ///          表示し続けたい場合に使う。既にプレイヤーが存在する（再生中・プレビュー表示中を問わず）場合は何もしない
    void ShowFirstFramePreview() {
        if (currentPlayer_) return;
        const auto videoHandle = ResolveVideoHandle();
        if (videoHandle == VideoManager::kInvalidHandle) return;

        currentPlayer_ = VideoManager::CreatePlayer(videoHandle);
        if (!currentPlayer_) return;
        if (!currentPlayer_->ShowFirstFrame()) {
            VideoManager::DestroyPlayer(currentPlayer_);
            currentPlayer_ = nullptr;
            return;
        }
        ApplyToRenderer();
    }

    /// @brief レンダラーへの反映がまだ・または外れてしまっている場合に再試行する
    /// @details 2つのケースに対応する保険:
    ///          (1) SpriteRenderer/MeshRendererがVideoSourceより後から同じオブジェクトへ追加された場合等、
    ///              追加順の都合でApplyToRenderer()がまだレンダラーへ反映できていないケース
    ///          (2) シーン読み込み時、VideoSourceが自分のLoadFromJson()でオーバーライドを反映した直後に、
    ///              同じオブジェクトのSpriteRenderer/MeshRenderer自身のLoadFromJson()が
    ///              （読み込み順でVideoSourceより後だった場合）マテリアルハンドルを自分のJSONデータで
    ///              上書きしてしまい、せっかく反映したオーバーライドが外れてしまうケース
    ///          ゲームループ停止中はUpdate()が呼ばれないため、エディターではShowPersistentImGui()からも呼ぶ
    void RetryApplyToRendererIfNeeded() {
        if (!currentPlayer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *meshRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<MeshRenderer>();
        if (!spriteRenderer && !meshRenderer) return;

        const auto currentHandle = spriteRenderer ? spriteRenderer->GetMaterialHandle() : meshRenderer->GetMaterialHandle();
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle || currentHandle != overrideMaterialHandle_) {
            ApplyToRenderer();
        }
    }

    /// @brief 同じオブジェクトのSpriteRenderer/MeshRendererへ、再生中の動画テクスチャを適用する
    /// @details 元のマテリアルを複製して専用インスタンスを作り、そのtextureHandleだけを
    ///          動画のテクスチャで上書きする（元のマテリアルアセットは書き換えない）
    void ApplyToRenderer() {
        if (!currentPlayer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *meshRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<MeshRenderer>();
        if (!spriteRenderer && !meshRenderer) return;

        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) {
            originalMaterialHandle_ = spriteRenderer ? spriteRenderer->GetMaterialHandle() : meshRenderer->GetMaterialHandle();

            MaterialManager::Material overrideMaterial{};
            if (auto *baseMaterial = MaterialManager::GetMaterial(originalMaterialHandle_)) {
                overrideMaterial = *baseMaterial;
            }
            overrideMaterial.name = overrideMaterialName_;
            overrideMaterialHandle_ = MaterialManager::RegisterMaterial(overrideMaterialName_, overrideMaterial);
        }
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) return;

        currentPlayer_->ConfigureMaterial(overrideMaterialHandle_);

        if (spriteRenderer) spriteRenderer->SetMaterialHandle(overrideMaterialHandle_);
        if (meshRenderer) meshRenderer->SetMaterialHandle(overrideMaterialHandle_);
    }

    /// @brief 適用していたレンダラーのマテリアルを元へ戻し、複製した専用マテリアルを破棄する
    void RevertRenderer() {
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) return;

        if (auto *objectContext = GetOwnerObjectContext()) {
            if (auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>()) {
                spriteRenderer->SetMaterialHandle(originalMaterialHandle_);
            } else if (auto *meshRenderer = objectContext->GetComponent<MeshRenderer>()) {
                meshRenderer->SetMaterialHandle(originalMaterialHandle_);
            }
        }

        MaterialManager::RemoveMaterial(overrideMaterialName_);
        overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
        originalMaterialHandle_ = MaterialManager::kInvalidHandle;
    }

    std::string videoAssetPath_;
    mutable VideoManager::VideoHandle videoHandle_ = VideoManager::kInvalidHandle;
    bool loop_ = false;
    float volume_ = 1.0f;
    bool playOnAwake_ = true;
    bool routeAudioToAudioSource_ = false;

    /// @brief 次のUpdate()でplayOnAwakeによる再生を行うかどうか（ゲームループが実際に開始してから再生するため）
    bool pendingAutoPlay_ = false;

    VideoPlayer *currentPlayer_ = nullptr;

    // このコンポーネント専用の複製マテリアル（他オブジェクトと共有される元のマテリアルアセットは書き換えない）
    const std::string overrideMaterialName_ = "__VideoSourceMaterial_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    MaterialManager::MaterialHandle overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
    MaterialManager::MaterialHandle originalMaterialHandle_ = MaterialManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(VideoSource)

} // namespace KashipanEngine
