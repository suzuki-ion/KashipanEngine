#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class EmptyObject;
class IObjectComponent;
class SceneEditorContext;
class SceneEditorCommands;

/// @brief Prefabインスタンス（ライブなシーン状態）とPrefabファイルの間の差分検出・同期実行
/// @details PrefabUtility（Prefabファイルの構築/読み込みという静的なJSON変換）とは役割分担し、
///          こちらは「今シーンに置かれているインスタンスが、元のPrefabからどう変化しているか」を
///          検出し、Apply/Revert・他インスタンスへの伝播を行う。
///          Override検出の粒度はコンポーネント単位（フィールド単位ではない）：あるコンポーネントの
///          データに1つでも差異があれば、そのコンポーネントごと「Override」として扱う。
namespace PrefabSyncUtility {

/// @brief 単一オブジェクトのコンポーネント単位のOverride情報
struct ComponentOverride {
    EmptyObject *object = nullptr;
    /// @brief インスタンス側コンポーネント（nullptr = Prefab側にのみ存在＝ローカルで削除された）
    IObjectComponent *component = nullptr;
    std::string componentType;
    /// @brief 同一オブジェクト内での同型コンポーネントの出現順（0始まり）
    size_t ordinal = 0;
    /// @brief false = インスタンス側でのみ追加されたコンポーネント（Prefab側に対応が無い）
    bool existsInPrefab = true;
    /// @brief false = インスタンス側で削除されたコンポーネント（componentはnullptrになる）
    bool existsInInstance = true;
};

/// @brief サブツリー全体の同期状態レポート
struct SyncReport {
    UUID128 prefabID;
    EmptyObject *instanceRoot = nullptr;
    /// @brief Overrideと判定されたコンポーネント一覧
    std::vector<ComponentOverride> overrides;
    /// @brief prefabNodeIDが無効、またはPrefab側に対応ノードが無い＝インスタンス側だけの新規オブジェクト
    std::vector<EmptyObject *> instanceOnlyObjects;
    /// @brief Prefab側にのみ存在する（インスタンス側で削除済みの）ノードのID
    std::vector<UUID128> prefabOnlyNodeIDs;

    bool IsEmpty() const {
        return overrides.empty() && instanceOnlyObjects.empty() && prefabOnlyNodeIDs.empty();
    }
};

/// @brief 指定オブジェクトを根とする部分木のEmptyObject*一覧をpre-orderで収集する
///        （PrefabUtility::CollectSubtreeNodesと同じ走査規則のEmptyObject*版）
std::vector<EmptyObject *> CollectSubtreeObjects(SceneEditorContext *context, EmptyObject *root);

/// @brief objの祖先チェーン（自身含む）を辿り、最も近い有効なPrefabInstanceComponent保持オブジェクトを返す
///        （EmptyObject::IsEditorOnlyInHierarchyと同じ祖先辿りパターン）。見つからなければnullptr
EmptyObject *FindEnclosingPrefabInstanceRoot(EmptyObject *obj);

/// @brief 単一オブジェクト単位でのOverride判定（ヒエラルキーの毎フレーム色分け用の軽量版。
///        O(そのオブジェクトのコンポーネント数)で完了する）
bool HasComponentOverride(SceneEditorContext *context, EmptyObject *obj);

/// @brief サブツリー全体でOverride・構造差分が1件でも存在するか
bool HasAnyOverride(SceneEditorContext *context, EmptyObject *instanceRoot);

/// @brief サブツリー全体の詳細差分を計算する（インスペクターのOverride一覧表示用）
SyncReport Diff(SceneEditorContext *context, EmptyObject *instanceRoot);

/// @brief Apply All: instanceRootのサブツリーの現在状態をPrefabファイルへ書き戻す
/// @details 書き戻し後、PrefabAssetManagerの変更通知を経由して他の同一Prefabインスタンスへも
///          非破壊マージ（ローカルでOverrideされていない値のみの更新）が伝播する。
///          instanceRoot自身のPrefabInstanceComponentはPrefab側からは取り除かれる。
///          ローカルにだけ追加されたオブジェクトは新規ノードとしてPrefabへ追加され、
///          ローカルで削除されたオブジェクトはPrefab側からも取り除かれる（構造も含めて反映される）
bool ApplyAll(SceneEditorContext *context, EmptyObject *instanceRoot);

/// @brief Revert All: instanceRootのサブツリーをPrefabの現在の内容で完全に置き換える（破壊的操作）
/// @details ローカルの上書き・追加オブジェクトはすべて破棄され、Prefabの構造も含めて完全に一致する
///          状態へ戻る。既存のDeleteObjectCommand+PasteObjectCommandの合成で実現するため、
///          新規コマンドクラスの追加は不要
bool RevertAll(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *instanceRoot);

/// @brief 個別コンポーネントのApply（対象コンポーネントのみをPrefabファイルへ書き戻す）
bool ApplyComponentOverride(SceneEditorContext *context, const ComponentOverride &target);
/// @brief 個別コンポーネントのRevert（Prefab側の値をこのインスタンスへ反映するのみ。ファイル書き込みなし）
bool RevertComponentOverride(SceneEditorContext *context, SceneEditorCommands *commands, const ComponentOverride &target);

/// @brief PrefabAssetManagerの変更リスナーとして登録される、他インスタンスへの伝播本体
/// @details oldPrefabJson/newPrefabJsonの対応ノード同士でコンポーネントを型+出現順（ordinal）で
///          突き合わせ、「変更前のPrefab値と一致する（＝ローカル未編集）」コンポーネントの値・
///          追加・削除を反映する。ローカルで既に上書き・追加・削除されているコンポーネントは
///          尊重してスキップする。ノード（オブジェクト）自体の追加・削除という構造変化は
///          伝播しない（各インスタンス側のApply All/Revert Allで個別に対応する）
void SyncOtherInstances(SceneEditorContext *context, SceneEditorCommands *commands,
    const UUID128 &prefabID, const JSON &oldPrefabJson, const JSON &newPrefabJson);

} // namespace PrefabSyncUtility

} // namespace KashipanEngine
#endif // USE_IMGUI
