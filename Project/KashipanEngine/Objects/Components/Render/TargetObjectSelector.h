#pragma once
#if defined(USE_IMGUI)
#include <string>
#include <unordered_set>

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

/// @brief 対象オブジェクトが持つ描画先ごとに、描画する/しないをチェックボックスで選択するUI
/// @details 対象オブジェクトが未指定、または描画先コンポーネントを1つも持たない場合は何も表示しない
/// @param sceneContext 所属シーンのコンテキスト
/// @param targetObjectID 対象オブジェクトのUUID
/// @param excludedRenderTargetNames 除外する描画先名の集合（チェックを外すとここに名前が追加される）
void ShowRenderTargetFilters(SceneContext *sceneContext, const UUID128 &targetObjectID, std::unordered_set<std::string> &excludedRenderTargetNames);

} // namespace TargetObjectSelector
} // namespace KashipanEngine

#endif // USE_IMGUI
