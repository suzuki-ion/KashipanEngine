# ユーティリティ：変換（Color / String）

対象ヘッダ：
- `Utilities/Conversion/ConvertColor.h`
- `Utilities/Conversion/ConvertString.h`

---

## 1. Color 変換（`ConvertColor`）

### 目的

色表現を `Vector4(r,g,b,a)` の **正規化レンジ（0.0f〜1.0f）** に統一するための変換です。

### 公開API

- `Vector4 ConvertColor(unsigned int color);`
- `Vector4 ConvertColor(const Vector4 &color);`

### 使いどころ

- 32bit 色（例：`0xRRGGBBAA` 形式の packed color）をシェーダ定数用の `Vector4` に変換したい
- 外部入力（GUI/設定ファイル等）で 0〜255 などのスケールを想定した値を正規化したい

### 使用例

```cpp
#include <KashipanEngine.h>

// 例：packed color を Vector4 へ
Vector4 c = KashipanEngine::ConvertColor(0xFF8040FF);

// 例：0〜255スケールの Vector4 を正規化
Vector4 input{255.0f, 128.0f, 64.0f, 255.0f};
Vector4 normalized = KashipanEngine::ConvertColor(input);
```

> packed color のバイト並び（R/G/B/Aの位置）は `ConvertColor(unsigned int)` 実装に従います。

---

## 2. 文字列変換（`ConvertString` / SJIS 変換）

### 目的

Windows API / ファイルパス / 外部データ等で混在しがちな文字列型・エンコーディングを変換します。

- `std::string`：UTF-8 を想定
- `std::wstring`：UTF-16 を想定

また、ShiftJIS との変換ユーティリティも提供します。

### 公開API

- `std::wstring ConvertString(const std::string &str);`
- `std::string ConvertString(const std::wstring &str);`
- `std::wstring ShiftJISToUTF16(const std::string &sjis);`
- `std::string UTF16ToUTF8(const std::wstring &utf16);`
- `std::string ShiftJISToUTF8(const std::string &sjis);`

### 使用例

```cpp
#include <KashipanEngine.h>

// UTF-8 -> UTF-16
std::wstring w = KashipanEngine::ConvertString(std::string("Assets/テクスチャ.png"));

// UTF-16 -> UTF-8
std::string u8 = KashipanEngine::ConvertString(w);

// ShiftJIS -> UTF-8
std::string utf8 = KashipanEngine::ShiftJISToUTF8("...sjis bytes...");
```

### 注意点

- ShiftJIS 変換が必要なのは、外部ファイルや古い環境由来の入力を扱う場合が中心です。
- Win32 API は wide 版（`W`）を使う場面が多いため、`std::wstring` への変換が必要になることがあります。
