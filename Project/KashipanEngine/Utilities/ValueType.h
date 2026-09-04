#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <type_traits>
#include <cstdint>
#include <memory>
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Color.h"
#include "Assets/TextureRef.h"
#include "Assets/TextureCubeRef.h"
#include "Utilities/FileIO/JSON.h"
#include "Debug/Logger.h"

/// @brief 値の型を表す列挙型
enum class ValueType {
    None,       // 未定義
    Bool,       // 真偽値
    Int8,       // 8ビット符号付き整数（char）
    UInt8,      // 8ビット符号なし整数（unsigned char）
    Int16,      // 16ビット符号付き整数（short）
    UInt16,     // 16ビット符号なし整数（unsigned short）
    Int32,      // 32ビット符号付き整数（int）
    UInt32,     // 32ビット符号なし整数（unsigned int）
    Int64,      // 64ビット符号付き整数（long long）
    UInt64,     // 64ビット符号なし整数（unsigned long long）
    Float,      // 単精度浮動小数点数（float）
    Double,     // 倍精度浮動小数点数（double）
    String,
    Pointer,
    Class,
    Any,
    Vector2,
    Vector3,
    Vector4,
    Matrix3x3,
    Matrix4x4,
    Quaternion,
    Color,      // 色（内部データはVector4と同じr,g,b,a4 float。編集UIでColorEdit4を使う点のみVector4と異なる）
    TextureRef, // テクスチャ参照（内部データはAssetsルートからの相対パス文字列）
    TextureCubeRef, // キューブマップ参照（環境マップ等。TextureRefと同じ内部データだが、編集UIのピッカーが
                    // isCubemapなテクスチャのみに絞り込む点が異なる）
    Vector,         // 可変長の配列（std::vector）
    UnorderedMap,   // ハッシュマップ（std::unordered_map）
    Pair,           // ペア（std::pair）
};

/// @brief ValueType 列挙型を文字列に変換する
/// @param type 変換したい ValueType
/// @return 対応する文字列
std::string ValueTypeToString(ValueType type);
/// @brief 文字列を ValueType 列挙型に変換する（テンプレート引数は無視。先頭の文字列を基に判定）
/// @param str 変換したい文字列
/// @return 対応する ValueType
ValueType StringToValueType(const std::string &str);

/// @brief 型情報を表すクラス
class TypeInfo {
public:
    TypeInfo(ValueType baseType) : baseType_(baseType) {}
    TypeInfo(ValueType baseType, std::vector<TypeInfo> templateArguments)
        : baseType_(baseType), templateArguments_(templateArguments) {}

    /// @brief 型情報を文字列に変換する
    std::string ToString() const;
    /// @brief ベース型を取得する
    ValueType GetBaseType() const;
    /// @brief テンプレート引数（内部型）を取得する
    const std::vector<TypeInfo> &GetTemplateArguments() const;

    bool operator==(const TypeInfo &other) const {
        return baseType_ == other.baseType_ && templateArguments_ == other.templateArguments_;
    }
    bool operator!=(const TypeInfo &other) const {
        return !(*this == other);
    }

private:
    ValueType baseType_;
    std::vector<TypeInfo> templateArguments_;
};

/// @brief 型 T から TypeInfo を取得するためのテンプレート構造体
template <typename T>
struct TypeExtractor {
    static TypeInfo Get() {
        KashipanEngine::LogScope scope;
        if constexpr (std::is_pointer_v<T>) {
            return TypeInfo(ValueType::Pointer);
        } else if constexpr (std::is_class_v<T>) {
            // C++では class と struct は区別できないため、Class に倒す
            return TypeInfo(ValueType::Class);
        } else {
            return TypeInfo(ValueType::None);
        }
    }
};

// プリミティブ型の特殊化
template <> struct TypeExtractor<bool> { static TypeInfo Get() { return TypeInfo(ValueType::Bool); } };
template <> struct TypeExtractor<int8_t> { static TypeInfo Get() { return TypeInfo(ValueType::Int8); } };
template <> struct TypeExtractor<uint8_t> { static TypeInfo Get() { return TypeInfo(ValueType::UInt8); } };
template <> struct TypeExtractor<int16_t> { static TypeInfo Get() { return TypeInfo(ValueType::Int16); } };
template <> struct TypeExtractor<uint16_t> { static TypeInfo Get() { return TypeInfo(ValueType::UInt16); } };
template <> struct TypeExtractor<int32_t> { static TypeInfo Get() { return TypeInfo(ValueType::Int32); } };
template <> struct TypeExtractor<uint32_t> { static TypeInfo Get() { return TypeInfo(ValueType::UInt32); } };
template <> struct TypeExtractor<int64_t> { static TypeInfo Get() { return TypeInfo(ValueType::Int64); } };
template <> struct TypeExtractor<uint64_t> { static TypeInfo Get() { return TypeInfo(ValueType::UInt64); } };
template <> struct TypeExtractor<float> { static TypeInfo Get() { return TypeInfo(ValueType::Float); } };
template <> struct TypeExtractor<double> { static TypeInfo Get() { return TypeInfo(ValueType::Double); } };
template <> struct TypeExtractor<std::string> { static TypeInfo Get() { return TypeInfo(ValueType::String); } };

// 数学系型の特殊化
template <> struct TypeExtractor<Vector2> { static TypeInfo Get() { return TypeInfo(ValueType::Vector2); } };
template <> struct TypeExtractor<Vector3> { static TypeInfo Get() { return TypeInfo(ValueType::Vector3); } };
template <> struct TypeExtractor<Vector4> { static TypeInfo Get() { return TypeInfo(ValueType::Vector4); } };
template <> struct TypeExtractor<Matrix3x3> { static TypeInfo Get() { return TypeInfo(ValueType::Matrix3x3); } };
template <> struct TypeExtractor<Matrix4x4> { static TypeInfo Get() { return TypeInfo(ValueType::Matrix4x4); } };
template <> struct TypeExtractor<Quaternion> { static TypeInfo Get() { return TypeInfo(ValueType::Quaternion); } };
template <> struct TypeExtractor<Color> { static TypeInfo Get() { return TypeInfo(ValueType::Color); } };
template <> struct TypeExtractor<TextureRef> { static TypeInfo Get() { return TypeInfo(ValueType::TextureRef); } };
template <> struct TypeExtractor<TextureCubeRef> { static TypeInfo Get() { return TypeInfo(ValueType::TextureCubeRef); } };

// コンテナ系型の部分特殊化（内部の型を再帰的に取得する）
template <typename T>
struct TypeExtractor<std::vector<T>> {
    static TypeInfo Get() {
        return TypeInfo(ValueType::Vector, { TypeExtractor<T>::Get() });
    }
};
template <typename K, typename V>
struct TypeExtractor<std::unordered_map<K, V>> {
    static TypeInfo Get() {
        return TypeInfo(ValueType::UnorderedMap, { TypeExtractor<K>::Get(), TypeExtractor<V>::Get() });
    }
};
template <typename T1, typename T2>
struct TypeExtractor<std::pair<T1, T2>> {
    static TypeInfo Get() {
        return TypeInfo(ValueType::Pair, { TypeExtractor<T1>::Get(), TypeExtractor<T2>::Get() });
    }
};
// MyAny型の特殊化
class MyAny;
template <>
struct TypeExtractor<MyAny> {
    static TypeInfo Get() {
        return TypeInfo(ValueType::Any);
    }
};

/// @brief 型 T から TypeInfo を取得する関数
template <typename T>
TypeInfo GetValueType() {
    return TypeExtractor<T>::Get();
}

/// @brief 文字列から TypeInfo を取得する関数
TypeInfo GetValueType(const std::string &typeStr);
