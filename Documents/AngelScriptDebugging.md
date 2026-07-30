# AngelScriptをVS Codeからデバッグする

KashipanEngineのDebug/Developmentビルドは、ゲーム用`ScriptComponent`を対象とする
技術検証版のDebug Adapter Protocol（DAP）サーバーを内蔵しています。

## 対応機能

- `.as`ファイルに設定した行ブレークポイント
- Continue / Pause
- Step In / Step Over / Step Out
- 停止位置とコールスタックの表示

変数表示、条件付きブレークポイント、ログポイント、EditorToolスクリプトのデバッグは
技術検証版では未対応です。

## 使用手順

1. VS Codeへ`AngelScript Language Server`（`sashi0034.angel-lsp`）をインストールします。
2. VS Codeでこのリポジトリの`Project`フォルダーを開きます。
3. KashipanEngineをDebugまたはDevelopmentビルドで起動し、デバッグ対象のシーンを開きます。
4. VS Codeの「実行とデバッグ」から`Attach to KashipanEngine AngelScript`を開始します。
5. `Assets`以下の`.as`ファイルへブレークポイントを設定し、ゲームを実行します。

DAPサーバーは`localhost:27979`だけで待ち受けます。正常に開始するとエンジンログへ
`AngelScript DAP: localhost:27979 で待受を開始しました`と出力されます。

## 制限事項

- ブレークポイント停止中はAngelScriptを実行しているエンジンのメインスレッドも停止します。
- 同じスクリプトを複数の`ScriptComponent`が使用している場合、ブレークポイントは全インスタンスへ適用されます。
- ReleaseビルドではDAPサーバーを開始しません。
- 別のプロセスが27979番ポートを使用している場合、DAPサーバーは開始されません。
