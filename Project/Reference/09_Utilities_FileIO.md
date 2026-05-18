# ユーティリティ：ファイル I/O（CSV / Directory / INI / JSON / RawFile / TextFile）

対象ヘッダ：
- `Utilities/FileIO.h`（窓口）
- `Utilities/FileIO/CSV.h`
- `Utilities/FileIO/Directory.h`
- `Utilities/FileIO/INI.h`
- `Utilities/FileIO/JSON.h`
- `Utilities/FileIO/RawFile.h`
- `Utilities/FileIO/TextFile.h`

---

## 0. 全体像（例外方針の違い）

ファイル系は **例外を投げるもの** と **失敗時に空データを返すもの** が混在します。

- 例外あり（`std::runtime_error`）
  - `CSV`：読み込み/保存
  - `RawFile`：読み込み/保存/判定
- 例外なし（失敗時に空データ）
  - `INI`：読み込み/保存（保存は失敗しても return）
  - `TextFile`：読み込み/保存

> JSON は `LoadJSON()` 自体は例外を投げないパース設定を使いますが、入力ファイルの打开可否など呼び出し側の前提に依存します。

---

## 1. CSV（`Utilities/FileIO/CSV.h`）

### 目的

CSV の読み書き。

### 公開API

- `struct CSVData`
  - `std::string filePath;`
  - `std::vector<std::string> headers;`
  - `std::vector<std::vector<std::string>> rows;`
- `CSVData LoadCSV(const std::string &filePath, bool hasHeader = true);`
- `void SaveCSV(const std::string &filePath, const CSVData &data);`

### 使用例

```cpp
#include <KashipanEngine.h>

auto data = KashipanEngine::LoadCSV("Assets/table.csv", true);

// 1行目（headers）を使わない形式なら hasHeader = false
// auto data2 = KashipanEngine::LoadCSV("Assets/table_no_header.csv", false);

KashipanEngine::CSVData out;
out.headers = {"A","B"};
out.rows = {{"1","2"},{"3","4"}};
KashipanEngine::SaveCSV("Assets/out.csv", out);
```

### 注意点（実装から分かる仕様）

- カンマ `,` の単純分割で、クォートやエスケープには対応しません。
- `LoadCSV` はファイルが開けない場合 `std::runtime_error` を投げます。

---

## 2. Directory（`Utilities/FileIO/Directory.h`）

### 目的

ディレクトリ走査結果を `DirectoryData` として取得し、必要なら拡張子でフィルタします。

### 公開API

- `struct DirectoryData`
  - `std::string directoryName;`
  - `std::vector<std::string> files;`
  - `std::vector<DirectoryData> subdirectories;`

- `bool IsDirectoryExist(const std::string &directoryPath);`
- `DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive = false, bool isFullPath = true);`
- `DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::vector<std::string> &extensions);`
- `DirectoryData GetDirectoryDataByExtension(const std::string &directoryPath, const std::vector<std::string> &extensions, bool isRecursive = false, bool isFullPath = true);`

### 使用例

```cpp
#include <KashipanEngine.h>

if (KashipanEngine::IsDirectoryExist("Assets")) {
    // 再帰的に Assets を走査し、フルパスで返す
    auto dir = KashipanEngine::GetDirectoryData("Assets", true, true);

    // png と jpg のみ抽出
    auto images = KashipanEngine::GetDirectoryDataByExtension(dir, {".png", ".jpg"});
}
```

### 注意点

- `isFullPath=false` の場合、結果は「ファイル名のみ」になるため、後続処理で元ディレクトリと結合する必要があります。

---

## 3. INI（`Utilities/FileIO/INI.h`）

### 目的

INI の読み込みと、`INIData` 構造体としての保持・保存。

### 公開API

- `struct INIData`
  - `std::string filePath;`
  - `std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;`

- `INIData LoadINIFile(const std::string &filePath);`
- `void SaveINIFile(const INIData &iniData);`

### 使用例

```cpp
#include <KashipanEngine.h>

auto ini = KashipanEngine::LoadINIFile("Assets/config.ini");

// 値参照（存在チェックは呼び出し側で）
// auto value = ini.sections["Graphics"]["VSync"]; 

// 保存
ini.filePath = "Assets/config_out.ini";
KashipanEngine::SaveINIFile(ini);
```

### 実装上の仕様

- 行頭の `;` / `#` はコメント扱い。
- `LoadINIFile` はファイルを開けない場合、空の `INIData` を返します（例外なし）。

---

## 4. JSON（`Utilities/FileIO/JSON.h`）

### 目的

`nlohmann::json` の読み書きと、キー取得・パス指定アクセス・マージ・配列操作など。

### 公開API（主要）

- 型エイリアス：`using JSON = nlohmann::json;`（`Json` / `json` も同義）

- `JSON LoadJSON(const std::string &filepath);`
- `bool SaveJSON(const JSON &jsonData, const std::string &filepath, int indent = 4);`

- `template<typename T> std::optional<T> GetJSONValue(const JSON &json, const std::string &key);`
- `template<typename T> T GetJSONValueOrDefault(const JSON &json, const std::string &key, const T &defaultValue);`

- `std::optional<JSON> GetNestedJSONValue(const JSON &json, const std::string &path);`

- `bool ValidateJSONStructure(const JSON &json, const std::vector<std::string> &requiredKeys);`
- `bool IsJSONFileValid(const std::string &filepath);`

- `JSON MergeJSON(const JSON &base, const JSON &overlay, bool deepMerge = true);`

- `bool AppendToJSONArray(JSON &json, const std::string &arrayKey, const JSON &value);`
- `bool RemoveFromJSONArray(JSON &json, const std::string &arrayKey, size_t index);`

- `std::string JSONToFormattedString(const JSON &json, int indent = 4);`
- `void PrintJSON(const JSON &json, const std::string &title = "JSON Data");`

### 使用例：安全にキー取得

```cpp
#include <KashipanEngine.h>

KashipanEngine::JSON j = KashipanEngine::LoadJSON("Assets/config.json");

// optional で受け取る
if (auto v = KashipanEngine::GetJSONValue<int>(j, "maxFPS")) {
    int maxFps = *v;
    (void)maxFps;
}

// default 付き
int maxFps = KashipanEngine::GetJSONValueOrDefault<int>(j, "maxFPS", 60);
```

### 使用例：ネストパス

```cpp
#include <KashipanEngine.h>

auto j = KashipanEngine::LoadJSON("Assets/config.json");

// 例: "render.targets[0].name"
if (auto v = KashipanEngine::GetNestedJSONValue(j, "render.targets[0].name")) {
    // *v は JSON
}
```

### 実装上の仕様

- `LoadJSON()` は allow_exceptions=false 相当でパースしており、JSON 不正でも例外にしない設定です。
  - ただし戻り値 `JSON` が「破損」状態になり得るため、利用側での検証が必要です。

---

## 5. RawFile（`Utilities/FileIO/RawFile.h`）

### 目的

ファイルをバイト列として読み込み、先頭バイト列からファイル種別を推測します（拡張子に依存しない）。

### 公開API

- `enum class FileType`（`txt/json/xml/html/png/jpg/.../bin/unknown`）
- `struct RawFileData`
  - `std::string filePath;`
  - `std::vector<uint8_t> data;`
  - `size_t size;`
  - `FileType fileType;`

- `bool IsFileExist(const std::string &filePath);`
- `RawFileData LoadFile(const std::string &filePath, size_t detectBytes = 64 * 1024);`
- `void SaveFile(const RawFileData &fileData);`
- `FileType DetectFileTypeFromFile(const std::string &filePath, size_t detectBytes = 64 * 1024);`

### 使用例

```cpp
#include <KashipanEngine.h>

if (KashipanEngine::IsFileExist("Assets/anyfile")) {
    auto raw = KashipanEngine::LoadFile("Assets/anyfile");

    // 推測された fileType を参照
    auto t = raw.fileType;
    (void)t;
}
```

### 実装上の仕様

- `LoadFile` / `SaveFile` / `DetectFileTypeFromFile` は、失敗時に `std::runtime_error` を投げます。
- `detectBytes` は「種別推定に使う先頭の最大バイト数」です。

---

## 6. TextFile（`Utilities/FileIO/TextFile.h`）

### 目的

テキストを「行配列」として読み書きします。

### 公開API

- `struct TextFileData`
  - `std::string filePath;`
  - `std::vector<std::string> lines;`

- `TextFileData LoadTextFile(const std::string &filePath);`
- `void SaveTextFile(const TextFileData &textFileData);`

### 使用例

```cpp
#include <KashipanEngine.h>

auto txt = KashipanEngine::LoadTextFile("Assets/readme.txt");
for (const auto& line : txt.lines) {
    // ...
}

KashipanEngine::TextFileData out;
out.filePath = "Assets/out.txt";
out.lines = {"line1", "line2"};
KashipanEngine::SaveTextFile(out);
```

### 実装上の仕様

- `LoadTextFile` は失敗しても例外を投げず、空データを返します。
