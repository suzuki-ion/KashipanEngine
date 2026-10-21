#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;

class DX12Commands final {
public:
    DX12Commands(Passkey<DirectXCommon>, ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
    ~DX12Commands() = default;

    DX12Commands(const DX12Commands&) = delete;
    DX12Commands& operator=(const DX12Commands&) = delete;
    DX12Commands(DX12Commands&&) = delete;
    DX12Commands& operator=(DX12Commands&&) = delete;

    ID3D12GraphicsCommandList* GetCommandList() const noexcept { return commandList_.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return commandAllocator_.Get(); }

    bool IsRecording() const noexcept { return isRecording_; }
    bool IsRecorded() const noexcept { return isRecorded_; }

    ID3D12GraphicsCommandList* BeginRecord();
    bool EndRecord();

    void ResetFlags(Passkey<DirectXCommon>) noexcept {
        isRecording_ = false;
        isRecorded_ = false;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

    bool isRecording_ = false;
    bool isRecorded_ = false;
};

} // namespace KashipanEngine
