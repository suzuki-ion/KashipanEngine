#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <source_location>
#include <cstdint>

namespace KashipanEngine {

// 関数シグネチャ情報
struct FunctionSignatureInfo {
    std::string namespaceName;          // 名前空間（クラスを除くスコープ部）
    std::string className;              // クラス/構造体名（最も内側）なければ空
    std::string functionName;           // 関数名（operator/コンストラクタ/デストラクタ含む）
    std::vector<std::string> arguments; // 関数引数（各要素は1引数のテキスト）
    std::string returnType;             // 返り値型（コンストラクタ/デストラクタは空）

    // 補助情報
    std::string rawSignature;           // source_location::function_name の生文字列
    std::string trailingQualifiers;     // 例: const noexcept など
};

// 生シグネチャ文字列から解析（保持）
FunctionSignatureInfo ParseFunctionSignature(std::string sig);
// source_location から解析（保持）
FunctionSignatureInfo ParseFunctionSignature(const std::source_location &loc);

// ソースロケーションの包括情報
struct SourceLocationInfo {
    std::string filePath;               // ファイルパス
    std::uint_least32_t line = 0;       // 行
    std::uint_least32_t column = 0;     // 列
    FunctionSignatureInfo signature;    // 関数シグネチャ解析結果
    std::source_location raw;           // 元のロケーション
};

// std::source_location から包括情報を構築
SourceLocationInfo MakeSourceLocationInfo(const std::source_location &loc = std::source_location::current());

} // namespace KashipanEngine
