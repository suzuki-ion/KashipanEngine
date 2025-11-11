#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace KashipanEngine {

// パイプラインセット
struct PipelineSet {
    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

} // namespace KashipanEngine