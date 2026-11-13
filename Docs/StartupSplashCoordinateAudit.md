# 起動スプラッシュの座標系比較

> この文書は座標系を修正する前の調査記録です。後続の改修で1920x1080・左下原点へ統一し、
> モニターに応じた拡縮に対応しました。現在の仕様は [StartupSplash.md](StartupSplash.md) を参照してください。

確認日: 2026-10-28。元ブランチ: `origin/TD2_3`、コミット
`1320133765963d22ffd285156e3753f6da3b91e0`。
この調査ではエンジンの実装を変更していません。
元ブランチの参照ファイルを `Generated/StartupSplashCoordinateAudit/TD2_3/` に抽出して比較しました。

## 結論

一致していません。元の実装は2D世界座標のYが上向きで、現スプラッシュは左上原点・Yが下向きです。
文字ロゴと帯の上下位置、および正の角度による回転方向が逆です。
さらに、元の描画先は1920x1080で、現実装の「1280x720を基準に半分に縮小する」という前提が誤っています。

| 比較対象 | TD2_3 | 現在のスプラッシュ | 判定 |
|---|---|---|---|
| 2D座標の原点 | 投影範囲の左下 | 描画領域の左上 | 不一致 |
| Xの正方向 | 右 | 右 | 一致 |
| Yの正方向 | 上 | 下 | 不一致 |
| 正のZ回転の画面上の方向 | 反時計回り | 時計回り | 不一致 |
| ロゴ画像の基準点 | 中央アンカー | 中央を基準にDrawImage | 一致 |
| 角度の単位 | ラジアン（度から変換） | 度 | 単位変換は成立するが符号が不一致 |
| ロゴの描画先 | 1920x1080のScreenBuffer | 1280x720相当を0.5倍して640x360へ描画 | 不一致 |

## 元の投影・回転・画像の向き

[正射影行列とビューポート行列](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Utilities/MathUtils/Matrix4x4.cpp:216) は、
`clipY = 2*y/H - 1`、`pixelY = H*(1-clipY)/2` となるため、合成すると `pixelY = H-y` です。
従って世界座標のY=0は描画先の下辺、Y=Hは上辺へ対応します。
[Camera2Dの設定](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Scene/Components/SceneDefaultVariables.cpp:26) は
範囲(0,0,w,h)をこの投影へ渡しています。
[Z回転行列](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Utilities/MathUtils/Matrix4x4.cpp:176) は右方向ベクトルを
+90度で世界座標の上方向へ回し、投影後は画面の上方向になります。
[現スプラッシュの変換](C:/Users/ionnn/source/repos/KashipanEngine/SourceFiles/KashipanEngine/Splash/SplashScreen.cpp:40) はGDI+の
TranslateTransformとRotateTransformを使用します。実際のGDI+行列APIによる数値検証で、
Scale(0.5,0.5)、Translate(640,360)、Rotate(+90)の後、中心(0,0)は(320,180)、
右方向の点(1,0)は(320,180.5)になり、画面の下へ回ることを確認しました。
検証ソースは `Generated/StartupSplashCoordinateAudit/RotationProbe.cpp` です。

[Spriteの頂点とUV](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Objects/GameObjects/2D/Sprite.cpp:44) は、
世界座標で上側の頂点にUVのV=0、下側にV=1を割り当てています。
そのため、元のPNG自体を上下反転して見せる実装ではありません。
一致させる際は、画像全体を単純に上下反転するのではなく、中心座標のY変換と回転符号の変換が必要です。

## 元の描画サイズと実配置

[AppInitialize](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/Application/AppInitialize.h:18) のウィンドウ作成指定は1920x1280です。
EngineLogoScene中の1280x720はウィンドウを取得できない場合のフォールバックであり、通常の基準ではありません。
実際にはウィンドウのGetClientWidth/GetClientHeightを使用します。
[SceneDefaultVariables](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Scene/Components/SceneDefaultVariables.cpp:13) は
2D/3DのScreenBufferを1920x1080で作成しています。ウィンドウとバッファの高さも異なります。
[元のロゴ位置](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/Application/Scenes/EngineLogoScene.cpp:78) は、歯車・パンを(W/2,H/2)、
文字・帯を(W/2,H*0.2)に置きます。元コメントの「height*2/3」「height*1/3」とも異なるため、式を根拠にしています。

次の表は、作成指定どおりウィンドウのクライアント領域が1920x1280である場合に、
元の1920x1080バッファ画像だけを640x360へ縮小して比較した数値です。
元ブランチを実起動して測ったウィンドウサイズではありません。
全項目とも左上原点のピクセル座標へ変換しています。

| 要素（演出の拡大完了後） | 元のバッファを640x360へ縮小 | 現スプラッシュ |
|---|---:|---:|
| 歯車・パンの中心 | (320,146.67) | (320,180) |
| 歯車・パンのサイズ | 170.67x170.67 | 256x256 |
| 文字ロゴの中心 | (320,274.67) | (320,72) |
| 文字ロゴのサイズ | 341.33x85.33 | 512x128 |
| 帯の中心Y・高さ | Y=274.67、高さ85.33 | Y=72、高さ128 |

計算は `x'=x/3`、`y'=(1080-y)/3`、`size'=size/3`。
現在の文字は上側ですが、元は歯車・パンより下側です。
現在のロゴサイズは、この比較では元の1.5倍になっています。

[画面合成時のアスペクト比維持](C:/Users/ionnn/source/repos/KashipanEngine/Generated/StartupSplashCoordinateAudit/TD2_3/KashipanEngine/Scene/Components/ScreenBufferKeepRatio.h:39) も追跡しました。
元のバッファは1920x1280のウィンドウへ1920x1080のまま中央配置される計算となり、上下に100ずつ余白が生じます。
従って元の最終ウィンドウでの中心は、歯車・パン(960,540)、文字(960,924)です。
その最終画面全体を縦横比を保って640x360へ収めるなら、倍率0.28125・左右余白50となり、
中心は歯車・パン(320,151.875)、文字(320,259.875)、サイズは歯車144、文字288x72になります。
この「最終ウィンドウ全体の縮小」と上表の「バッファだけの縮小」は別の比較です。
実行時のウィンドウサイズ変更・OSによるサイズ調整がある場合、元側の位置も変わります。

## 確認範囲

- ブランチ上のソース、投影・回転の数式、最終画面への合成処理を確認。
- GDI+の実際の行列変換APIを独立プログラムで検証し、正の角度が画面下方向へ回ることを確認。
- 元ブランチの実行画像と現スプラッシュの目視比較は未実施。
- この調査ではコード修正・エンジンの再ビルドは行っていません。
