#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace KashipanEngine {

template<typename T>
// メッシュ
struct Mesh {
    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    // インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    // インデックスバッファビュー
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    // 頂点バッファマップ
    T *vertexBufferMap = nullptr;
    // インデックスバッファマップ
    uint32_t *indexBufferMap = nullptr;
};

} // namespace KashipanEngine