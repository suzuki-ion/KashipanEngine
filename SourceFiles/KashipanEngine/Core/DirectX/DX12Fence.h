#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12フェンスクラス
class DX12Fence final {
public:
    DX12Fence(Passkey<DirectXCommon>, ID3D12Device *device);
    ~DX12Fence();

    /// @brief フェンスの値を増加してシグナルを送る
    /// @param commandQueue コマンドキュー
    void Signal(Passkey<DirectXCommon>, ID3D12CommandQueue *commandQueue);
    /// @brief フェンスが指定の値に到達するまで待機する
    /// @param value 待機するフェンスの値
    /// @return 待機に成功したらtrue、タイムアウトやエラーの場合はfalseを返す
    bool Wait(Passkey<DirectXCommon>);

    /// @brief 直近の Signal で設定した目標値を取得する（ノンブロッキング待機の完了判定用）
    uint64_t GetCurrentValue(Passkey<DirectXCommon>) const noexcept { return currentValue_; }
    /// @brief 指定の値までGPU側の処理が完了しているかをブロックせずに確認する
    /// @param value 完了を確認したいフェンスの値
    bool IsComplete(Passkey<DirectXCommon>, uint64_t value) const noexcept { return fence_->GetCompletedValue() >= value; }

private:
    DX12Fence(const DX12Fence &) = delete;
    DX12Fence &operator=(const DX12Fence &) = delete;
    DX12Fence(DX12Fence &&) = delete;
    DX12Fence &operator=(DX12Fence &&) = delete;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_;
    uint64_t currentValue_;
};

} // namespace KashipanEngine