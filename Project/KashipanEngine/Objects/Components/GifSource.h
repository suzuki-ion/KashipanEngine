#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "Assets/GifManager.h"
#include "Assets/GifPlayer.h"
#include "Assets/MaterialManager.h"
#include "Assets/TextureManager.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

/// @brief アニメーションGIFを再生するコンポーネント
/// @details 使用するGIF・ループ・自動再生の設定を持つ。同じオブジェクトに
///          SpriteRenderer/MeshRendererが付いている場合、再生開始時にそのレンダラーが
///          使用しているマテリアルを複製し、複製したマテリアルのテクスチャを再生中のGIFへ
///          差し替えてレンダラーへ割り当てる（複製するのは、複数オブジェクトで共有されている
///          可能性のある元のマテリアルアセット自体を書き換えないため）。停止時は元のマテリアルへ戻す。
///          フレーム送り・表示用テクスチャのライフサイクル管理は`GifPlayer`（Assetsウィンドウの
///          GIFプレビューウィンドウとも共通のロジック）に委譲し、このクラスはレンダラーへの
///          適用まわりだけを担う。
class GifSource final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(GifSource, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(gifAssetPath_, [this] { gifHandle_ = GifManager::kInvalidHandle; });
        ADD_MEMBER_VARIABLE(loop_);
        ADD_MEMBER_VARIABLE(playOnAwake_);
    )
    COMPONENT_CATEGORY("Animation")
    // Finalize()が何らかの理由で呼ばれずに破棄される場合の保険。Stop()経由だと再生していない場合に
    // 新規GifPlayerを作ってしまいそれが破棄されずに残るため、後始末のみ行うDestroyCurrentPlayer()を呼ぶ
    // （VideoSource::~VideoSourceと同じ理由）
    ~GifSource() override { DestroyCurrentPlayer(); }

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<GifSource>();
        ptr->gifAssetPath_ = gifAssetPath_;
        ptr->loop_ = loop_;
        ptr->playOnAwake_ = playOnAwake_;
        return ptr;
    }

    //==================================================
    // 再生制御
    //==================================================

    /// @brief 再生する（既に再生中の場合は最初から再生し直す）
    /// @return 成功した場合 true
    bool Play() {
        if (!EnsurePlayer()) return false;
        if (!player_->Play(loop_)) return false;
        ApplyToRenderer();
        return true;
    }

    /// @brief 停止する（レンダラーには、GIFの最初の1フレームを表示し続ける）
    void Stop() {
        if (player_) player_->Stop();
        ShowFirstFramePreview();
    }

    /// @brief 一時停止する
    bool Pause() { return player_ && player_->Pause(); }
    /// @brief 一時停止を解除する
    bool Resume() { return player_ && player_->Resume(); }
    /// @brief 再生中かどうか
    bool IsPlaying() const { return player_ && player_->IsPlaying(); }
    /// @brief 一時停止中かどうか
    bool IsPaused() const { return player_ && player_->IsPaused(); }

    //==================================================
    // プロパティ
    //==================================================

    void SetGifAssetPath(const std::string &gifAssetPath) {
        gifAssetPath_ = gifAssetPath;
        gifHandle_ = GifManager::kInvalidHandle;
    }
    const std::string &GetGifAssetPath() const noexcept { return gifAssetPath_; }

    void SetLoop(bool loop) { loop_ = loop; }
    bool GetLoop() const noexcept { return loop_; }

    void SetPlayOnAwake(bool playOnAwake) { playOnAwake_ = playOnAwake; }
    bool GetPlayOnAwake() const noexcept { return playOnAwake_; }

protected:
    void Initialize() override {
        // Initialize()はシーン読み込み・エディター編集時にも走る（ゲームループ開始前）ため、
        // ここで即座にPlay()すると実際のゲーム開始前に再生が進んでしまう。VideoSourceと同様に
        // 最初のUpdate()（ゲームループが実際に回っているときのみ呼ばれる）まで再生を遅延させ、
        // それまではGIFの最初の1フレームをプレースホルダーとして表示しておく
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

        if (player_) player_->Update(GetDeltaTime());
    }

#if defined(USE_IMGUI)
    void ShowPersistentImGui() override {
        RetryApplyToRendererIfNeeded();
    }

    void ShowImGui() override {
        std::vector<std::string> gifPaths = GifManager::GetLoadedGifAssetPaths();
        bool gifChanged = false;
        if (ImGuiCustom::SelectString(TranslationLabel("component.gifsource.gif"), gifAssetPath_, gifPaths, true)) {
            gifHandle_ = GifManager::kInvalidHandle;
            gifChanged = true;
        }
        // Assetsウィンドウからのテクスチャ（GIFはテクスチャ扱い）ドラッグ&ドロップも受け付ける
        if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
            const bool isKnownGif = GifManager::GetGifHandleFromAssetPath(droppedPath) != GifManager::kInvalidHandle;
            const bool hasGifExtension = droppedPath.size() >= 4 && droppedPath.compare(droppedPath.size() - 4, 4, ".gif") == 0;
            if (isKnownGif || hasGifExtension) {
                gifAssetPath_ = droppedPath;
                gifHandle_ = GifManager::kInvalidHandle;
                gifChanged = true;
            }
        }
        if (gifChanged && !IsPlaying() && !IsPaused()) {
            Stop();
        }

        ImGui::Checkbox(TranslationLabel("component.gifsource.loop"), &loop_);
        ImGui::Checkbox(TranslationLabel("component.gifsource.play_on_awake"), &playOnAwake_);

        ImGui::Separator();
        if (!IsPlaying()) {
            if (ImGui::Button(TranslationLabel("component.gifsource.play"))) Play();
        } else if (IsPaused()) {
            if (ImGui::Button(TranslationLabel("component.gifsource.resume"))) Resume();
        } else {
            if (ImGui::Button(TranslationLabel("component.gifsource.pause"))) Pause();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsPlaying() && !IsPaused());
        if (ImGui::Button(TranslationLabel("component.gifsource.stop"))) Stop();
        ImGui::EndDisabled();
    }
#endif

    JSON SaveToJson() const override {
        return JSON{
            {"gifAssetPath", gifAssetPath_}, {"loop", loop_}, {"playOnAwake", playOnAwake_}
        };
    }

    bool LoadFromJson(const JSON &json) override {
        gifAssetPath_ = json.value("gifAssetPath", std::string{});
        gifHandle_ = GifManager::kInvalidHandle;
        loop_ = json.value("loop", false);
        playOnAwake_ = json.value("playOnAwake", true);

        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前の
        // gifAssetPath_/playOnAwake_（空文字・デフォルト値）で呼ばれてしまっているため、
        // ここで読み込んだ実際の値を使って改めてプレビュー表示・自動再生予約をやり直す。
        // 非アクティブな場合はSetActive(true)時のInitialize()に任せる
        if (IsActive()) {
            pendingAutoPlay_ = playOnAwake_;
            Stop();
        }
        return true;
    }

private:
    GifManager::GifHandle ResolveGifHandle() const {
        if (gifHandle_ == GifManager::kInvalidHandle && !gifAssetPath_.empty()) {
            gifHandle_ = GifManager::GetGifHandleFromAssetPath(gifAssetPath_);
            if (gifHandle_ == GifManager::kInvalidHandle) {
                gifHandle_ = GifManager::GetGifHandleFromFileName(gifAssetPath_);
            }
        }
        return gifHandle_;
    }

    /// @brief 指定GIFを指す`GifPlayer`が用意されていることを保証する
    /// @details GIFを別のものへ切り替えた場合（ハンドルが変わった場合）は作り直す
    bool EnsurePlayer() {
        const auto handle = ResolveGifHandle();
        if (handle == GifManager::kInvalidHandle) return false;

        if (player_ && currentPlayerHandle_ != handle) {
            GifManager::DestroyPlayer(std::move(player_));
        }
        if (!player_) {
            player_ = GifManager::CreatePlayer(handle);
            currentPlayerHandle_ = handle;
        }
        return player_ != nullptr;
    }

    /// @brief GIFが指定されていて、かつ再生していない場合に、最初の1フレームを
    ///        プレースホルダーとしてレンダラーへ表示する
    void ShowFirstFramePreview() {
        if (!EnsurePlayer()) return;
        if (!player_->ShowFirstFrame()) return;
        ApplyToRenderer();
    }

    /// @brief レンダラーへの反映がまだ・または外れてしまっている場合に再試行する
    /// @details VideoSource::RetryApplyToRendererIfNeededと同じ理由（追加順・読み込み順の都合による保険）
    void RetryApplyToRendererIfNeeded() {
        if (!player_) return;
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

    /// @brief 同じオブジェクトのSpriteRenderer/MeshRendererへ、GIFのテクスチャを適用する
    void ApplyToRenderer() {
        if (!player_) return;
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

        if (auto *material = MaterialManager::GetMaterial(overrideMaterialHandle_)) {
            material->textureHandle = player_->GetTextureHandle();
        }

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

    /// @brief 再生中のGifPlayerを破棄し、レンダラーのマテリアルも元へ戻す
    /// @details RevertRenderer()はレンダラーへ反映済み（overrideMaterialHandle_が有効）な場合のみ
    ///          マテリアルを元へ戻す処理のため、GifPlayer自体の解放はレンダラーの有無に関わらず
    ///          必ず行えるよう分けて呼ぶ
    void DestroyCurrentPlayer() {
        RevertRenderer();
        GifManager::DestroyPlayer(std::move(player_));
    }

    std::string gifAssetPath_;
    mutable GifManager::GifHandle gifHandle_ = GifManager::kInvalidHandle;
    bool loop_ = true;
    bool playOnAwake_ = true;

    /// @brief 次のUpdate()でplayOnAwakeによる再生を行うかどうか（ゲームループが実際に開始してから再生するため）
    bool pendingAutoPlay_ = false;

    std::unique_ptr<GifPlayer> player_;
    GifManager::GifHandle currentPlayerHandle_ = GifManager::kInvalidHandle;

    // このコンポーネント専用の複製マテリアル（他オブジェクトと共有される元のマテリアルアセットは書き換えない）
    const std::string overrideMaterialName_ = "__GifSourceMaterial_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    MaterialManager::MaterialHandle overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
    MaterialManager::MaterialHandle originalMaterialHandle_ = MaterialManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(GifSource)

} // namespace KashipanEngine
