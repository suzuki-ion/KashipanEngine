#include "DX12DXGIs.h"
#include "Utilities/Conversion/ConvertString.h"

#include <stdexcept>

namespace KashipanEngine {

DX12DXGIs::DX12DXGIs(Passkey<DirectXCommon>) {
    LogScope scope;
    HRESULT hr = S_OK;
    Log(Translation("engine.directx.dxgi.initialize.start"), LogSeverity::Debug);

    //==================================================
    // DXGIファクトリー作成
    //==================================================

    Log(Translation("engine.directx.dxgi.factory.initialize.start"), LogSeverity::Debug);
    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    if (FAILED(hr)) {
        Log(Translation("engine.directx.dxgi.factory.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create DXGI Factory.");
    }
    Log(Translation("engine.directx.dxgi.factory.initialize.end"), LogSeverity::Debug);

    //==================================================
    // DXGIアダプター作成
    //==================================================

    Log(Translation("engine.directx.dxgi.adapter.initialize.start"), LogSeverity::Debug);
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 desc{};
        hr = dxgiAdapter_->GetDesc3(&desc);
        if (FAILED(hr)) {
            Log(Translation("engine.directx.dxgi.adapter.getdesc.failed"), LogSeverity::Warning);
            continue;
        }
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
            // ソフトウェアアダプターはスキップ
            continue;
        }
        Log(Translation("engine.directx.dxgi.adapter.selected") + ConvertString(desc.Description), LogSeverity::Debug);
        break;
    }
    if (!dxgiAdapter_) {
        Log(Translation("engine.directx.dxgi.adapter.initialize.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to find a suitable DXGI Adapter.");
    }
    Log(Translation("engine.directx.dxgi.adapter.initialize.end"), LogSeverity::Debug);

    Log(Translation("engine.directx.dxgi.initialize.end"), LogSeverity::Debug);
}

DX12DXGIs::~DX12DXGIs() {
    LogScope scope;
    Log(Translation("instance.destroying"), LogSeverity::Debug);
    dxgiAdapter_.Reset();
    dxgiFactory_.Reset();
    Log(Translation("instance.destroyed"), LogSeverity::Debug);
}

} // namespace KashipanEngine