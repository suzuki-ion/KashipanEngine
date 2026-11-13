# リポジトリのフォルダ構成

```text
KashipanEngine/
├─ SourceFiles/       C++ソース・ヘッダー（エンジン、Hub、Checker、Application、MyStd）
├─ Build/             Visual Studioソリューション・プロジェクト・共通パス設定
├─ Externals/         外部ライブラリとそのビルド設定
├─ EngineData/
│  ├─ AssetsTemplate/ 新規ゲーム作成時にコピーするテンプレート
│  ├─ EditorTools/    エディターが読み込むAngelScript拡張と辞書
│  ├─ Locales/        ゲーム選択前にも使うエンジン共通翻訳
│  └─ UI/            Splash・LauncherのHTML/CSS/JS
├─ GameProjects/      <Game>/Project.json、Assets、Logs、Dumps、SceneBackups
├─ EngineTools/       ビルド・配布・生成スクリプト、外部CPUプロファイラー
├─ EngineTests/       回帰テストのソースと実行スクリプト
├─ Docs/              開発文書、Reference/以下にHTMLリファレンス
├─ Websites/Portfolio/ ポートフォリオサイト
├─ UserSettings/      ローカルの個人設定（Git管理外）
└─ Generated/         Outputs、Obj、WorkCopy、Distribution、テスト出力等（Git管理外）
```

## ビルドと実行

`Build/KashipanEngine.sln` をVisual Studio 2026で開きます。
`Build/RepositoryPaths.props` がリポジトリのルートを `RepositoryRoot` として解決するため、
Visual Studioのプロジェクト位置や作業ディレクトリは実行用データの基準にはなりません。

KashipanEngineのソリューションエクスプローラーでは、「すべてのファイルを表示」をオフにして
フィルター表示を使用します。フィルターはリポジトリ直下を基準とする実階層
（`SourceFiles/`、`EngineData/`、`Externals/`、`EngineTools/`、`Build/` など）に合わせています。
プロジェクトファイルは `Build/` 内にあり、ソース等はその外側にあるため、
「すべてのファイルを表示」に切り替えた場合は、それらがプロジェクト直下に並ぶ表示になります。
ファイルをプロジェクトに追加・移動した際は、
`powershell -NoProfile -ExecutionPolicy Bypass -File EngineTools/SyncVisualStudioFilters.ps1` で
フィルターを再同期できます。`-Check` を付けると、書き換えずに同期状態を確認します。

KashipanEngineのDebug/Development/Releaseでは、ビルド前イベントの先頭でも同じ同期処理を実行します。
Windows標準のWindows PowerShell 5.1で動作し、Pythonは不要です。同期失敗時はビルドを停止します。
同期済みの場合は `.vcxproj.filters` を書き換えません。ビルド不要と判定されて
ビルド前イベントが省略された場合は、外部ツールから手動で同期してください。

Visual Studioの「ツール → 外部ツール」には、次の内容で登録できます（PCごとの設定）。

| 項目 | 設定 |
| --- | --- |
| タイトル | `KashipanEngine フィルター同期` |
| コマンド | `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe`（Windowsのインストール先に合わせる） |
| 引数 | `-NoProfile -ExecutionPolicy Bypass -File "$(SolutionDir)..\EngineTools\SyncVisualStudioFilters.ps1"` |
| 初期ディレクトリ | `$(SolutionDir)..` |
| 出力ウィンドウを使用 | オン |

`Build/KashipanEngine.sln` を開いた状態で、このメニューを実行します。
対象はプロジェクトに登録済みのファイルです。ディスク上の未登録ファイルをビルド対象へ追加する処理は行いません。
更新後の階層が表示に反映されない場合は、プロジェクトを再読み込みしてください。

Debug/Developmentのビルド後処理は、exeの隣の `EngineRoot.txt` にリポジトリルートを記録します。
`ProjectPaths::EngineRoot()` はその位置、`InEngineData()` はその下の `EngineData/` を基準にします。
ゲームの一覧・作成・名前指定の起動は `GameProjects/` を使い、`--project <絶対パス>` も利用できます。
シーン等に保存する `Assets/...` の論理パスは変更しません。

`EngineTools/CopyBuildAssets.bat [Debug|Development|Release] [Platform]` でコピー処理だけを再実行できます。
WorkCopy同期は既存のログ・ダンプ・個人設定を除外して保持します。
WorkCopyは `Generated/WorkCopy/<Platform>/<Configuration>`、配布物は
`Generated/Distribution/<Platform>/<Configuration>` へ出力します。

Release配布はexeの隣に `Assets/` を置き、`EngineRoot.txt` を含めません。
Development配布は `GameProjects/`、`EngineData/`、`Docs/Reference/`、`EngineTools/Profiler/` を同梱します。
配布先のディレクトリ名も実行時の参照と一致させます。

## ローカルデータ

ログとダンプは開いているゲームの `Logs/` と `Dumps/` へ出力します。
ゲーム未選択の起動処理ではexeの隣を使い、書き込めない場合は従来どおり
`%LOCALAPPDATA%/KashipanEngine/<ProjectName>/` にフォールバックします。
シーンバックアップは既存の論理パス解決を通してゲームごとに保存します。

移行前のProject直下にあったログ・ダンプ・バックアップ・IDEキャッシュ等は
`Generated/LegacyProject/` に保持します。既存のWorkCopy・配布物内の診断データや設定は
`Generated/LegacyRuntime/` にも退避しています。過去の出力を現在のゲームへ自動的に割り当てることはしません。
旧ルート直下のデバッグ出力とシーンバックアップは `Generated/LegacyRoot/` に保持します。
既存WorkCopy内の実行ファイルは移行前のパスを含むため、新しいビルドの同期後に使用してください。

## テスト

`python EngineTests/RepositoryLayoutRegression.py` でビルド項目と配置を検証できます。
`EngineTests/RunProjectPathsLayoutRegression.ps1` は、実際のルート解決をゲーム名・絶対パス・単体配置で検証します。
各 `EngineTests/Run*.ps1` はVisual Studioのx64開発者PowerShellから実行します。
テストコードはGit管理し、実行結果・一時プロジェクト・キャッシュはGit管理から除外します。
