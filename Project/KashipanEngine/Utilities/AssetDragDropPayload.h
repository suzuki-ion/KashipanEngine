#pragma once
#ifdef USE_IMGUI
#include <cstdio>
#include <imgui.h>
#include <string>

namespace KashipanEngine {

/// @brief Assetsウィンドウからのテクスチャファイルドラッグのペイロード型名
inline constexpr const char *kTextureAssetDragDropType = "DND_ASSET_TEXTURE";
/// @brief Assetsウィンドウからのマテリアルファイルドラッグのペイロード型名
inline constexpr const char *kMaterialAssetDragDropType = "DND_ASSET_MATERIAL";

/// @brief Assetsウィンドウからのアセットファイルドラッグ用ペイロード
/// @details assetPath はAssetsルートからの相対パス（TextureManager/MaterialManager等の
///          アセットパスと同じ形式）。ImGuiのドラッグペイロードは値コピーされるため、
///          std::string ではなく固定長バッファで持つ。
struct AssetDragDropPayload {
    char assetPath[256] = {};
};

/// @brief アセットファイルのドラッグソースへペイロードを設定する
inline void SetAssetDragDropPayload(const char *dragDropType, const std::string &assetPath) {
    AssetDragDropPayload payload{};
    std::snprintf(payload.assetPath, sizeof(payload.assetPath), "%s", assetPath.c_str());
    ImGui::SetDragDropPayload(dragDropType, &payload, sizeof(payload));
}

/// @brief 直前のImGuiアイテムをアセットD&Dのドロップターゲットにする
/// @return ドロップされた場合は true（outAssetPath にドロップされたアセットパスが入る）
inline bool AcceptAssetDragDropTarget(const char *dragDropType, std::string &outAssetPath) {
    bool dropped = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(dragDropType)) {
            IM_ASSERT(payload->DataSize == sizeof(AssetDragDropPayload));
            const auto *dndPayload = static_cast<const AssetDragDropPayload *>(payload->Data);
            outAssetPath = dndPayload->assetPath;
            dropped = true;
        }
        ImGui::EndDragDropTarget();
    }
    return dropped;
}

} // namespace KashipanEngine

#endif // USE_IMGUI
