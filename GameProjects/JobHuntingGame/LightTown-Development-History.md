# Light Town 開発履歴とFPS改善

対象は KashipanEngine の `JobHuntingProject` ブランチにある `JobHuntingGame`。街の追加直前から、2026年11月24日の HEAD（`bfb2d871`）までを調べた。日付は、実装経緯にはコミット日、計測には報告書の実施日を使った。10月下旬〜11月1日の作業は、11月13日の複数コミットにまとめて収録されている。

## 1. 横スクロールアクションから街づくりへ

もともとは `Stage1` が起動シーンの横スクロールアクションで、移動・ジャンプ・変身、敵、チェックポイント、ゴール、移動床などがあった。街づくりの追加後は、既存プレイヤーやステージギミックを遠征で再利用し、**横スクロールで素材を持ち帰り、街で道路・街灯・施設を整え、最終的に「光の塔」を稼働させる**流れになった。遠征内の帰還地点は「光の門」、街づくり全体のゴールは「光の塔」。

| 時期 | 追加・変更された内容 | 根拠 |
| --- | --- | --- |
| 10月18日 | `CityScene` と AngelScript の `GridGenerator` を追加。街の配置基盤を作り、マス数・間隔・線幅などを指定してグリッド線を生成できるようにした。 | [街シーンの追加](https://github.com/suzuki-ion/KashipanEngine/commit/783f34f3d) |
| 10月23日 | タイトルに新規・続き・設定・終了を追加。街の資源・各種指標・施設の配置／稼働状態を共通の進行データで管理し、街と遠征を往復しても保持する構成にした。 | [進行・セーブ・タイトル](https://github.com/suzuki-ion/KashipanEngine/commit/cda23dce3) |
| 10月23日 | 接続した部屋を探索する `ExpeditionScene` を追加。木材・石材・光結晶のノードをツルハシで攻撃し、HPが0になると回収・保存する。通常形態を人型のスキニングモデルと1×2×1mのBoxColliderに変更。変身はコードを残し、当時の試作では入力・購入を停止した。 | [遠征・採集](https://github.com/suzuki-ion/KashipanEngine/commit/19d810e9f) |
| 10月23日 | 16×16の街で、道路・街灯・住宅・工房・ショップ・ギルドを配置。施設の稼働を道路と街灯の照明に結び付け、施設表示、NPC派遣、浮遊光、光の塔とCLEAR画面を追加。当初は街の基礎収入と工房でも原材料を増やせた。 | [建築とクリア](https://github.com/suzuki-ion/KashipanEngine/commit/a35f2836c)・[当時の仕様](https://github.com/suzuki-ion/KashipanEngine/blob/36dd0a855/GameProjects/JobHuntingGame/README.md) |
| 10月25日ごろ | 道路の16接続パターンに応じてモデルを切り替え。施設は入口正面の道路を使うルールに変更し、道路への自動方向合わせ、Rキーの90度回転、Shift＋ドラッグの連続建築を追加。住民が道路をたどって施設を訪ね、住居へ戻るようにした。 | [施設の向きと連続建築](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/TownConstruction-20261025.md)・[道路と人口](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/TownRoadDevelopment-20261025.md) |
| 10月27日ごろ | 建築を8カテゴリ・22候補に拡張し、施設の規模、小・中規模の1×1／大規模の2×2の占有、3種類の加工品、警察署、工場周辺への影響を追加。初期分以外の原材料は遠征採掘かNPC派遣で入手する仕様へ変更し、工場は原材料と光を使って加工品を生産する役割に変更。採掘報酬は木材4→16、石材4→16、光結晶3→12。 | [街と遠征の拡張](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/TownExpansion-20261027.md)・[施設拡張コミット](https://github.com/suzuki-ion/KashipanEngine/commit/cdf5a439e) |
| 10月下旬〜11月13日収録 | 森・採石場・洞窟から行き先を選べるようにし、テーマ別の地形・装飾と素材の偏りを追加。通常64部屋と特殊部屋4室の構成に拡張し、敵から得る鍵で素材庫を開ける仕掛けを追加。街・遠征へのロード中に専用シーンを表示し、旧アクションシーンも再登録。プレイヤーの段差・床の継ぎ目・狭所での衝突処理を改善した。 | [遠征先とロード](https://github.com/suzuki-ion/KashipanEngine/commit/0ceb649e7)・[ロード画面の検証](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/Docs/JobHuntingLoadingScene.md)・[旧ステージと衝突処理](https://github.com/suzuki-ion/KashipanEngine/commit/ac8f9c474) |

### 現行ルールで光の塔まで進む流れ

1. **素材を入手する。** 初期の木材140・石材110・光結晶35で開拓を始める。遠征で資材を採掘し、光の門などから街へ戻る。街でギルドを稼働させれば、NPC派遣でも原材料を得られる。
2. **稼働する街を作る。** 道路を敷き、道路を正面に向いた街灯で照明を確保し、施設を稼働させる。光エネルギー・幸福度・治安・繁栄度を管理する。加工工場の加工木材・加工石材・加工光結晶を中・大規模施設に使える。
3. **光の塔を建築して稼働させる。** 建築時点で稼働施設8棟以上（道路を除く）、光エネルギー80以上、繁栄度60以上、木材180・石材140・光結晶65が必要。塔自身も入口正面の道路と照明で稼働させる。条件を満たすとCLEAR画面が出て、続行か保存して終了を選べる。

根拠：[ルール定義](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/Assets/Application/Data/LightTownRules.json)・[稼働／クリア判定](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownState.as)・[クリア画面](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/Assets/Application/Scripts/LightTown/TownClearMenu.as)

### 旧アクションの計測記録（10月21日）

街シーンの初回追加（10月18日）と、街・遠征・クリアの統合（10月23日）の間に、旧アクションの計測が残っている。ローカルの各集計JSONは10月21日更新。各条件75サンプルのRelease実行で、表は記録されたFPSの中央値。旧ステージでも影設定への感度はシーンによって異なった。

| 旧シーン | 基準設定 | 影解像度512 | 影なし |
| --- | ---: | ---: | ---: |
| Stage1 | 123.66 FPS | 225.93 FPS | 240.00 FPS |
| GameScene | 87.85 FPS | 107.89 FPS | 135.42 FPS |

Stage1とGameSceneの基準設定でのフレーム時間中央値はそれぞれ8.08 ms、11.37 ms。後述の街の8.79 FPSは全セルを使用した別シーンの区間平均であり、街の追加による低下率はこの組み合わせから算出できない。ローカル原本は [Stage1の基準値](../../Generated/FpsInvestigation/Stage1-Release-baseline.summary.json)・[GameSceneの基準値](../../Generated/FpsInvestigation/GameScene-Release-baseline.summary.json)・[計測スクリプト](../../Generated/FpsInvestigation/measure.py)。

## 2. FPS低下の発見と切り分け

街の追加前後で、同じ負荷・画質・計測方法をそろえたFPS比較は確認できなかった。街の性能を最初に数値で追えるのは**10月23日の街シーンの計測**。浮遊光や施設を増やした全セル配置では、通常更新で約8.8 FPS（約114 ms/フレーム）まで低下した。

| 当時の調査 | 結果と読み取り |
| --- | --- |
| 10月23日・機能を個別停止 | 別の街配置で通常27.6 FPS、浮遊光のライトだけ停止29.5 FPS、ライト・マーカー描画・更新をまとめて停止50.2 FPS。浮遊光に付随する更新と描画準備まで調べる必要があった。 |
| 10月23日・全セル配置で更新を停止 | 通常8.79 FPS、全スクリプト停止14.55 FPS、さらに復旧保存を止めると18.47 FPS、シーン更新も止めた状態で19.87 FPS。更新と復旧保存を外した状態にも、描画準備などの負荷が残った。 |
| 10月23日・CPU詳細とPIX | 別の詳細計測で通常8.38 FPS。CPU側ではマテリアル組み立て19.86 ms、ライト収集9.72 ms、スクリプト内では浮遊光20.80 msや施設バー11.36 msが目立った。静止状態のPIX再生ではGPU合計の中央値14.81 msで、影約4.77 ms、ライトカリング約3.77 msが大きかった。 |
| 10月25日・改修途中の街を再計測 | 浮遊光336個・建物14件ではPIX GPU合計2.340 ms（他のGPU使用アプリを終了した後）。被写界深度（DOF）が1.155 msで最大。浮遊光2,046個の全セル配置では、GPU合計4.943 msのうちシーン描画2.626 ms、DOF 1.157 ms。全セル配置の実行時は約35 FPSで、CPU側の更新・描画準備も大きかった。 |

根拠：[初期の切り分けと前後比較](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/CityPerformanceInvestigation.md)・[CPU詳細・PIX](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/CityDetailedPerformance.md)・[10月25日のGPU再計測](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/CityGpuInvestigation-20261025.md)・[10月25日の全セル配置](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/FullCityPerformance-20261025.md)

## 3. 対処と前後比較

以下は**各行の同条件比較**。行をまたぐと、街の配置・浮遊光の数や半径・住民数・カメラなどが異なる。FPSの差には、その行に記した複数の変更が含まれる場合がある。

| 報告日 | 対処 | 平均FPS（前→後） | その比較の条件 |
| --- | --- | ---: | --- |
| 10月23日 | 階層を含む有効状態をキャッシュ。更新対象コンポーネントの一覧、描画リスト、フレーム内のライト収集結果を再利用。 | **24.29 → 44.98** | 浮遊光360、マーカー720。更新16.86→7.73 ms、描画リスト構築7.08→1.66 ms。 |
| 10月24日 | マテリアル組み立て済みデータを再利用。ライト収集の共有範囲を広げ、クラッシュ復旧スナップショットの処理を複数フレームに分割。 | **9.35 → 16.14** | 全256セル、浮遊光2,046。 |
| 10月24日 | 街スクリプトでセル単位の施設検索をキャッシュし、JSONの反復複製や同じオブジェクトの再探索を削減。浮遊光の参照や施設バーの行・表示を再利用。 | **14.90 → 25.56** | 全セル配置。ネイティブ実行ファイルをそろえ、街スクリプトを切り替えて比較。 |
| 10月24日 | GPUライトカリングでライトの画面範囲計算をスレッド群内で共有。影をキャッシュし、描画リストの変更箇所を差分更新。 | **26.73 → 32.82** | 全セル配置、浮遊光2,046。同じ比較の前後に先行の最適化を含む。 |
| 10月25日 | 浮遊光ごとのライト・ハロー・コアを1つの管理オブジェクトと配列に集約。ライト2,046個とマーカー4,092枚を一括更新・描画。 | **35.02 → 86.08** | 全256セル、住民48人、光の半径2.2 m。CPUの更新＋描画準備18.92→7.55 ms、PIX GPU合計4.943→4.949 ms。 |
| 10月27日 | 建築UIの全行を毎フレーム有効／無効にする処理を解消。開閉・選択・スクロール時などにレイアウトを更新し、見えている回転モデルだけを更新。 | **41.3〜44.9 → 232.8〜236.6** | 空の街の3カテゴリ。UIを閉じた基準は235.6→236.6 FPS。 |
| 11月1日 | 歩行者557人の個別オブジェクト1,671個を身体・頭の2描画バッチに集約。配列を再利用し、住居中心・調整済み経路の計算も共有。 | **27.34 → 54.26** | 全256セル、稼働施設140棟、浮遊光2,048、半径4.5 m、街全体を収める固定視点。 |

根拠：[最適化前後の総表](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/PerformanceComparisonSummary-20261101.md)・[レンダラーと復旧保存](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/Docs/RendererAndRecoveryOptimizations.md)・[街スクリプト](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/CityScriptOptimizations.md)・[ライトカリングと影](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/Docs/LightShadowDrawListOptimizations.md)・[浮遊光バッチ](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/LightBatchOptimization-20261025.md)・[建築UI](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/BuildMenuPerformance-20261027.md)・[歩行者バッチ](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/PedestrianBatchOptimization-20261101.md)

### 遠征ステージの低下と対処

生成ステージの比較では、形状が変わらない3Dコライダーを再利用し、同じ親の再設定で描画キャッシュを破棄しないように変更した。採集時の遮蔽判定は固定床の全走査から8m単位の近傍検索へ変更。影は関係するライトジョブだけ再描画できるようにした。

| テーマ | 変更前（2回の範囲） | 変更後 |
| --- | ---: | ---: |
| 森 | 57.7〜61.3 FPS | 73.7 FPS |
| 採石場 | 32.7〜37.2 FPS | 55.3 FPS |
| 洞窟 | 34.8〜36.0 FPS | 61.3 FPS |

各テーマの同一シード、通常64部屋＋特殊部屋4室、資材ノード86個で比較。採石場では衝突処理3.47→1.27 ms、描画リスト3.80→1.07 ms、スクリプト2.94→0.39 ms。影のCPU時間は4.28→4.28 msで、変更後も採石場は60 FPSを下回った。遠征比較の報告書は10月25日付で、衝突再利用のエンジン差分は11月13日のコミットに収録されている。

根拠：[遠征の前後比較と検証](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/Docs/ActionStagePerformanceOptimizations.md)・[コライダー再利用](https://github.com/suzuki-ion/KashipanEngine/commit/50365d0ff)

## 4. 最後に確認できる負荷試験の結果

- **全セル・歩行者557人の街。** 11月1日の初回固定視点試験は平均26.97 FPS。その後、同系統の配置で歩行者のバッチ化を比較し、27.34→54.26 FPSまで改善した。10月25日の86.08 FPSは住民48人・光半径2.2 mの試験で、11月1日は住民557人・光半径4.5 mと、配置と描画負荷が異なる。
- **Forward+の効果。** 11月1日の同一配置・同一カメラの比較では、街を画面に大きく収めた視点で、Forward+ **37.45 FPS**、タイルカリングを外した通常Forward **18.34 FPS**（約2.04倍）。街を小さく映す固定視点でもForward+は53.83 FPSだった。低い方の37.45 FPSは画角を変えた別条件の値で、バッチ化直後の54.26 FPSと直接比較できない。
- **CPU時間とGPU時間。** PIXは取得済みフレームのGPU再生計測。CPU側の送信・Present・Fence待ちは別の指標で、当時の描画ループは毎フレームGPU完了を待つ。両者の加算や最大値だけで実行時FPSを求めることはできない。影の再生成がないフレームを使ったPIX計測もある。
- **確認範囲。** 主要な後期比較は Ryzen 7 7745HX／RTX 4070 Laptop GPU、Release、3D描画1920×1080、影512・ポストエフェクト有効。試験ごとに配置、ウィンドウの表示状態、カメラが異なる。この資料ではGit履歴・現行スクリプト・既存の計測記録を照合した。HEADの実機再計測や、街づくり追加前のアクションと同条件での比較は行っていない。

根拠：[11月1日の全セル配置](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/FullCityPerformance-20261101.md)・[大きな画角でのForward比較](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/ForwardFullScreenComparison-20261101.md)・[CPU・GPU同期の考察](https://github.com/suzuki-ion/KashipanEngine/blob/bfb2d871/GameProjects/JobHuntingGame/FullCityPerformance-20261025.md)

## 5. ローカル記録で確認できた開発後半の経過

| 日付 | 記録と確認できたこと |
| --- | --- |
| 10月25日 | 実際の街のセーブを使い、生産やイベントも進めた試験では、光336・建物14件で96.45 FPS、別セーブの光390・建物15件で95.58 FPSだった。光336・建物14件で生産とイベントの時計を固定した比較では、通常101.25〜101.56 FPS、浮遊光の更新だけを止めると121.93 FPS。スクリプト更新の負荷が通常規模の街でも現れた。[調査記録](CityPerformanceInvestigation-20261025.md) |
| 10月25日 | 浮遊光の一括処理の集計JSONには、検証用配置が街灯80、稼働施設160、歩行者48、光2,046、マーカー4,092と記録されている。CPU更新＋描画準備は約60.1%短縮し、GPU再生時間は前後とも約4.95 ms。表示数を保ってCPU側の処理を減らした効果を確認できる。[比較集計](../../Generated/LightBatchOptimization/Summary.json) |
| 11月1日 | 歩行者バッチ化の比較JSONに残る平均は27.34355 FPSと54.25931 FPS。全セルを使っても、街の人口が増えた段階では歩行者の更新・描画準備が次の改善対象になった。[比較集計](../../Generated/PedestrianBatch-20261101/Comparison.json) |
| 11月10日 | 入力検証でマウスを動かしながらキーを保持。OSが60回の観測すべてで押下中と判定したのに対し、修正前のエンジンは保持0回、修正後は60回、押下・解放も各1回検出した。建築操作の検証では、Shift＋ドラッグで途中セルを含めて連続撤去し、派遣中のギルドを保護するケースも通過した。[修正前ログ](../../Generated/KeyboardMouseContinuity-20261110/Before.log)・[修正後ログ](../../Generated/KeyboardMouseContinuity-20261110/After.log)・[操作テスト](../../EngineTests/TownPaintGestureRuntime.as)・[結果](../../Generated/CityPaintToolsQA-20261110/Results/PaintGesture.json) |
| 11月13日 | 描画バッチ・影・コライダーの改善、施設拡張、遠征先選択、旧ステージ復元をコミット。別途、Releaseアセットの事前変換、非同期ロード・分割復旧保存、Direct2D起動画面も収録した。[描画改修](https://github.com/suzuki-ion/KashipanEngine/commit/21588a9d)・[非同期ロード](https://github.com/suzuki-ion/KashipanEngine/commit/9e7f65e5)・[性能記録の収録](https://github.com/suzuki-ion/KashipanEngine/commit/b761fa8e) |
| 11月24日 | アセット管理の共通化、CookedAssetsの寿命と参照の制限、ProfilerのAPI・生成の制限を実施した。この日のコミットに対応する街のFPS前後比較は確認できなかった。[アセット管理](https://github.com/suzuki-ion/KashipanEngine/commit/bf695ccf)・[CookedAssetsとProfiler](https://github.com/suzuki-ion/KashipanEngine/commit/bfb2d871) |

Generated配下のリンクは調査に使用したローカル作業環境の記録を指す。GitHubから参照できる計測報告書と実装コミットは各節に併記した。
