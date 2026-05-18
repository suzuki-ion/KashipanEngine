# ユーティリティ：ダイアログ（Dialogs）

対象ヘッダ：
- `Utilities/Dialogs/MessageDialog.h`
- `Utilities/Dialogs/DialogBase.h`
- `Utilities/Dialogs/ConfirmDialog.h`
- `Utilities/Dialogs/FileDialog.h`
- `Utilities/Dialogs/FolderDialog.h`

---

## 1. `MessageDialog`（実装あり）

### 目的

簡易なメッセージ表示（情報/エラー）を行います。

### 公開API

名前空間：`KashipanEngine::Dialogs`

- `bool ShowMessageDialog(const char *title, const char *message, bool isError = false);`

返り値：
- `true`：ユーザーが OK を押した
- `false`：ユーザーがキャンセルを押した

### 使用例

```cpp
#include <KashipanEngine.h>

KashipanEngine::Dialogs::ShowMessageDialog("Info", "Hello");
KashipanEngine::Dialogs::ShowMessageDialog("Error", "Failed to load asset", true);
```

---

## 2. それ以外（現状はプレースホルダ）

以下のヘッダは現状、公開API（クラス/関数）が宣言されていません（空の名前空間宣言のみ）。

- `DialogBase.h`
- `ConfirmDialog.h`
- `FileDialog.h`
- `FolderDialog.h`

そのため、現時点ではドキュメント化できる「厳密なAPI仕様」が存在しません。

> 今後 API が追加された場合、このページに追記します。
