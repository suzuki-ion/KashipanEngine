#pragma once
#ifdef USE_IMGUI
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/ModelManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Utilities/UUID128.h"

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
/// @details 根オブジェクトのTransformの親参照はシーン固有のIDのため取り除かれる。
///          サブツリー内の各ライブオブジェクトについて、prefabNodeIDが未採番なら新規発行して
///          ライブオブジェクト側にも書き戻す（Prefabとの対応付けに使う安定ID。objectIDとは別）。
///          rootObjectが既にPrefabInstanceComponentを持っていても、そのコンポーネントは
///          出力JSONから取り除かれる（新しく作るPrefabは元のリンクと無関係な独立した資産にするため）
/// @param prefabID このPrefab資産の固有ID。省略時は新規に発行される
JSON BuildPrefabJson(SceneEditorContext *context, EmptyObject *rootObject, const UUID128 &prefabID = UUID128(true));

/// @brief プレハブJSONからPasteObjectCommand用のノード列を復元する
/// @details プレハブ形式（"objects"配列）のほか、オブジェクト保存形式そのままの
///          単一オブジェクトJSONも受け付ける
std::vector<PasteObjectCommand::Node> LoadPrefabNodes(const JSON &prefabJson);

/// @brief JSON内の文字列値のうち、remapテーブル（旧文字列→新文字列）に一致するものを再帰的に
///        置き換える。UUID文字列は衝突がほぼありえない一意な形式のため、値の完全一致だけで
///        安全に判定できる。オブジェクトID参照（Animator.rootBoneObjectID等のUUID128フィールド）を
///        まとめて新IDへ張り替える用途に使う
void RemapObjectIDReferences(JSON &value, const std::unordered_map<std::string, std::string> &remap);

/// @brief ノード列を配置用に複製し、全ノードへ新しいobjectIDを割り当てて
///        部分木内部の相互参照（Transformの"parent"、Animator/TargetLookAt/ParticleSystem等が
///        持つUUID128参照フィールド全般）を新IDへ張り替える。部分木の外側（シーン内の別オブジェクト等）
///        への参照はremapテーブルに存在しないため変更されず、意図通り維持される。
///        同じノード列から複数回配置してもUUIDが衝突しないよう、配置の都度呼び出すこと。
/// @param preserveRootParent trueの場合、ルートノード（parentIndexInSubtree<0）の"parent"参照は
///        元のまま変更しない（複製時に、複製元と同じ親へ自動的に接続されるようにするため）。
///        falseの場合はルートノードの"parent"を消去する（PasteObjectCommand側でattachParentへ接続する）
std::vector<PasteObjectCommand::Node> PrepareNodesForInstantiation(
    const std::vector<PasteObjectCommand::Node> &source, bool preserveRootParent);

/// @brief ルートノード群（parentIndexInSubtree<0）を指定したワールド座標へ移動する
/// @details 複数のルートノードがある場合は、先頭のルートの元の位置を基準にした差分を全ルートへ
///          等しく適用し、ルート間の相対配置を保ったまま全体を移動する。ルートは親を持たない前提
///          （PrepareNodesForInstantiationで親参照が消去された後の呼び出しを想定）のため、
///          customData["translate"]をそのままワールド座標として扱う。
///          Prefabのシーンビューへのドラッグ&ドロップ配置（カーソル位置への配置）に使う
void OffsetRootsToWorldPosition(std::vector<PasteObjectCommand::Node> &nodes, const Vector3 &worldPosition);

/// @brief シーンビューへのPrefabドラッグ中プレビュー用に、メッシュハンドルとワールド行列だけを取り出す
/// @details EmptyObject/コンポーネントは一切生成しない（ドラッグ中は未確定のため、実際に配置される
///          Prefabインスタンスとは無関係に、SceneRendererへ直接渡すためだけの一時データを作る）。
///          MeshFilter+MeshRenderer（いずれも非アクティブでない）を持つノードのみを対象にする
struct GhostPreviewMesh {
    ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
    Matrix4x4 worldMatrix = Matrix4x4::Identity();
};

/// @param nodes プレビュー対象のノード列（PrefabUtility::LoadPrefabNodes等で構築したもの。
///        このノード列自体は書き換えない）
/// @param worldPosition ルートを配置するワールド座標（OffsetRootsToWorldPositionと同じ規約）
std::vector<GhostPreviewMesh> BuildGhostPreviewMeshes(
    const std::vector<PasteObjectCommand::Node> &nodes, const Vector3 &worldPosition);

} // namespace PrefabUtility

} // namespace KashipanEngine

#endif // USE_IMGUI
