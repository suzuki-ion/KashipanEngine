#include "DCompHost.h"

namespace KashipanEngine {

bool DCompHost::InitializeForHwnd(HWND hwnd, BOOL topmost) {
    hwnd_ = hwnd;
    // デバイス作成
    Microsoft::WRL::ComPtr<IDCompositionDevice> device;
    HRESULT hr = DCompositionCreateDevice(
        nullptr, __uuidof(IDCompositionDevice),
        reinterpret_cast<void **>(device.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }

    device_ = device;

    // ターゲット作成
    hr = device_->CreateTargetForHwnd(hwnd_, topmost, target_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        device_.Reset();
        return false;
    }

    // ルートビジュアル作成
    hr = device_->CreateVisual(root_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        target_.Reset();
        device_.Reset();
        return false;
    }

    // ターゲットにルートを設定
    hr = target_->SetRoot(root_.Get());
    if (FAILED(hr)) {
        root_.Reset();
        target_.Reset();
        device_.Reset();
        return false;
    }

    return true;
}

bool DCompHost::SetContentSwapChain(IUnknown *swapChainContent) {
    if (!root_) return false;
    HRESULT hr = root_->SetContent(swapChainContent);
    return SUCCEEDED(hr);
}

bool DCompHost::Commit() {
    if (!device_) return false;
    return SUCCEEDED(device_->Commit());
}

}