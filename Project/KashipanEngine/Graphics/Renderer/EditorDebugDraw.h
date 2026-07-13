#pragma once
#include <vector>

#include "Assets/TextureManager.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

/// @brief デバッグ線1頂点（ワールド座標系）
struct DebugLineVertex {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

/// @brief エディターのシーンビューに表示するデバッグ描画設定
/// @details SceneEditorView が毎フレーム内容を構築し、SceneRenderer へ登録する。
///          実際の描画は Renderer が対象がエディター用描画先の場合にのみ行う。
struct EditorDebugDrawSettings {
    bool showGrid = true;
    bool showColliderGizmos = true;
    /// @brief グリッドのフェード開始距離（カメラからのワールド距離）
    float gridFadeDistance = 100.0f;
    /// @brief 当たり判定等のデバッグ線（2頂点で1本の線分を表す。LineListとして描画される）
    std::vector<DebugLineVertex> lines;

    /// @brief シーンビューの背景色（backgroundTextureHandleが無効な場合はこの単色で塗りつぶす）
    Vector4 backgroundColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    /// @brief シーンビューの背景に使うテクスチャ（有効な場合はbackgroundColorの代わりにこちらを表示する）
    TextureManager::TextureHandle backgroundTextureHandle = TextureManager::kInvalidHandle;
};

} // namespace KashipanEngine
