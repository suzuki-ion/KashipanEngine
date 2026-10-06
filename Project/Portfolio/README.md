# ポートフォリオの公開と編集

提供されたPDFと2026年9月時点までの追加情報をもとに作成した、鈴木色音の静的ポートフォリオサイトです。

## 公開先

- リポジトリ：suzuki-ion/KashipanEngine
- 公開元ブランチ：JobHuntingProject
- 公開後のURL：https://suzuki-ion.github.io/KashipanEngine/portfolio/
- エンジンリファレンス：https://suzuki-ion.github.io/KashipanEngine/

`.github/workflows/deploy-reference.yml` がリファレンスをルートへ、ポートフォリオを `portfolio/` へまとめて公開します。別のPagesワークフローを追加しないため、片方のサイトがもう片方を上書きすることを避けられます。公開元をJobHuntingProjectに統一し、masterからは公開しません。

## 初回公開

1. このフォルダーと上記ワークフローの変更をJobHuntingProjectへコミット・プッシュします。
2. リポジトリの **Settings → Pages → Build and deployment → Source** が **GitHub Actions** になっていることを確認します。既存リファレンスがこの方式で公開済みなら変更不要です。
3. **Actions → Deploy Reference and Portfolio to GitHub Pages** の実行完了を確認します。手動実行する場合もJobHuntingProjectを選択してください。
4. 公開後のURLを開いて画像とPDFの表示を確認します。

ポートフォリオ、リファレンス、公開ワークフローに変更をプッシュすると自動更新されます。ブランチがリモートより進んでいる場合、通常のプッシュには既存の未送信コミットも含まれます。

GitHub公式：https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site

## 編集

- `index.html`：作品一覧、プロフィール、実績、詳細ページへの導線。
- `works/*/index.html`：9作品それぞれの制作概要、担当内容、工夫。
- `works/job-hunting/index.html`：制作中の就活作品と、プログラム説明資料に基づく技術説明・計測結果・実装状況。技術説明の原本PDFへのリンクもこのページに掲載。
- `engine/index.html`：Kashipan Engine本体の紹介、制作環境、対象範囲、リファレンスへのリンク。
- `style.css`：配色、レイアウト、スマートフォン対応。
- `script.js`：就活作品ページ内のForward / Forward+比較グラフの数値。
- `assets/`：PDFから抽出した作品画像と原本PDF。

リンクは相対パスのため、公開先の `/portfolio/` 配下でも利用できます。画像と原本PDFも公開対象です。開発状況・期間・実績・計測結果は提供資料時点の内容で、現状への更新や再計測は行っていません。

## 手元で確認

このフォルダーの `index.html` をブラウザーで開けます。Webサーバーを使用する場合は `python -m http.server 8765` をこのフォルダーで実行してください。

## ゲームのダウンロード

就活作品を除く8作品の詳細ページに、GitHub ReleasesのZIPへの直接リンクを掲載しています。
配布Release：https://github.com/suzuki-ion/KashipanEngine/releases/tag/portfolio-games-2026-10-05

GitHubが日本語ファイル名を置き換えるため、実際のZIP名は作品名の英字表記を使い、日本語名はReleaseの表示ラベルに設定しています。ZIPの内容は提供された原本から変更していません。

| 作品 | ReleaseのZIP名 |
| --- | --- |
| カケダマ | `Kakedama.zip` |
| 鉱石洞窟 | `KousekiDoukutsu.zip` |
| モノクション | `Monokushon.zip` |
| くるくるレイヤー | `KurukuruLayer.zip` |
| GUNSOLE | `GUNSOLE.zip` |
| 大氷怪鳥ジークアイス | `DaihyouKaichouJikuAisu.zip` |
| CLUBOM | `CLUBOM.zip` |
| グランナー | `Grunner.zip` |

リンクは特定のReleaseタグへ固定しているため、エンジン用ライブラリのReleaseを追加しても配布先は変わりません。ファイルを更新するときは、Releaseのアップロード完了を確認してから各作品ページのURLと容量を変更してください。


## パロディデザイン

全ページを当選広告・警告通知を題材にしたパロディ風に統一しています。

- `style.css`：青いウィンドウ枠、赤・黄色の告知、広告風の作品カードを含む全ページのスタイル。
- `parody.js`：初回の当選ダイアログ、右下に表示する9作品の通知、閉じる操作。
- 当選ダイアログはブラウザーのタブ内のセッションで1回表示します。閉じた状態は`sessionStorage`に保存します。
- 作品通知は最初に8秒後、その後18秒ごとに次の作品を案内します。通知ボタンでも表示できます。
- 通知とダイアログはページ内のHTML要素です。外部ウィンドウやOS通知は使用していません。
- 既存の作品情報・実績・PDF・ゲーム配布先と技術説明は引き継いでいます。
