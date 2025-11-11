#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <string>

namespace KashipanEngine {

class ComputePipelineState {
public:
    [[nodiscard]] const D3D12_COMPUTE_PIPELINE_STATE_DESC &operator[](const std::string &pipelineStateName) const {
        return pipelineStateDescs_.at(pipelineStateName);
    }

    void AddPipelineState(const std::string &pipelineStateName, D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc) {
        pipelineStateDescs_[pipelineStateName] = pipelineStateDesc;
    }

    [[nodiscard]] const D3D12_COMPUTE_PIPELINE_STATE_DESC &GetPipelineState(const std::string &pipelineStateName) const {
        return pipelineStateDescs_.at(pipelineStateName);
    }

    void Clear() {
        pipelineStateDescs_.clear();
    }

private:
    std::unordered_map<std::string, D3D12_COMPUTE_PIPELINE_STATE_DESC> pipelineStateDescs_;
};

} // namespace KashipanEngine