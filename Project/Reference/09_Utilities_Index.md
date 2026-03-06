# ユーティリティ（カテゴリ別インデックス）

エンジン側で用意しているユーティリティは `KashipanEngine/UtilitiesHeaders.h` で一括インクルードできます。

```cpp
#include <KashipanEngine.h>
// もしくは
#include "KashipanEngine/UtilitiesHeaders.h"
```

この章はユーティリティをカテゴリごとに分割し、各カテゴリの目的・前提・具体的な使い方を記述します。

---

## カテゴリ一覧

- `09_Utilities_Conversion.md`：変換（Color / String）
- `09_Utilities_Dialogs.md`：ダイアログ
- `09_Utilities_FileIO.md`：ファイルI/O（CSV / Directory / INI / JSON / RawFile / TextFile）
- `09_Utilities_Math.md`：数学（`MathUtils`）
- `09_Utilities_Passkeys.md`：パスキー（アクセス制御）
- `09_Utilities_Random.md`：乱数
- `09_Utilities_SourceLocation.md`：ソースロケーション/関数シグネチャ解析
- `09_Utilities_TemplateLiteral.md`：文字列テンプレート
- `09_Utilities_Time.md`：時間（デルタタイム、計測、タイマー）
- `09_Utilities_Translation.md`：翻訳

---

## 旧ページについて

従来の `09_Utilities.md` は「全ユーティリティの単一ページ列挙」でしたが、
説明の厳密化と使用例の拡充のためカテゴリ別ページへ分割しました。

- 旧：`09_Utilities.md`
- 新：本インデックス＋カテゴリ別ページ群
