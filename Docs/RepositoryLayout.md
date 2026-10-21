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
