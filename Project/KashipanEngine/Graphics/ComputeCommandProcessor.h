#pragma once
#include <d3d12.h>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class DX12Commands;
class Renderer;

/// @brief 1フレーム内のComputeシェーダー処理を1本のコマンドリストへ集約するクラス
/// @details ComputeShaderProcessing、GPUスキニング、GPUパーティクル、ライトカリングを
///          呼び出し順にまとめて記録し、全Computeフェーズ終了後に一度だけ提出する。
///          フレームの開始・終了はRendererが管理する。
class ComputeCommandProcessor final {
public:
    /// @brief GameEngine から DirectXCommon を設定し、専用コマンドリストを確保する
    static void Initialize(Passkey<GameEngine>, DirectXCommon *dx);
    /// @brief 専用コマンドリストを解放する
    static void Finalize(Passkey<GameEngine>);

    /// @brief 1フレーム分のCompute記録期間を開始する
    static void BeginFrame(Passkey<Renderer>);
    /// @brief フレーム内で共有するコマンドリストを取得する
    /// @details 初回取得時にのみResetして記録を開始し、以降は同じコマンドリストを返す。
    /// @return コマンドリスト（未初期化または記録期間外の場合はnullptr）
    static ID3D12GraphicsCommandList *GetCommandList(Passkey<Renderer>);
    /// @brief 記録を終了し、実行待ちキューへ一度だけ登録する
    /// @details コマンドリストが一度も取得されていない場合は登録しない。
    static void EndFrame(Passkey<Renderer>);

private:
    ComputeCommandProcessor() = delete;

    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static inline DX12Commands *sCommands_ = nullptr;
    static inline int sCommandSlotIndex_ = -1;
    static inline bool sFrameActive_ = false;
};

} // namespace KashipanEngine
