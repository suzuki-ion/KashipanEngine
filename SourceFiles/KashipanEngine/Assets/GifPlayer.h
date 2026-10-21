#pragma once
#include <cstdint>
#include <memory>

#include "Assets/GifManager.h"
#include "Assets/TextureManager.h"

namespace KashipanEngine {

class DirectXCommon;
class GifTexture;

/// @brief GIFの再生インスタンス
/// @details フレーム送りのタイミング管理と、表示用`GifTexture`・`TextureManager`への外部テクスチャ
///          登録のライフサイクル管理を担う。`VideoPlayer`と異なり非同期デコード・ストリーミングが
///          無いため、専用マネージャーによるプーリングや毎フレームUpdate呼び出しの一元化は不要。
///          呼び出し元（`GifSource`コンポーネント、Assetsウィンドウのプレビューウィンドウ等）が
///          直接所有し、自身の更新タイミングで`Update()`を呼ぶだけでよい。
///          このロジックをコンポーネントとプレビューウィンドウの双方で個別に持つと、フレーム送り
///          アルゴリズムを直すときに一方だけ直し忘れる恐れがあるため、ここへ集約している。
class GifPlayer final {
public:
    GifPlayer(DirectXCommon *directXCommon, GifManager::GifHandle handle);
    ~GifPlayer();

    GifPlayer(const GifPlayer &) = delete;
    GifPlayer &operator=(const GifPlayer &) = delete;
    GifPlayer(GifPlayer &&) = delete;
    GifPlayer &operator=(GifPlayer &&) = delete;

    /// @brief 有効なGIF（1フレーム以上デコード済み）を指しているか
    bool IsValid() const noexcept;

    /// @brief 最初から再生する
    /// @return 成功した場合 true
    bool Play(bool loop);
    /// @brief 停止する（再生位置を先頭へ戻す。テクスチャには最初のフレームが表示され続ける）
    void Stop();
    /// @brief 一時停止する
    bool Pause();
    /// @brief 一時停止を解除する
    bool Resume();
    bool IsPlaying() const noexcept { return isPlaying_ && !isPaused_; }
    bool IsPaused() const noexcept { return isPlaying_ && isPaused_; }

    /// @brief 再生していない間、最初の1フレームをプレースホルダーとして表示する
    /// @return 成功した場合 true
    bool ShowFirstFrame();

    /// @brief 毎フレーム呼ぶ（再生中でなければ何もしない）
    /// @param deltaTime 経過秒数
    void Update(float deltaTime);

    /// @brief 表示用テクスチャのハンドル（`TextureManager::RegisterExternalTexture`で登録済み）
    TextureManager::TextureHandle GetTextureHandle() const noexcept { return textureHandle_; }
    std::uint32_t GetWidth() const noexcept;
    std::uint32_t GetHeight() const noexcept;

private:
    bool EnsureTexture();
    void UploadCurrentFrame();

    DirectXCommon *directXCommon_ = nullptr;
    GifManager::GifHandle handle_ = GifManager::kInvalidHandle;

    std::unique_ptr<GifTexture> texture_;
    TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;

    bool loop_ = true;
    bool isPlaying_ = false;
    bool isPaused_ = false;
    std::size_t currentFrameIndex_ = 0;
    float frameElapsed_ = 0.0f;
};

} // namespace KashipanEngine
