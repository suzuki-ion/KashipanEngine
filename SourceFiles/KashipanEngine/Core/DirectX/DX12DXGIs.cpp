#include "DX12DXGIs.h"
#include "Utilities/Conversion/ConvertString.h"

#include <d3d12.h>
#include <stdexcept>
#include "Utilities/Translation.h"

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
    for (UINT i = 0;; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter4> candidate;
        if (dxgiFactory_->EnumAdapterByGpuPreference(i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC3 desc{};
        hr = candidate->GetDesc3(&desc);
        if (FAILED(hr)) {
            Log(Translation("engine.directx.dxgi.adapter.getdesc.failed"), LogSeverity::Warning);
            continue;
        }
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
            // ソフトウェアアダプターはスキップ
            continue;
        }
        // 列挙順の先頭を無条件に選ばず、実際に必要なFeature Levelでデバイスを
        // 作成できるアダプターだけを採用する。マルチGPU環境では次候補も試す。
        Microsoft::WRL::ComPtr<ID3D12Device> testDevice;
        if (FAILED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&testDevice)))) {
            continue;
        }
        dxgiAdapter_ = std::move(candidate);
        Log(Translation("engine.directx.dxgi.adapter.selected") + ConvertString(desc.Description), LogSeverity::Debug);
        break;
    }

    // 対応ハードウェアがない環境でも診断・軽量動作ができるようWARPを最後に試す。
    if (!dxgiAdapter_) {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)))) {
            warpAdapter.As(&dxgiAdapter_);
            if (dxgiAdapter_) {
                Log("No compatible hardware D3D12 adapter was found; using WARP.", LogSeverity::Warning);
            }
        }
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
