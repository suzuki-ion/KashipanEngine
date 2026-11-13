# ランタイムのポイントライト・ビルボード一括更新

## 汎用メッシュの一括更新（2026-11-01追加）

```angelscript
bool SetWorldMeshInstances(
    const array<Vector3> &in positions,
    const array<Quaternion> &in rotations,
    const array<Vector3> &in scales,
    const array<Vector4> &in colors);
```

同じMeshRendererのメッシュ・マテリアル・描画先を共有し、個別のObjectやTransformを生成せずに、位置・回転・非一様スケール・色をワールド空間で一括提出する。既存のワールドインスタンス描画経路へ渡す、影なしのランタイム演出用API。呼び出し側で `SetCastShadows(false)` を指定する。

4配列の長さは同一であること。非有限値とゼロ長Quaternionは拒否し、失敗時に前の提出データを変更しない。Quaternionは正規化する。データはエンジン側へコピーし、スクリプト配列のアドレスを保持しない。空の4配列は「バッチとして何も描かない」状態にし、原点へ単体メッシュを出さない。配列内容はシーン保存対象ではない。個別の当たり判定・コンポーネント更新・個体別Object IDは生成しない。

街の歩行者では、住人の論理状態を `TownPedestrians` の配列で保持し、円錐の胴体と球の頭を2つのMeshRendererへ提出する。道路上の移動、施設訪問、出入りの縮小・拡大はゲーム側で計算する。住人の人数や経路ルールをエンジン側へ組み込まない。

大量の演出用ライト／マーカーを、個別のObject・Transform・Rendererを作らず配列で扱う。`MeshRenderer` と `LightRenderer` の既存の登録・描画先・マテリアル・ライト共有経路を利用する。

## MeshRenderer

```angelscript
bool SetWorldBillboardPairs(
    const array<Vector3> &in positions,
    const array<float> &in outerOpacity,
    const Quaternion &in facing,
    float outerSize, float innerSize,
    const Vector4 &in outerColor, const Vector4 &in innerColor);

uint GetWorldInstanceCount() const;
Vector3 GetWorldInstancePosition(uint index) const;
Vector4 GetWorldInstanceColor(uint index) const;
void ClearWorldInstances();
```

- 同じMeshFilter・マテリアルを用い、位置ごとに外側→内側の2インスタンスを生成する。単位はワールド座標。オーナーの移動・回転・拡縮は配列の位置へ追加適用しない。
- 外側の色は `outerColor`、そのアルファは `outerColor.w * outerOpacity[i]`。内側は `innerColor`。既存のInstanceColorBlendMode・UV設定も使用する。
- カメラ向けの回転は `facing` で指定する。両サイズは非負。位置と透明度の長さが違う場合や、位置／透明度／サイズ／回転に非有限値がある場合はfalseを返し、以前のデータを維持する。
- 空配列を渡すとバッチモードのまま0個を描画する。`ClearWorldInstances()` はモードを解除し、通常の単体MeshRendererへ戻す。この2つの操作は意味が異なる。
- バッチモード中はシャドウキャスターにしない。オーナーとRendererの有効状態、描画先、除外、パイプライン、マテリアルによる制御は通常と同じ。
- 描画リストにはバッチ単位のエントリだけを登録。描画直前にインスタンスのデータを再利用するネイティブ配列へ展開し、既存のインスタンシング・マテリアルパック・GPUバッファ処理へ渡す。
- 個々のインスタンスは独立したObjectではなく、個別のTransform・コライダー・スクリプトを持たない。個別操作が必要なゲーム要素は通常のObjectを使う。

## LightRenderer

```angelscript
bool SetPointInstances(
    const array<Vector3> &in positions,
    const array<float> &in intensities);

uint GetPointInstanceCount() const;
Vector3 GetPointInstancePosition(uint index) const;
float GetPointInstanceIntensity(uint index) const;
void ClearPointInstances();
```

- 同じObjectにPointタイプの `Light` を追加する。色・半径・減衰・Lightの有効状態は共通のLightから取得する。位置はワールド座標、強度は配列の絶対値を用い、共通LightのIntensityとは乗算しない。
- 配列の長さが違う場合、位置または強度が非有限の場合、強度が負の場合はfalse。失敗時は以前のデータを維持する。
- 影なしのポイントライト群として、既存のフレーム内ライト収集・ターゲット別絞り込み・GPUライトカリング・描画バッファ共有へ参加する。
- 同じバッチ内の連続要素は、描画先・パイプライン・除外条件をまとめて判定する。別のターゲット／別フレームの判定は使い回さない。
- 空配列は0灯。`ClearPointInstances()` は通常の単体LightRendererへ戻す。Point以外のタイプへ切り替える際も先に解除する。

## データの所有と保存

提出された配列はネイティブ側で所有するデータへコピーする。呼び出し後のスクリプト配列への書き込みや、その配列の寿命終了は、提出済みの描画データへ影響しない。

AngelScriptのオブジェクト型配列は、`CScriptArray::GetBuffer()` を連続した `Vector3` 配列と解釈してはいけない。バインディングでは `At()` で各値へアクセスしてコピーする。プリミティブなfloat配列は呼び出し中だけspanとして参照し、保存先では所有データへコピーしている。

`worldInstancesEnabled` / `pointInstancesEnabled` のモードはJSONへ保存するが、フレームごとのサンプルは保存しない。ロード・復旧後のプロデューサーは再提出する。配列が未提出なら空バッチとして扱い、原点に単体モデル／ライトを誤表示しない。クローン後のプロデューサーも再提出を前提とする。

## JobHuntingGameでの利用

`TownFireflies` は1個の `TownFireflyBatch` に、共通Light・LightRenderer・MeshFilter・MeshRendererを配置する。軌道と明滅は従来通りスクリプトで計算し、次の2呼び出しでまとめて反映する。

```angelscript
batchLights.SetPointInstances(positions, intensities);
batchMarkers.SetWorldBillboardPairs(
    positions, opacities, facing, .4f, .1f,
    Vector4(1, .75f, .12f, 1), Vector4(1, .95f, .5f, .9f));
```

この経路にゲーム固有の軌道式・光の上限・強度調整ルールは含めない。汎用部分はC++、ゲームの動きと調整値はAngelScriptに置く。

計測・検証結果：[最大配置の前後比較](../GameProjects/JobHuntingGame/LightBatchOptimization-20261025.md)。
