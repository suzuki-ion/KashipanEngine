#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "Utilities/ValueType.h"

namespace KashipanEngine {

/// @brief StructuredBuffer<Material> の1フィールド分のバイトレイアウト
struct MaterialFieldLayout {
    std::string name;
    std::uint32_t byteOffset = 0;
    std::uint32_t byteSize = 0;
    ValueType valueType = ValueType::None;
};

/// @brief シェーダー側 struct Material { ... }; から求めた StructuredBuffer<Material> 1要素分のバイトレイアウト
/// @details ID3D12ShaderReflection::GetVariableByName は StructuredBuffer の要素型（cbuffer外の
///          リソース）に対してメンバー情報を返さないため、リフレクションではなくHLSLソースの
///          テキスト走査でレイアウトを求める。
struct MaterialLayout {
    std::uint32_t totalByteSize = 0;
    std::vector<MaterialFieldLayout> fields;

    const MaterialFieldLayout *Find(const std::string &name) const {
        for (const auto &field : fields) {
            if (field.name == name) return &field;
        }
        return nullptr;
    }

    /// @brief ピクセルシェーダーのソースから struct Material { ... }; を抽出しレイアウトを構築する
    /// @param shaderFilePath シェーダーの.hlslファイルパス（物理パス）
    /// @param definedMacros コンパイル時に定義されたマクロ名の集合（#ifdef/#ifndefの評価に使う）
    /// @return struct Material が見つからない場合は空（totalByteSize=0, fields空）のレイアウトを返す
    static MaterialLayout BuildFromHlslSource(const std::string &shaderFilePath, const std::unordered_set<std::string> &definedMacros);
};

} // namespace KashipanEngine
