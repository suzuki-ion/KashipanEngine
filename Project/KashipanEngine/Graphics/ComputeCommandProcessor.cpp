#include "ComputeCommandProcessor.h"
#include "Core/DirectXCommon.h"
#include "Core/DirectX/DX12Commands.h"

namespace KashipanEngine {

void ComputeCommandProcessor::Initialize(Passkey<GameEngine>, DirectXCommon *dx) {
    sDirectXCommon_ = dx;
    if (!sDirectXCommon_ || sCommandSlotIndex_ >= 0) return;
    sCommandSlotIndex_ = sDirectXCommon_->AcquireCommandObjects(Passkey<ComputeCommandProcessor>{});
    sCommands_ = sDirectXCommon_->GetCommandObjects(Passkey<ComputeCommandProcessor>{}, sCommandSlotIndex_);
}

void ComputeCommandProcessor::Finalize(Passkey<GameEngine>) {
    if (sDirectXCommon_ && sCommandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseCommandObjects(Passkey<ComputeCommandProcessor>{}, sCommandSlotIndex_);
    }
    sCommands_ = nullptr;
    sCommandSlotIndex_ = -1;
    sDirectXCommon_ = nullptr;
}

ID3D12GraphicsCommandList *ComputeCommandProcessor::BeginRecord(Passkey<Renderer>) {
    if (!sCommands_) return nullptr;
    return sCommands_->BeginRecord();
}

void ComputeCommandProcessor::EndRecord(Passkey<Renderer>) {
    if (!sCommands_ || !sCommands_->IsRecording()) return;
    if (!sCommands_->EndRecord()) return;
    if (sDirectXCommon_) {
        sDirectXCommon_->AddRecordCommandList(Passkey<ComputeCommandProcessor>{}, sCommands_->GetCommandList());
    }
}

} // namespace KashipanEngine
