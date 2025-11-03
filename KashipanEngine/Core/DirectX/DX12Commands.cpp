#include "DX12Commands.h"
#include <stdexcept>

namespace KashipanEngine {

DX12Commands::DX12Commands(Passkey<DirectXCommon>, ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type) {
    LogScope scope;
    Log(Translation("engine.directx.commands.initialize.start"), LogSeverity::Debug);

    //==================================================
    // コマンドキュー作成
    //==================================================

    Log(Translation("engine.directx.commands.commandqueue.initialize.start"), LogSeverity::Debug);
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.commands.commandqueue.initialize.failed"), LogSeverity::Error);
        throw std::runtime_error("Failed to create command queue.");
    }
    Log(Translation("engine.directx.commands.commandqueue.initialize.end"), LogSeverity::Debug);
    
    //==================================================
    // コマンドアロケーター作成
    //==================================================

    Log(Translation("engine.directx.commands.commandallocator.initialize.start"), LogSeverity::Debug);
    hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.commands.commandallocator.initialize.failed"), LogSeverity::Error);
        throw std::runtime_error("Failed to create command allocator.");
    }
    Log(Translation("engine.directx.commands.commandallocator.initialize.end"), LogSeverity::Debug);
    
    //==================================================
    // コマンドリスト作成
    //==================================================

    Log(Translation("engine.directx.commands.commandlist.initialize.start"), LogSeverity::Debug);
    hr = device->CreateCommandList(0, type, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.commands.commandlist.initialize.failed"), LogSeverity::Error);
        throw std::runtime_error("Failed to create command list.");
    }
    Log(Translation("engine.directx.commands.commandlist.initialize.end"), LogSeverity::Debug);

    // コマンドリストは初期状態でオープンされているため、クローズする
    commandList_->Close();

    Log(Translation("engine.directx.commands.initialize.end"), LogSeverity::Debug);
}

DX12Commands::~DX12Commands() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

void DX12Commands::ExecuteCommandList(Passkey<DirectXCommon>) {
}

} // namespace KashipanEngine
