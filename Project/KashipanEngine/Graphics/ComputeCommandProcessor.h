#pragma once
#include <d3d12.h>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class DX12Commands;
class Renderer;

/// @brief Computeシェーダー処理専用のコマンドリストを管理するクラス
/// @details エンジンに1つだけ存在し、ComputeShaderProcessingコンポーネントの
///          Dispatch呼び出しは全てこのコマンドリストへまとめて記録される。
///          記録・提出のタイミングはRenderer（毎フレーム1回）が管理する。
class ComputeCommandProcessor final {
public:
    /// @brief GameEngine から DirectXCommon を設定し、専用コマンドリストを確保する
    static void Initialize(Passkey<GameEngine>, DirectXCommon *dx);
    /// @brief 専用コマンドリストを解放する
    static void Finalize(Passkey<GameEngine>);

    /// @brief 記録を開始する（既に開始済みの場合はそのままコマンドリストを返す）
    /// @return コマンドリスト（未初期化の場合は nullptr）
    static ID3D12GraphicsCommandList *BeginRecord(Passkey<Renderer>);
    /// @brief 記録を終了し、フレーム終端の実行キューへ登録する（記録していなければ何もしない）
    static void EndRecord(Passkey<Renderer>);

private:
    ComputeCommandProcessor() = delete;

    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static inline DX12Commands *sCommands_ = nullptr;
    static inline int sCommandSlotIndex_ = -1;
};

} // namespace KashipanEngine
