# ユーティリティ：翻訳（Translation）

対象ヘッダ：`Utilities/Translation.h`

---

## 1. 目的

翻訳キー（例：`"engine.initialize.start"`）を、
現在の言語設定（language code）に対応するテキストへ変換します。

エンジン内ログ出力などでも利用されています。

---

## 2. 公開API

- `bool LoadTranslationFile(const std::string &filePath);`

- `const std::string &GetTranslationText(const std::string &lang, const std::string &key);`
- `const std::string &GetTranslationText(const std::string &key);`

- `inline const std::string &Translation(const std::string &lang, const std::string &key);`
- `inline const std::string &Translation(const std::string &key);`

- `const std::string &GetCurrentLanguage();`
- `void SetCurrentLanguage(const std::string &lang);`

- `const std::string &GetCurrentLanguageFontPath();`

---

## 3. 厳密な仕様（ヘッダから読み取れる部分）

- 翻訳が見つからない場合、返り値は「キー文字列」そのものになります。
- `Translation(...)` は `GetTranslationText(...)` の薄いラッパです。

---

## 4. 使用例

```cpp
#include <KashipanEngine.h>

// 例：言語を選ぶ
KashipanEngine::SetCurrentLanguage("ja");

// 例：現在言語で取得
const auto& t1 = KashipanEngine::Translation("engine.initialize.start");

// 例：言語コード指定で取得
const auto& t2 = KashipanEngine::Translation("en", "engine.initialize.start");

(void)t1;
(void)t2;
```

---

## 5. 翻訳ファイル配置

翻訳ファイルのパスは `EngineSettings` の translations 設定や、
`LoadTranslationFile(...)` の呼び出し側設計に従います。

（どの JSON 構造を要求するかは実装に依存します。詳細は `Translation.cpp` を参照してください。）
