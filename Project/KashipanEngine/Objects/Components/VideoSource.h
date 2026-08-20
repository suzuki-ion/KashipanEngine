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
    ~VideoSource() override { Stop(); }

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

        Stop();

        currentPlayer_ = VideoManager::CreatePlayer(videoHandle);
        if (!currentPlayer_) return false;
        if (!currentPlayer_->Play(loop_, volume_)) {
            VideoManager::DestroyPlayer(currentPlayer_);
            currentPlayer_ = nullptr;
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

    /// @brief 停止する（レンダラーのマテリアルも元へ戻す）
    void Stop() {
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
        if (playOnAwake_) Play();
    }

    void Finalize() override {
        Stop();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        std::vector<std::string> videoPaths = VideoManager::GetLoadedVideoAssetPaths();
        if (ImGuiCustom::SelectString(TranslationLabel("component.videosource.video"), videoAssetPath_, videoPaths, true)) {
            videoHandle_ = VideoManager::kInvalidHandle;
        }
        // Assetsウィンドウからの動画ファイルドラッグ&ドロップも受け付ける
        if (std::string droppedPath; AcceptAssetDragDropTarget(kVideoAssetDragDropType, droppedPath)) {
            videoAssetPath_ = droppedPath;
            videoHandle_ = VideoManager::kInvalidHandle;
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

    VideoPlayer *currentPlayer_ = nullptr;

    // このコンポーネント専用の複製マテリアル（他オブジェクトと共有される元のマテリアルアセットは書き換えない）
    const std::string overrideMaterialName_ = "__VideoSourceMaterial_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    MaterialManager::MaterialHandle overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
    MaterialManager::MaterialHandle originalMaterialHandle_ = MaterialManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(VideoSource)

} // namespace KashipanEngine
