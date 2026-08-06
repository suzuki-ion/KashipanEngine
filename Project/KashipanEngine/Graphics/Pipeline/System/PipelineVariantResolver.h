#pragma once
#include <string>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief パイプラインバリアント名の解決結果
struct PipelineVariantResolution {
    bool matched = false;
    JSON synthesizedPipelineJson;
};

/// @brief "Object3D.Toon.Solid.BlendAdd" のようなドット区切り名から、既存の静的Pipelines/*.jsonと
///        同一スキーマのJSONを合成できるか判定する。
/// @details 対応するのは Object3D/Object2D の Blend×Culling×Toon(Object3Dのみ) の組み合わせのみ。
///          未対応の名前（先頭がObject3D/Object2D以外、未知トークン、重複トークン等）は
///          matched=false を返す。呼び出し側は今まで通り「パイプラインが見つからない」扱いにできる
PipelineVariantResolution TryResolvePipelineVariant(const std::string &pipelineName);

} // namespace KashipanEngine
