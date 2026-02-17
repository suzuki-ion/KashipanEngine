# ユーティリティ一覧（`UtilitiesHeaders.h`）

エンジン側で用意しているユーティリティは `KashipanEngine/UtilitiesHeaders.h` で一括インクルードできます。

```cpp
#include <KashipanEngine.h>
// もしくは
#include "KashipanEngine/UtilitiesHeaders.h"
```

このページでは、`UtilitiesHeaders.h` が取り込む **全ユーティリティ**について、公開APIと「何をするものか」をヘッダ定義に基づいて列挙します。

---

## 1. 変換系

### 1.1 `Utilities/Conversion/ConvertColor.h`

**目的**: 32bit 色値や既存 `Vector4` を、正規化済みRGBA（`0.0f`〜`1.0f`）の `Vector4` として扱うための変換。

公開API：
- `Vector4 ConvertColor(unsigned int color);`
  - 32bit 色値を `Vector4(r,g,b,a)`（正規化）へ変換。
- `Vector4 ConvertColor(const Vector4 &color);`
  - `Vector4` を正規化RGBAへ変換（入力の表現が 0〜255 等の場合の正規化用途）。

### 1.2 `Utilities/Conversion/ConvertString.h`

**目的**: 文字列のエンコーディング/型変換（主に `std::string(UTF-8)` と `std::wstring(UTF-16)` の相互変換、および ShiftJIS 変換）。

公開API：
- `std::wstring ConvertString(const std::string &str);`
- `std::string ConvertString(const std::wstring &str);`
- `std::wstring ShiftJISToUTF16(const std::string &sjis);`
- `std::string UTF16ToUTF8(const std::wstring &utf16);`
- `std::string ShiftJISToUTF8(const std::string &sjis);`

---

## 2. ダイアログ系

### 2.1 `Utilities/Dialogs/MessageDialog.h`

**目的**: メッセージダイアログ表示。

名前空間：`KashipanEngine::Dialogs`

公開API：
- `bool ShowMessageDialog(const char *title, const char *message, bool isError = false);`
  - `isError=true` の場合はエラー系の表示として扱う想定。

### 2.2 `Utilities/Dialogs/DialogBase.h`
### 2.3 `Utilities/Dialogs/ConfirmDialog.h`
### 2.4 `Utilities/Dialogs/FileDialog.h`
### 2.5 `Utilities/Dialogs/FolderDialog.h`

現状のヘッダでは **公開API（クラス/関数）が定義されていません**（空の名前空間宣言のみ）。

- `DialogBase.h`：`namespace KashipanEngine {}` のみ
- `ConfirmDialog.h`：`namespace KashipanEngine {}` のみ
- `FileDialog.h`：`namespace KashipanEngine {}` のみ
- `FolderDialog.h`：`namespace KashipanEngine {}` のみ

> 実装予定のプレースホルダとして存在している状態です。

---

## 3. ファイルI/O

### 3.1 `Utilities/FileIO.h`

`FileIO.h` 自体は「窓口ヘッダ」で、以下のファイルI/Oユーティリティをまとめてインクルードします。

- `Utilities/FileIO/CSV.h`
- `Utilities/FileIO/Directory.h`
- `Utilities/FileIO/INI.h`
- `Utilities/FileIO/JSON.h`
- `Utilities/FileIO/RawFile.h`
- `Utilities/FileIO/TextFile.h`

#### 3.1.1 `Utilities/FileIO/CSV.h`

**目的**: CSV の読み書き。

公開API：
- `struct CSVData`
  - `std::string filePath;`
  - `std::vector<std::string> headers;`
  - `std::vector<std::vector<std::string>> rows;`
- `CSVData LoadCSV(const std::string &filePath, bool hasHeader = true);`
- `void SaveCSV(const std::string &filePath, const CSVData &data);`

#### 3.1.2 `Utilities/FileIO/Directory.h`

**目的**: ディレクトリ走査と、拡張子によるファイルフィルタ。

公開API：
- `struct DirectoryData`
  - `std::string directoryName;`
  - `std::vector<std::string> files;`
  - `std::vector<DirectoryData> subdirectories;`
- `bool IsDirectoryExist(const std::string &directoryPath);`
- `DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive = false, bool isFullPath = true);`
- `DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::vector<std::string> &extensions);`
- `DirectoryData GetDirectoryDataByExtension(const std::string &directoryPath, const std::vector<std::string> &extensions, bool isRecursive = false, bool isFullPath = true);`

挙動の要点：
- `isRecursive=true` の場合、サブディレクトリを再帰的に `DirectoryData::subdirectories` に構築。
- `isFullPath=false` の場合、`files` はファイル名のみ（`filename()`）を格納。

#### 3.1.3 `Utilities/FileIO/INI.h`

**目的**: INI の読み書き（セクション/キー/値）。

公開API：
- `struct INIData`
  - `std::string filePath;`
  - `std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;`
- `INIData LoadINIFile(const std::string &filePath);`
- `void SaveINIFile(const INIData &iniData);`

挙動の要点（実装より）：
- コメント行 `;` / `#` はスキップ。
- `LoadINIFile` はファイルを開けない場合、空の `INIData` を返す（例外は投げない）。

#### 3.1.4 `Utilities/FileIO/JSON.h`

**目的**: nlohmann::json を使った JSON の読み書きと補助関数。

型：
- `using JSON = nlohmann::json;`（`Json` / `json` も同義）

公開API：
- 基本
  - `JSON LoadJSON(const std::string &filepath);`
  - `bool SaveJSON(const JSON &jsonData, const std::string &filepath, int indent = 4);`
- 安全なキー取得
  - `template<typename T> std::optional<T> GetJSONValue(const JSON &json, const std::string &key);`
  - `template<typename T> T GetJSONValueOrDefault(const JSON &json, const std::string &key, const T &defaultValue);`
- ネストパス対応
  - `std::optional<JSON> GetNestedJSONValue(const JSON &json, const std::string &path);`
    - 例：`"object.array[0].value"`
- 検証
  - `bool ValidateJSONStructure(const JSON &json, const std::vector<std::string> &requiredKeys);`
  - `bool IsJSONFileValid(const std::string &filepath);`
- 合成
  - `JSON MergeJSON(const JSON &base, const JSON &overlay, bool deepMerge = true);`
- 配列操作
  - `bool AppendToJSONArray(JSON &json, const std::string &arrayKey, const JSON &value);`
  - `bool RemoveFromJSONArray(JSON &json, const std::string &arrayKey, size_t index);`
- 表示
  - `std::string JSONToFormattedString(const JSON &json, int indent = 4);`
  - `void PrintJSON(const JSON &json, const std::string &title = "JSON Data");`

挙動の要点：
- `LoadJSON()` は `JSON::parse(..., allow_exceptions=false, ignore_comments=true)` 相当でパース（不正 JSON でも例外にしない設定）。

#### 3.1.5 `Utilities/FileIO/TextFile.h`

**目的**: テキストファイル（行配列）の読み書き。

公開API：
- `struct TextFileData`
  - `std::string filePath;`
  - `std::vector<std::string> lines;`
- `TextFileData LoadTextFile(const std::string &filePath);`
- `void SaveTextFile(const TextFileData &textFileData);`

挙動の要点（実装より）：
- `LoadTextFile` は開けない場合、空の `TextFileData` を返す（例外は投げない）。

#### 3.1.6 `Utilities/FileIO/RawFile.h`

**目的**: バイナリとしてファイルを読み書きし、先頭バイト列からファイル種別（拡張子に依存しない）を推測する。

公開API：
- `enum class FileType`
  - テキスト（`txt/json/xml/html`）
  - 画像（`png/jpg/gif/bmp/ico/webp/tiff`）
  - メディア（`wav/mp3/ogg/flac/mp4/avi/mkv`）
  - アーカイブ/その他（`pdf/zip/rar/sevenz/gzip/bin/unknown`）
- `struct RawFileData`
  - `std::string filePath;`
  - `std::vector<uint8_t> data;`
  - `size_t size;`
  - `FileType fileType;`
- `bool IsFileExist(const std::string &filePath);`
- `RawFileData LoadFile(const std::string &filePath, size_t detectBytes = 64 * 1024);`
- `void SaveFile(const RawFileData &fileData);`
- `FileType DetectFileTypeFromFile(const std::string &filePath, size_t detectBytes = 64 * 1024);`

挙動の要点（実装より）：
- `LoadFile` は `detectBytes`（デフォルト 64KB）以内の先頭データで `fileType` を推測。
- `LoadFile` / `SaveFile` / `DetectFileTypeFromFile` は、読み書き失敗時に `std::runtime_error` を投げる。

---

## 4. 数学ユーティリティ

### `Utilities/MathUtils.h`

**目的**: `MathUtils/*` 配下の数学系ツールの集約。

主に含まれるもの：
- `MathUtils/Easings.h`（イージング関数群）
- `MathUtils/Vector2.h` / `Vector3.h` / `Vector4.h`
- `MathUtils/Matrix3x3.h` / `Matrix4x4.h`
- `MathUtils/PerlinNoise.h` / `FractalNoise.h`
- `MathUtils/SphericalCoordinates.h`

補足：
- `MathUtils.h` は `#define M_PI ...` を定義します（単位円周率）。

---

## 5. アクセス制御（パスキー）

### `Utilities/Passkeys.h`

**目的**: 特定の関数/コンストラクタを「特定クラス（または関数）からしか呼べない」ようにするためのパターン。

公開API：
- `template<typename T> class Passkey`
  - `friend T;` を持ち、`T` のみが `Passkey<T>` を生成できる。
- `class PasskeyForWinMain`
  - `friend int ::WinMain(...)`（WinMain からのみ生成可能）
- `class PasskeyForGameEngineMain`
  - `friend int Execute(PasskeyForWinMain, const std::string &)`
- `class PasskeyForCrashHandler`
  - `friend LONG WINAPI CrashHandler(EXCEPTION_POINTERS *exceptionInfo)`

---

## 6. 乱数

### `Utilities/RandomValue.h`

**目的**: 簡易乱数 API（整数/浮動小数/真偽値）。

公開API：
- `int GetRandomInt(int min, int max);`
- `float GetRandomFloat(float min, float max);`
- `double GetRandomDouble(double min, double max);`
- `bool GetRandomBool(float trueProbability = 0.5f);`

---

## 7. ソースロケーション/関数シグネチャ解析

### `Utilities/SourceLocation.h`

**目的**: `std::source_location` から、ファイル/行/列に加え、関数シグネチャ文字列を解析して構造化情報として扱う。

公開API：
- `struct FunctionSignatureInfo`
  - `std::vector<std::string> scopes;`（名前空間やクラスのスコープ列）
  - `std::string functionName;`
  - `std::vector<std::string> arguments;`
  - `std::string returnType;`
  - `std::string rawSignature;`
  - `std::string trailingQualifiers;`
- `FunctionSignatureInfo ParseFunctionSignature(std::string sig);`
- `FunctionSignatureInfo ParseFunctionSignature(const std::source_location &loc);`
- `struct SourceLocationInfo`
  - `std::string filePath;`
  - `std::uint_least32_t line;`
  - `std::uint_least32_t column;`
  - `FunctionSignatureInfo signature;`
  - `std::source_location raw;`
- `SourceLocationInfo MakeSourceLocationInfo(const std::source_location &loc = std::source_location::current());`

---

## 8. 文字列テンプレート（プレースホルダー置換）

### `Utilities/TemplateLiteral.h`

**目的**: テンプレート文字列内の `${name}` プレースホルダーへ値を差し込み、最終文字列を生成する。

公開API（主要）：
- `TemplateLiteral(std::string_view templ);`
- `const std::string& Template() const noexcept;`
- `void SetTemplate(std::string_view templ);`
- `const std::vector<std::string>& Placeholders() const noexcept;`
- `const std::string& Get(std::string_view key) const;`
- `bool HasPlaceholder(std::string_view key) const noexcept;`
- `template<class T> void Set(std::string_view key, T&& value);`
  - 値は string-like / `operator<<` 可能型 / コンテナ型（string 以外の iterable）をサポート。
- `std::string Render() const;`
- `Slot operator[](std::string_view key);`（`temp["value"] = 10;` のように書ける）

---

## 9. 時間

### `Utilities/TimeUtils.h`

**目的**: 
- `GameEngine` 管理のデルタタイム
- 現在時刻/実行時間取得
- 簡易計測（ラベル付き）
- 進行型タイマー（`TimeUtils.h` 内の `KashipanEngine::GameTimer`）

公開API：
- デルタタイム（エンジン専用更新）
  - `void UpdateDeltaTime(Passkey<GameEngine>);`
  - `float GetDeltaTime();`
- ゲームスピード
  - `void SetGameSpeed(float speed);`
  - `float GetGameSpeed();`
- 現在時刻
  - `int GetNowTimeYear();` / `int GetNowTimeMonth();` / `int GetNowTimeDay();`
  - `int GetNowTimeHour();` / `int GetNowTimeMinute();`
  - `long long GetNowTimeSecond();` / `long long GetNowTimeMillisecond();`
  - `TimeRecord GetNowTime();`
  - `std::string GetNowTimeString(const std::string &format = "%Y-%m-%d %H:%M:%S");`
- 実行時間（プログラム開始から）
  - `int GetGameRuntimeYear();` / `int GetGameRuntimeMonth();` / `int GetGameRuntimeDay();`
  - `int GetGameRuntimeHour();` / `int GetGameRuntimeMinute();`
  - `long long GetGameRuntimeSecond();` / `long long GetGameRuntimeMillisecond();`
  - `TimeRecord GetGameRuntime();`
- 計測
  - `void StartTimeMeasurement(const std::string &label);`
  - `TimeRecord EndTimeMeasurement(const std::string &label);`

`TimeRecord`：
- `int year, month, day, hour, minute;`
- `long long second, millisecond;`

`KashipanEngine::GameTimer`（※ `TimeUtils.h` 側）：
- `GameTimer(float duration, bool loop = false)`
- `void Update()`（内部で `GetDeltaTime()` を使用）
- `void Start(float duration, bool loop = false)` / `Stop()` / `Reset()` / `Pause()` / `Resume()`
- `bool IsActive() const` / `bool IsFinished() const`
- `float GetProgress() const` / `GetReverseProgress() const`
- `float GetRemainingTime() const` / `GetElapsedTime() const`
- `float GetDuration() const` / `void SetDuration(float)` / `void SetLoop(bool)`

> 注意: `Utilities/GameTimer.h` にも `GameTimer` が存在しますが、こちらは **グローバル名前空間**で定義され、`Update(float deltaTime=...)` の形です。用途/名前衝突に注意してください。

---

## 10. 翻訳

### `Utilities/Translation.h`

**目的**: 文字列キーを言語別テキストへ変換（翻訳ファイルは JSON を想定）。

公開API：
- `bool LoadTranslationFile(const std::string &filePath);`
- `const std::string &GetTranslationText(const std::string &lang, const std::string &key);`
- `const std::string &GetTranslationText(const std::string &key);`
- `inline const std::string &Translation(const std::string &lang, const std::string &key);`
- `inline const std::string &Translation(const std::string &key);`
- `const std::string &GetCurrentLanguage();`
- `void SetCurrentLanguage(const std::string &lang);`
- `const std::string &GetCurrentLanguageFontPath();`

挙動の要点（ヘッダ記述より）：
- 翻訳が見つからない場合は「キー文字列」をそのまま返す。

---

## 11. `Utilities/GameTimer.h`（別実装）

`UtilitiesHeaders.h` には `Utilities/GameTimer.h` も含まれます。
この `GameTimer` は **`namespace KashipanEngine` ではなくグローバル名前空間**に定義されています。

公開API（抜粋）：
- `GameTimer(float duration, bool loop = false);`
- `void Update(float deltaTime = 1.0f / 60.0f);`
- `Start/Stop/Reset/Pause/Resume`
- `IsActive/IsFinished`
- `GetProgress/GetReverseProgress/GetRemainingTime/GetElapsedTime`

> `KashipanEngine::GameTimer`（`TimeUtils.h` 側）と併存しているため、利用側は意図した方を明確に選んでください。
