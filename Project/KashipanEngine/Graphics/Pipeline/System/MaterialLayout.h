#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "Utilities/ValueType.h"

namespace KashipanEngine {

/// @brief StructuredBuffer<Material> の1フィールド分のバイトレイアウト
/// @details hasRange/rangeMin/rangeMax/rangeStepは、HLSL側のフィールド宣言末尾の行コメント注釈
///          `// @Range(min, max[, step])` から読み取ったUI用のヒント（GPUバイトレイアウトには無関係）。
///          `@Color`注釈が付いたfloat4フィールドはvalueTypeがVector4ではなくColorになる
struct MaterialFieldLayout {
    std::string name;
    std::uint32_t byteOffset = 0;
    std::uint32_t byteSize = 0;
    ValueType valueType = ValueType::None;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 0.0f;
    /// @brief 注釈でstepが省略された場合は (rangeMax - rangeMin) / 100 を使う
    float rangeStep = 0.0f;
};

/// @brief シェーダー側 struct Material { ... }; から求めた StructuredBuffer<Material> 1要素分のバイトレイアウト
/// @details ID3D12ShaderReflection::GetVariableByName は StructuredBuffer の要素型（cbuffer外の
///          リソース）に対してメンバー情報を返さないため、リフレクションではなくHLSLソースの
///          テキスト走査でレイアウトを求める。
struct MaterialLayout {
    std::uint32_t totalByteSize = 0;
    std::vector<MaterialFieldLayout> fields;
    /// @brief シェーダー内で宣言されている任意の `Texture2D <name> : register(tN);` の変数名一覧。
    ///        struct Materialのメンバーにはなり得ない（StructuredBufferの要素型のため）ので別枠で持つ。
    ///        固定スロット（gTexture等）も区別せず全て含む。バインドレス化済みのモジュール
    ///        （NormalMap等）はTexture2D宣言自体を持たなくなるため、ここには含まれない
    ///        （代わりにfieldsに"<名前>TextureIndex"というuintフィールドとして現れる）
    std::vector<std::string> textureFields;
    /// @brief シェーダー内で宣言されている任意の `TextureCube <name> : register(tN);` の変数名一覧。
    ///        textureFieldsと同じ理由で別枠。マテリアルエディタのTextureCubeRef型パラメータの
    ///        「一括追加」ボタンで参照する
    std::vector<std::string> textureCubeFields;

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
