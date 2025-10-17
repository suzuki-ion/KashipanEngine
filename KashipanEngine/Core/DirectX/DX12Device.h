#pragma once
#include <d3d12.h>

namespace KashipanEngine {

class DX12Device {
public:
    DX12Device();
    ~DX12Device() = default;

    ID3D12Device* const GetDevice();
    ID3D12GraphicsCommandList *GetCommandList() const;
    ID3D12CommandQueue *GetCommandQueue() const;
    ID3D12CommandAllocator *GetCommandAllocator() const;
    void ExecuteCommandList();

};

} // namespace KashipanEngine