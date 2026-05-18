# ユーティリティ：文字列テンプレート（`TemplateLiteral`）

対象ヘッダ：`Utilities/TemplateLiteral.h`

---

## 1. 目的

テンプレート文字列中の `${name}` プレースホルダーへ値を差し込み、完成した文字列を生成します。

- UI 文言
- ログ文言
- デバッグ用の整形文字列

など、簡易なテンプレート用途を想定しています。

---

## 2. 公開API（主要）

- `TemplateLiteral()`
- `explicit TemplateLiteral(std::string_view templ)`
- `const std::string &Template() const noexcept`
- `void SetTemplate(std::string_view templ)`
- `const std::vector<std::string> &Placeholders() const noexcept`
- `const std::string &Get(std::string_view key) const`
- `bool HasPlaceholder(std::string_view key) const noexcept`
- `template <class T> void Set(std::string_view key, T &&value)`
- `std::string Render() const`

代入用：
- `Slot operator[](std::string_view key)`

---

## 3. 値型の厳密な条件

`Set(key, value)` で受け付ける型はヘッダ実装で以下に分類されます。

- **文字列ライク**：`std::string`, `std::string_view`, `const char*`, `char*`
- **ストリーム出力可能**：`operator<<(std::ostream&, const T&)` が定義されている型
- **コンテナライク**：`begin/end` があり、かつ文字列ライクではない型
  - 要素は文字列ライク、またはストリーム出力可能である必要があります

コンテナを渡した場合は
- 内部に各要素の文字列配列を保持し
- 既定では `", "` で join した文字列も `${name}` の値として設定

します。

サポート外型を渡すと `static_assert` でコンパイルエラーになります。

---

## 4. 使用例（スカラー値）

```cpp
#include <KashipanEngine.h>

KashipanEngine::TemplateLiteral t("HP=${hp}, Score=${score}");

t["hp"] = 120;
t["score"] = 3450;

std::string s = t.Render();
```

---

## 5. 使用例（コンテナ）

```cpp
#include <KashipanEngine.h>

KashipanEngine::TemplateLiteral t("Loaded: ${files}");

t.Set("files", std::vector<std::string>{"a.png", "b.png"});

// ${files} は "a.png, b.png" として展開される
auto s = t.Render();
```

---

## 6. 注意点

- `${name}` の解析はテンプレート文字列に対して行われ、`SetTemplate()` で再解析されます。
- プレースホルダー名の有効範囲・文法は実装（`TemplateLiteral.cpp`）に従います。
