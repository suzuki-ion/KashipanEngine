# Utilities（Dialogs 除外）

対象コード:

- `KashipanEngine/UtilitiesHeaders.h`
- `KashipanEngine/Utilities/*`

このドキュメントは `UtilitiesHeaders.h` の include 一覧から **Dialogs 系を除外** してまとめています。

## 1. Conversion

- `Utilities/Conversion/ConvertColor.h`
  - 色表現の変換ユーティリティ
- `Utilities/Conversion/ConvertString.h`
  - 文字列変換ユーティリティ（例: 文字コード/フォーマット変換用途）

## 2. FileIO

`Utilities/FileIO.h` は FileIO 系の集約ヘッダで、配下に以下があります。

- `Utilities/FileIO/JSON.h` / `.cpp`
  - JSON ロード/保存（`LoadJSON` など）
- `Utilities/FileIO/CSV.h` / `.cpp`
- `Utilities/FileIO/INI.h` / `.cpp`
- `Utilities/FileIO/TextFile.h` / `.cpp`
- `Utilities/FileIO/RawFile.h` / `.cpp`
- `Utilities/FileIO/Directory.h` / `.cpp`

## 3. MathUtils

- `Utilities/MathUtils.h`（集約）
- `Utilities/MathUtils/Matrix3x3.*`
- `Utilities/MathUtils/Matrix4x4.*`
- `Utilities/MathUtils/Vector2.*`
- `Utilities/MathUtils/Vector3.*`
- `Utilities/MathUtils/Easings.*`
- `Utilities/MathUtils/PerlinNoise.*`
- `Utilities/MathUtils/FractalNoise.*`

## 4. Passkeys

- `Utilities/Passkeys.h`

エンジン内部 API を「特定の呼び出し元に限定する」ための passkey 型定義。

例:

- `PasskeyForGameEngineMain`
- `PasskeyForWinMain`

（実体は `Passkeys.h` を参照）

## 5. Random

- `Utilities/RandomValue.h/.cpp`

乱数ユーティリティ。

## 6. SourceLocation

- `Utilities/SourceLocation.h/.cpp`

ソース位置（ファイル名/行/関数名等）の補助。

※ ログの `LogScope` は `std::source_location` を利用します（`Debug/Logger.h`）。

## 7. TemplateLiteral

- `Utilities/TemplateLiteral.h/.cpp`

テンプレートリテラル（文字列展開）関連のユーティリティ。

## 8. Time

- `Utilities/TimeUtils.h/.cpp`

時間計測・デルタタイム等のユーティリティ。

## 9. Translation

- `Utilities/Translation.h/.cpp`

翻訳（ローカライズ）関連。

- `Translation(key)` のようなキー参照
- `EngineSettings::Translations` の `languageFilePaths` と連携する設計
