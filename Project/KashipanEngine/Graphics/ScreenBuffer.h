#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "Core/DirectX/DX12Commands.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/DepthStencilResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/IShaderTexture.h"
#include "Graphics/IRenderTarget.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class Window;
class Renderer;

/// @brief オフスクリーンレンダリング用スクリーンバッファ
class ScreenBuffer final : public IShaderTexture, public IRenderTarget {
public:
    /// @brief GameEngine から DirectXCommon を設定
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }
    /// @brief 全 ScreenBuffer 破棄
    static void AllDestroy(Passkey<GameEngine>);
    /// @brief 破棄要求済み ScreenBuffer をフレーム終端で実際に破棄する
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief ScreenBuffer 生成
    /// @param name 管理用の名前（空の場合は自動生成。TextureManagerにこの名前で登録される）
    static ScreenBuffer *Create(std::uint32_t width, std::uint32_t height,
        const std::string &name = "",
        DXGI_FORMAT colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);
    /// @brief ポインタから存在確認
    static bool IsExist(ScreenBuffer *buffer);

    ScreenBuffer(const ScreenBuffer &) = delete;
    ScreenBuffer &operator=(const ScreenBuffer &) = delete;
    ScreenBuffer(ScreenBuffer &&) = delete;
    ScreenBuffer &operator=(ScreenBuffer &&) = delete;

    ~ScreenBuffer();

    /// @brief 指定バッファが破棄要求済みか
    bool IsPendingDestroy() const;
    /// @brief アプリ側から破棄要求（実体の破棄は CommitDestroy で行う）
    void DestroyNotify();

    //==================================================
    // IShaderTexture オーバーライド関数
    //==================================================

    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;

    //==================================================
    // IRenderTarget オーバーライド関数
    //==================================================

    RenderTargetKind GetRenderTargetKind() const noexcept override { return RenderTargetKind::ScreenBuffer; }
    std::string GetRenderTargetName() const override { return name_; }
    /// @brief 管理用の名前を設定（TextureManagerへの登録名も更新される）
    void SetRenderTargetName(const std::string &name);
    /// @brief TextureManagerに登録されているテクスチャハンドルを取得
    std::uint32_t GetTextureHandle() const noexcept { return textureHandle_; }
    std::uint32_t GetRenderTargetWidth() const noexcept override { return width_; }
    std::uint32_t GetRenderTargetHeight() const noexcept override { return height_; }
    bool IsRenderTargetAvailable() const noexcept override { return width_ > 0 && height_ > 0 && !this->IsPendingDestroy(); }

    void BeginDraw() override;
    void EndDraw() override;
    ID3D12GraphicsCommandList *GetCommandList() const override { return dx12Commands_->GetCommandList(); }

    /// @brief ポストエフェクト用のパス切り替え処理
    /// @details 現在の書き込み面をSRVとして参照可能な状態にしてバッファを進め、
    ///          次の書き込み面への描画（深度書き込みなし）を開始する。
    ///          BeginDraw と EndDraw の間でのみ呼び出すこと。
    void NextPass();

    /// @brief 現在の書き込み面を再度レンダーターゲットとして設定する
    /// @details ポストエフェクトが内部レンダーターゲットへ描画した後、
    ///          描画先をこのバッファへ戻すために呼ぶ（クリアは行わない）。
    void RebindWriteTarget();

    /// @brief 直近で描画が完了した時点のSRVハンドルを取得（ビューア表示・他コンポーネントからの
    ///        フレームをまたいだ参照用）
    /// @details GetSrvHandle()はポストエフェクトの中間パス同士が同一フレーム内で参照し合うための
    ///          「直近に完成した書き込み面」を返す（ダブルバッファの一方を指す）。一方、この関数が返す
    ///          ハンドルは専用の安定バッファ（CopyToPreviewTarget参照）を指しており、
    ///          このバッファは1フレームにつき「全ポストエフェクト適用後の最終結果」で1回だけ
    ///          上書きされる以外は変化しないため、Renderer::RenderFrameより前のタイミングで
    ///          参照しても、中間パスの内容や翌フレームの描画によって書き換えられる心配が無い
    D3D12_GPU_DESCRIPTOR_HANDLE GetPreviewSrvHandle() const noexcept {
        return (previewReady_ && previewShaderResource_) ? previewShaderResource_->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }

    /// @brief 直近で描画完了が確定した時点のレンダーターゲットを画像ファイルへ保存する
    /// @details 内部でGPU完了を同期的に待つ（スコールする）ため、毎フレーム呼ぶ用途ではなく
    ///          スクリーンショット等の単発利用を想定している。GetPreviewSrvHandleと同じ
    ///          専用の安定バッファ（CopyToPreviewTarget参照）を対象にするため、
    ///          この呼び出しがどのタイミング（エディターのボタン、スクリプトのUpdate等、
    ///          いずれもRenderer::RenderFrameより前）で行われても、
    ///          ポストエフェクト適用途中の中間結果を保存してしまうことはない。
    /// @param filePath 保存先パス（拡張子から形式判定。.png/.jpg/.jpeg/.bmp、それ以外はpng扱い）
    /// @return 成功した場合は true
    bool SaveToFile(const std::string &filePath) const;

    //==================================================
    // ハンドルとリソース取得
    //==================================================

    /// @brief Depth(読み取り面)をテクスチャ(SRV)として参照するためのハンドル（未作成なら空）
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandle() const noexcept;

    RenderTargetResource *GetRenderTarget() const noexcept { return renderTargets_[GetRtvReadIndex()].get(); }
    DepthStencilResource *GetDepthStencil() const noexcept { return depthStencils_[GetDsvReadIndex()].get(); }
    ShaderResourceResource *GetShaderResource() const noexcept { return shaderResources_[GetRtvReadIndex()].get(); }

    /// @brief 今まさに描画中（書き込み側）のレンダーターゲット/深度を取得する
    /// @details GetRenderTarget()/GetDepthStencil()はダブルバッファのうち「直前に描画が完了した
    ///          読み取り用の面」を返す（ポストエフェクトの中間パスが直前のパス結果を参照する用途）。
    ///          BeginDraw〜EndDrawの間に、通常の3Dオブジェクト描画が今まさに書き込んでいる面
    ///          （＝不透明・通常ディザ描画済みの内容を含む）を参照したい場合はこちらを使うこと
    RenderTargetResource *GetWriteRenderTarget() const noexcept { return renderTargets_[GetRtvWriteIndex()].get(); }
    DepthStencilResource *GetWriteDepthStencil() const noexcept { return depthStencils_[GetDsvWriteIndex()].get(); }

    /// @brief 深度書き込みの有効/無効を設定（ポストエフェクト用や2D描画用。次のBeginDrawから反映される）
    /// @param enable true の場合、深度書き込みを有効にする。false の場合、深度書き込みを無効にする。
    void SetDepthWriteEnabled(bool enable) noexcept { isDepthWriteEnabled_ = enable; }

    /// @brief バッファサイズの変更を通知する（GPUリソースの再生成は即時には行わない）
    /// @details 呼び出した時点では新しいサイズを記憶するだけで、実際のGPUリソース再生成は
    ///          次のBeginDraw（内部的にはBeginRecord経由のEnsureRenderTargetSize/
    ///          EnsureDepthStencilSize）でまとめて行われる。
    ///          TextureManagerへの登録名・ハンドルはそのまま維持される（サイズはこのオブジェクトから
    ///          毎回取得されるため再登録は不要）。BeginDraw/EndDraw の外側から呼ぶこと。
    /// @return 成功した場合はtrue、失敗した場合（未初期化・サイズが0等）はfalseを返す
    bool Resize(std::uint32_t width, std::uint32_t height);

private:
    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static constexpr size_t kBufferCount = 2;

    ScreenBuffer() = default;

    size_t GetRtvWriteIndex() const noexcept { return rtvWriteIndex_; }
    /// @brief 直近に完成した（＝直前のBeginRecord/NextPassで書き込みが終わった）RTVインデックスを取得する
    /// @details ポストエフェクトの中間パス同士が同一フレーム内で「直前のパスの結果」を参照するための
    ///          もので、フレームをまたいだ参照には使わないこと（GetPreviewSrvHandle参照）
    size_t GetRtvReadIndex() const noexcept { return (rtvWriteIndex_ + 1) % kBufferCount; }
    size_t GetDsvWriteIndex() const noexcept { return dsvWriteIndex_; }
    size_t GetDsvReadIndex() const noexcept { return (dsvWriteIndex_ + 1) % kBufferCount; }

    void AdvanceFrameBufferIndex(bool updateRtv, bool updateDsv) noexcept {
        if (updateRtv) {
            rtvWriteIndex_ = (rtvWriteIndex_ + 1) % kBufferCount;
        }
        if (updateDsv) {
            dsvWriteIndex_ = (dsvWriteIndex_ + 1) % kBufferCount;
        }
    }

    /// @brief リソース初期化
    bool Initialize(std::uint32_t width, std::uint32_t height,
        DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat);

    /// @brief 破棄
    void Destroy();

    /// @brief コマンド記録開始
    /// @param isMainCall BeginDraw経由（true）かNextPass経由（false）か。
    ///        リサイズ待ちバッファの作り直し可否の判定に使う（EnsureRenderTargetSize参照）
    ID3D12GraphicsCommandList *BeginRecord(bool disableDepthWrite, bool isMainCall);

    /// @brief コマンド記録終了
    bool EndRecord(bool discard = false);

    /// @brief RenderTarget/ShaderResourceが現在の width_/height_ と異なるサイズの場合のみ、
    ///        ダブルバッファの両方のインデックスを同時に新しいサイズで作り直す
    /// @param isMainCall BeginDraw経由の呼び出しか
    /// @details 作り直しはBeginDraw経由（isMainCall=true）でのみ行い、NextPass経由（ポストエフェクトの
    ///          中間パス）では絶対に行わない。エンジンは毎フレーム完全にGPU同期する（WaitForFence）ため、
    ///          新しいフレームのBeginDraw時点では前フレームのGPU処理は必ず完了しており、
    ///          ダブルバッファの両方を安全に作り直せる。一方、NextPass（同一フレーム内の中間パス）は
    ///          直前のパスの結果をまだ参照中のため、ここで作り直すとGPUハング
    ///          （TDR・スワップチェーンPresent失敗）を引き起こすことを確認済み
    void EnsureRenderTargetSize(ID3D12GraphicsCommandList *cmd, bool isMainCall);
    /// @brief DepthStencilが現在の width_/height_ と異なるサイズの場合のみ、
    ///        ダブルバッファの両方のインデックスを同時に新しいサイズで作り直す
    /// @details EnsureRenderTargetSizeと同じ理由でBeginDraw経由（isMainCall=true）でのみ呼ばれる
    void EnsureDepthStencilSize(ID3D12GraphicsCommandList *cmd);

    /// @brief このフレームの最終描画結果（GetRtvReadIndex()が指す面）を、
    ///        プレビュー用の安定バッファへコピーする（GetPreviewSrvHandle参照）
    /// @details EndDraw()の中、コマンドリストをCloseする直前（＝まだ記録中）に呼ぶこと。
    ///          一度Closeされたコマンドリストは同一フレーム内で再度開けない
    ///          （BeginRecordはアロケータをResetするため、GPU実行完了前に呼ぶと不正になる）ため、
    ///          パイプライン経由の描画ではなく、同じコマンドリスト上でCopyResourceを使う
    void CopyToPreviewTarget(ID3D12GraphicsCommandList *cmd);

    /// @brief TextureManager への登録（名前が空の場合は自動生成）
    void RegisterToTextureManager(const std::string &name);
    /// @brief TextureManager からの登録解除
    void UnregisterFromTextureManager();

    std::string name_;
    std::uint32_t textureHandle_ = 0;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    DXGI_FORMAT colorFormat_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;

    size_t rtvWriteIndex_ = 0;
    size_t dsvWriteIndex_ = 0;

    bool isDepthWriteEnabled_ = true;
    bool isLastBeginDisableDepthWrite_ = false;
    bool isFirstBeginRecord_ = true;
    // previewShaderResource_ / shaderResources_ はInitialize直後から有効なディスクリプタを
    // 返せてしまうが、実体（RenderTargetResource）はRENDER_TARGET状態で生成されるため、
    // まだ一度もBeginDraw/EndDrawを経ていない段階でSRVとして参照するとD3D12の状態検証エラー
    // となる（Debug構成での初回フレームクラッシュの原因）。previewReady_はCopyToPreviewTarget
    // が最初に成功した時点でtrueになり、GetPreviewSrvHandle()はそれまで空ハンドルを返す
    bool previewReady_ = false;

    std::unique_ptr<RenderTargetResource> renderTargets_[kBufferCount];
    std::unique_ptr<DepthStencilResource> depthStencils_[kBufferCount];
    std::unique_ptr<ShaderResourceResource> shaderResources_[kBufferCount];

    // ダブルバッファ（renderTargets_/depthStencils_）が実際にGPU上で確保されているサイズ
    // （width_/height_とは異なる場合がある。Resize()は即時にはGPUリソースを作り直さず、
    // このサイズとの差分をEnsureRenderTargetSize/EnsureDepthStencilSizeで検知して、
    // BeginDraw経由でのみ両方のインデックスをまとめて作り直す）
    std::uint32_t rtBufferWidth_ = 0;
    std::uint32_t rtBufferHeight_ = 0;
    std::uint32_t dsBufferWidth_ = 0;
    std::uint32_t dsBufferHeight_ = 0;

    /// @brief ビューア表示・フレームをまたいだ参照専用の安定バッファ（GetPreviewSrvHandle参照）
    /// @details ダブルバッファ（renderTargets_）とは独立しており、EndDraw()内のCopyToPreviewTarget
    ///          呼び出しでのみ内容が更新される
    std::unique_ptr<RenderTargetResource> previewTarget_;
    std::unique_ptr<ShaderResourceResource> previewShaderResource_;
    std::uint32_t previewBufferWidth_ = 0;
    std::uint32_t previewBufferHeight_ = 0;
    /// @brief リサイズで置き換えられた直前のプレビューバッファ（1フレームだけ延命）
    /// @details ImGuiのウィンドウ構築（GetPreviewSrvHandle呼び出し）はRenderer::RenderFrameより前に
    ///          行われるが、その描画コマンド自体はimguiManager_->Render()（RenderFrameより後）まで
    ///          実行されない。そのため、このフレーム中にCopyToPreviewTargetがprevewTarget_を
    ///          直接作り直して即座に破棄すると、既にImGuiの描画コマンドに埋め込まれた古い
    ///          ディスクリプタが破棄済みリソースを指してしまい、GPUハングを引き起こす
    ///          （ダブルバッファでNextPass経由の作り直しを禁止したのと同種の問題）。
    ///          そのため古い方はここに1フレームだけ退避し、次にCopyToPreviewTargetが呼ばれた際
    ///          （＝前フレームのGPU処理が完全に同期済みであることが保証された後）に破棄する
    std::unique_ptr<RenderTargetResource> previewTargetPendingDestroy_;
    std::unique_ptr<ShaderResourceResource> previewShaderResourcePendingDestroy_;

    int commandSlotIndex_ = -1;
    DX12Commands *dx12Commands_ = nullptr;
};

} // namespace KashipanEngine
