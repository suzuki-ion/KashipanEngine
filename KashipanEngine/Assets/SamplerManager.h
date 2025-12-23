#pragma once

#include <cstdint>
#include <string>
#include <d3d12.h>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class ShaderVariableBinder;

/// @brief サンプラー管理クラス（SamplerHeap にサンプラーを確保し、外部へ D3D12 ハンドルを出さない）
class SamplerManager final {
public:
    using SamplerHandle = uint32_t;
    static constexpr SamplerHandle kInvalidHandle = 0;

    SamplerManager(Passkey<GameEngine>, DirectXCommon* directXCommon);
    ~SamplerManager();

    SamplerManager(const SamplerManager&) = delete;
    SamplerManager& operator=(const SamplerManager&) = delete;
    SamplerManager(SamplerManager&&) = delete;
    SamplerManager& operator=(SamplerManager&&) = delete;

    /// @brief サンプラーを作成してハンドルを返す（同一設定の dedup は行わない）
    static SamplerHandle CreateSampler(const D3D12_SAMPLER_DESC& desc);

    /// @brief ハンドルのサンプラーをシェーダーへバインドする
    static bool BindSampler(ShaderVariableBinder* shaderBinder, const std::string& nameKey, SamplerHandle handle);

private:
    DirectXCommon* directXCommon_ = nullptr;
};

} // namespace KashipanEngine
