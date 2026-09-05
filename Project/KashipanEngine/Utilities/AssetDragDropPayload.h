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
/// @brief Assetsウィンドウからのスクリプトファイル（.as）ドラッグのペイロード型名
/// @details スクリプトのパスはテクスチャ等と異なり、実行ディレクトリからの相対パス
///          （"Assets/" プレフィックス付き。ScriptComponentのScript Pathと同じ形式）で渡す
inline constexpr const char *kScriptAssetDragDropType = "DND_ASSET_SCRIPT";
/// @brief Assetsウィンドウからのプレハブファイル（.prefab）ドラッグのペイロード型名
/// @details パスは実行ディレクトリからの相対パス（"Assets/" プレフィックス付き）で渡す
inline constexpr const char *kPrefabAssetDragDropType = "DND_ASSET_PREFAB";
/// @brief Assetsウィンドウからの動画ファイルドラッグのペイロード型名
inline constexpr const char *kVideoAssetDragDropType = "DND_ASSET_VIDEO";
/// @brief Assetsウィンドウからの音声ファイルドラッグのペイロード型名
inline constexpr const char *kAudioAssetDragDropType = "DND_ASSET_AUDIO";

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
