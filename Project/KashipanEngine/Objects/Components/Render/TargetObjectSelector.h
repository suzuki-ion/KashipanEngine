#pragma once
#if defined(USE_IMGUI)
#include <string>

#include "Utilities/UUID128.h"

namespace KashipanEngine {

class EmptyObject;
class SceneContext;

namespace TargetObjectSelector {

/// @brief 指定オブジェクトが描画先コンポーネントを持っているか
bool HasRenderTargetComponent(EmptyObject *object);

/// @brief 描画先オブジェクトの選択UI（シーン上のオブジェクトから選択 or ヒエラルキーからのD&D）
/// @param label ラベル
/// @param sceneContext 所属シーンのコンテキスト
/// @param targetObjectID 選択中の描画先オブジェクトID（変更時に上書きされる）
/// @param allowNone 未指定（None）を選択可能にする
/// @return 値が変更された場合は true
bool ShowSelector(const char *label, SceneContext *sceneContext, UUID128 &targetObjectID, bool allowNone = true);

} // namespace TargetObjectSelector
} // namespace KashipanEngine

#endif // USE_IMGUI
