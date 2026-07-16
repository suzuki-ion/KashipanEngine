#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>

#include "Scene/Editor/SceneEditorCommands.h"

namespace KashipanEngine {

class EmptyObject;
class SceneEditorContext;

/// @brief プレハブ（.prefabファイル）の作成・読み込みユーティリティ
/// @details .prefabの中身はただのJSONで、シーン保存時のオブジェクト保存と同じ形式の
///          オブジェクトJSONを部分木（pre-order、先頭が根）として並べたもの。
///          配置時はヒエラルキーのコピー/貼り付けと同じ機構（PasteObjectCommand）で
///          新しいobjectIDを割り当てて生成される。
namespace PrefabUtility {

/// @brief プレハブファイルの拡張子
inline constexpr const char *kPrefabExtension = ".prefab";

/// @brief 指定オブジェクトを根とする部分木のJSONスナップショットを収集する（pre-order）
void CollectSubtreeNodes(SceneEditorContext *context, EmptyObject *obj, int parentIndex,
    std::vector<PasteObjectCommand::Node> &out);

/// @brief 指定オブジェクト（とその子孫）からプレハブJSONを構築する
/// @details 根オブジェクトのTransformの親参照はシーン固有のIDのため取り除かれる
JSON BuildPrefabJson(SceneEditorContext *context, EmptyObject *rootObject);

/// @brief プレハブJSONからPasteObjectCommand用のノード列を復元する
/// @details プレハブ形式（"objects"配列）のほか、オブジェクト保存形式そのままの
///          単一オブジェクトJSONも受け付ける
std::vector<PasteObjectCommand::Node> LoadPrefabNodes(const JSON &prefabJson);

/// @brief ノード列を配置用に複製し、全ノードへ新しいobjectIDを割り当てて
///        部分木内部の親子参照（Transformの"parent"）を新IDへ張り替える。
///        同じノード列から複数回配置してもUUIDが衝突しないよう、配置の都度呼び出すこと。
/// @param preserveRootParent trueの場合、ルートノード（parentIndexInSubtree<0）の"parent"参照は
///        元のまま変更しない（複製時に、複製元と同じ親へ自動的に接続されるようにするため）。
///        falseの場合はルートノードの"parent"を消去する（PasteObjectCommand側でattachParentへ接続する）
std::vector<PasteObjectCommand::Node> PrepareNodesForInstantiation(
    const std::vector<PasteObjectCommand::Node> &source, bool preserveRootParent);

} // namespace PrefabUtility

} // namespace KashipanEngine

#endif // USE_IMGUI
