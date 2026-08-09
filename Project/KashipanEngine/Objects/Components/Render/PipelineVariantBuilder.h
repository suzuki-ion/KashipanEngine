#pragma once
#if defined(USE_IMGUI)
#include <string>

namespace KashipanEngine {
namespace PipelineVariantBuilder {

/// @brief パイプラインのバリアントビルダーUI（Base/Toon/Raster/Blend＋シェーダーモジュール選択＋適用ボタン）
/// @details MeshRenderer/SkinnedMeshRenderer等、パイプライン名を1つ持つコンポーネントから共通で使う。
///          事前列挙されたBlend×Culling×Toon(Object3Dのみ)×シェーダーモジュールの組み合わせに無い
///          パイプライン名が欲しい場合、ここでトークンを組み立てて
///          PipelineManager::TryGetOrCreatePipelineに動的生成させる。
///          「適用」ボタン押下時、生成/取得に成功した場合のみpipelineNameを書き換える
///          （失敗時は何もしない＝呼び出し元は今まで通りの値のまま）
/// @param pipelineName 適用先のパイプライン名（適用成功時に上書きされる）
/// @return 適用成功しpipelineNameが変更された場合はtrue（呼び出し元は描画リストの再構築等が必要なら行うこと）
bool Show(std::string &pipelineName);

} // namespace PipelineVariantBuilder
} // namespace KashipanEngine
#endif // USE_IMGUI
