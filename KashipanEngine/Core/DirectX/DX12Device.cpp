#include "DX12Device.h"

#include <stdexcept>

namespace KashipanEngine {

DX12Device::DX12Device(Passkey<DirectXCommon>, IDXGIAdapter4 *adapter) {
    LogScope scope;
    Log(Translation("engine.directx.device.initialize.start"), LogSeverity::Debug);

    // デバイスの作成
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };
    const char *featureLevelStr[] = {
        "12_2",
        "12_1",
        "12_0",
    };
    HRESULT hr = E_FAIL;
    for (size_t i = 0; i < sizeof(featureLevels); ++i) {
        hr = D3D12CreateDevice(
            adapter,
            featureLevels[i],
            IID_PPV_ARGS(&device_)
        );
        if (SUCCEEDED(hr)) {
            Log(Translation("engine.directx.device.created.featurelevel") + " D3D_FEATURE_LEVEL_" + std::string(featureLevelStr[i]), LogSeverity::Debug);
            break;
        }
    }
    if (FAILED(hr)) {
        Log(Translation("engine.directx.device.initialize.failed"), LogSeverity::Error);
        throw std::runtime_error("Failed to create D3D12 Device.");
    }
    Log(Translation("engine.directx.device.initialize.end"), LogSeverity::Debug);
}

DX12Device::~DX12Device() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    device_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

} // namespace KashipanEngine