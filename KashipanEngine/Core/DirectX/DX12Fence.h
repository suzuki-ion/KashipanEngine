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