#include "DX12CommandQueue.h"
#include <stdexcept>
#include <cassert>

namespace KashipanEngine {

DX12CommandQueue::DX12CommandQueue(Passkey<DirectXCommon>, ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    LogScope scope;
    Log(Translation("engine.directx.commands.initialize.start"), LogSeverity::Debug);

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.commands.commandqueue.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create command queue.");
    }

    Log(Translation("engine.directx.commands.initialize.end"), LogSeverity::Debug);
}

DX12CommandQueue::~DX12CommandQueue() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    commandQueue_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

void DX12CommandQueue::ExecuteCommandLists(Passkey<DirectXCommon>, const std::vector<ID3D12CommandList*>& lists) {
    LogScope scope;
    if (lists.empty()) return;

    // 送信前バリデーション（デバッグビルドのみ）
#ifdef _DEBUG
    D3D12_COMMAND_QUEUE_DESC qdesc = commandQueue_->GetDesc();
    for (size_t idx = 0; idx < lists.size(); ++idx) {
        auto *cl = lists[idx];
        assert(cl && "Command list is null");

        // 型一致チェック
        auto type = cl->GetType();
        assert(type == qdesc.Type && "Queue/List type mismatch");
    }
#endif

    size_t i = 0;
    while (i < lists.size()) {
        const UINT batchCount = static_cast<UINT>(std::min<size_t>(8, lists.size() - i));
        commandQueue_->ExecuteCommandLists(batchCount, lists.data() + i);
        i += batchCount;
    }
}

} // namespace KashipanEngine
