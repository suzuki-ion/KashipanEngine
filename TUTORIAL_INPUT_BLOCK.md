# チュートリアル入力ブロック機能

## 概要
チュートリアル開始時の確認ボタン入力で誤って爆弾が設置されてしまう問題を解決しました。

## 実装内容

### 1. TutorialManager
- **入力ブロック機能を追加**
  - `isInputBlocked_`: 入力ブロック中かどうかのフラグ
  - `inputBlockTimer_`: 入力ブロックタイマー
  - `inputBlockDuration_`: 入力を無効にする時間（デフォルト: 0.3秒）
  - `CanAcceptInput()`: BombManagerとPlayerMoveが入力を受け付けられる状態かを取得

- **確認ボタン押下時の処理**
  - `WaitForConfirm`フェーズで確認ボタンを押した時、`isInputBlocked_`を`true`に設定
  - `inputBlockDuration_`の間、入力がブロックされる
  - タイマーが終了すると自動的に入力ブロックが解除される

### 2. BombManager
- **TutorialManagerへの参照を追加**
  - `SetTutorialManager(TutorialManager* tutorialManager)`: TutorialManagerを設定
  - `tutorialManager_`: TutorialManagerへのポインタ

- **爆弾設置時の入力チェック**
  - `Update()`メソッド内で、爆弾設置入力をチェックする前に`TutorialManager::CanAcceptInput()`を確認
  - 入力がブロックされている場合は爆弾を設置しない

## 使い方

チュートリアルシーンで以下のように設定してください：

```cpp
// TutorialManagerの作成
auto tutorialManager = std::make_unique<TutorialManager>(
    inputCommand,
    gameTargetPos, gameTargetRot,
    menuTargetPos, menuTargetRot
);

// BombManagerにTutorialManagerを設定
bombManager_->SetTutorialManager(tutorialManager.get());

// 他の設定...
tutorialManager->SetPlayerMove(playerMove);
tutorialManager->SetBombManager(bombManager_);
tutorialManager->SetExplosionManager(explosionManager_);
tutorialManager->SetPlayer(player_);
```

## 動作フロー

1. プレイヤーがモニターの説明を読む（`WaitForConfirm`フェーズ）
2. 確認ボタン（Submit）を押す
3. **入力ブロックが開始される（0.3秒間）**
4. カメラがステージに向く
5. チュートリアル実践開始（`Practicing`フェーズ）
6. **入力ブロックが自動解除される**
7. プレイヤーが爆弾を設置できるようになる

これにより、確認ボタンと爆弾設置ボタンが同じキー（例: スペースキー）でも、誤入力が発生しません。

## パラメータ調整

入力ブロックの時間を調整したい場合は、`TutorialManager.h`の以下の値を変更してください：

```cpp
float inputBlockDuration_ = 0.3f;  // 入力を無効にする時間（秒）
```
