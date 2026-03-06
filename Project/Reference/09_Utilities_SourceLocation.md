# ユーティリティ：ソースロケーション / 関数シグネチャ解析

対象ヘッダ：`Utilities/SourceLocation.h`

---

## 1. 目的

`std::source_location` を利用して、
- どのファイルの何行か
- どの関数から呼ばれたか

を取得し、さらに関数シグネチャ文字列をパースして「スコープ」「関数名」「引数」「戻り値型」等を構造化します。

ログ/アサート/デバッグ出力で、人間が読める形のメタ情報を付与する用途を想定しています。

---

## 2. 公開API

### 2.1 関数シグネチャ

- `struct FunctionSignatureInfo`
  - `std::vector<std::string> scopes;`
  - `std::string functionName;`
  - `std::vector<std::string> arguments;`
  - `std::string returnType;`
  - `std::string rawSignature;`
  - `std::string trailingQualifiers;`

- `FunctionSignatureInfo ParseFunctionSignature(std::string sig);`
- `FunctionSignatureInfo ParseFunctionSignature(const std::source_location &loc);`

### 2.2 ソースロケーション包括情報

- `struct SourceLocationInfo`
  - `std::string filePath;`
  - `std::uint_least32_t line;`
  - `std::uint_least32_t column;`
  - `FunctionSignatureInfo signature;`
  - `std::source_location raw;`

- `SourceLocationInfo MakeSourceLocationInfo(const std::source_location &loc = std::source_location::current());`

---

## 3. 使用例

```cpp
#include <KashipanEngine.h>
#include <iostream>

void Foo() {
    auto info = KashipanEngine::MakeSourceLocationInfo();

    std::cout << info.filePath << ":" << info.line << "\n";
    std::cout << info.signature.functionName << "\n";

    // rawSignature はコンパイラ依存の文字列
    std::cout << info.signature.rawSignature << "\n";
}
```

---

## 4. 注意点（厳密仕様として）

- `std::source_location::function_name()` の形式はコンパイラ依存です。
  - そのため `ParseFunctionSignature(...)` の解析結果も、コンパイラ/標準ライブラリの実装差の影響を受けます。
- 解析が失敗した場合のフィールド内容は、実装（`SourceLocation.cpp` 相当）に従います。
