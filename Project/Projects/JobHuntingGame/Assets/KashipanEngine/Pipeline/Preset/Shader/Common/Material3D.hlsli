// カスタムシェーダー向けの追加パラメータは、この構造体の末尾に自由に追加してよい。
// フィールド名が MaterialManager::Material::extraParameters（.matの"parameters"）のキーと一致していれば、
// C++側の対応コード変更なしに値が自動的にここへ渡る（対応する型は float/float2/float3/float4/float4x4/int/uint/bool）。
struct Material {
    float enableLighting;
    float enableEnvironmentMapping;
    float enableShadowMapProjection;
    float useTexture;
    float4 color;
    float4x4 uvTransform;
    float shininess;
    float4 specularColor;
    float environmentCoefficient;
    // リムライト（ライト方向を考慮した逆光縁取り）。rimIntensity=0（既定）の場合は無効
    float4 rimColor;
    float rimPower;
    float rimIntensity;
    // 法線マップ使用フラグ（0=未使用）。gNormalMapはRGBが接空間の法線[0,1]エンコード
    float useNormalMap;
    // オブジェクト単位の色（MeshRendererのInstance Color）。マテリアル本体（共有アセット）とは別に、
    // インスタンス（描画エントリ）ごとに異なる値を持つ
    float4 instanceColor;
    // instanceColorの適用方法。0=Override（置き換え）, 1=Multiply（乗算）, 2=Add（加算）, 3=Subtract（減算）
    float instanceColorBlendMode;

    // 色調整（最終色にHSV/ガンマ補正を適用）。saturation/brightness/gammaは1.0を基準とした差分値（既定0で無変化）
    float hueShift; // @Range(-180, 180, 1)
    float saturation; // @Range(-1, 1, 0.01)
    float brightness; // @Range(-1, 1, 0.01)
    float gamma; // @Range(-0.9, 2, 0.01)

    // 多段影（ObjectToonパイプラインでのみ有効）。toneCount<2.5で2階調、それ以外で3階調
    float toneCount; // @Range(2, 3, 1)
    float shadowThreshold; // @Range(0, 1, 0.01)
    float midThreshold; // @Range(0, 1, 0.01)
    float bandSoftness; // @Range(0, 0.2, 0.01)

    // バックライト（光源が背後にあるときに縁を光らせる）。backlightIntensity=0（既定）で無効
    float4 backlightColor; // @Color
    float backlightPower; // @Range(1, 20, 0.5)
    float backlightIntensity; // @Range(0, 5, 0.01)

    // リムシェード（影側のリムライトを別色にする）。rimShadeBlend=0（既定）で無効
    float4 rimShadeColor; // @Color
    float rimShadeBlend; // @Range(0, 1, 0.01)

    // ディゾルブ（ノイズに基づく消失＋エッジカラー）。dissolveThreshold=0（既定）で無効
    float dissolveThreshold; // @Range(0, 1, 0.01)
    float4 dissolveEdgeColor; // @Color
    float dissolveEdgeWidth; // @Range(0, 0.5, 0.01)
    float dissolveNoiseScale; // @Range(0.1, 50, 0.1)

    // 距離フェード（カメラからの距離でアルファを減衰）。fadeEndDistance<=fadeStartDistance（既定0,0）で無効
    float fadeStartDistance; // @Range(0, 100, 1)
    float fadeEndDistance; // @Range(0, 200, 1)

    // マットキャップ・2ndマットキャップ（ビュー空間法線からサンプルする後乗せレイヤー）。
    // matcap(2nd)Intensity=0（既定）で無効。gMatcapTex/gMatcap2ndTexはextraParametersでTextureRef型として設定する。
    // blendModeは0=Add/1=Multiply/2=Screen/3=Replace
    float matcapIntensity; // @Range(0, 2, 0.01)
    float matcapBlendMode; // @Range(0, 3, 1)
    float matcap2ndIntensity; // @Range(0, 2, 0.01)
    float matcap2ndBlendMode; // @Range(0, 3, 1)

    // グラデーションカラー（陰影度をU座標としてgGradationTexをサンプルし、乗算で合成する）。
    // gradationBlend=0（既定）で無効。gGradationTexはextraParametersでTextureRef型として設定する
    float gradationBlend; // @Range(0, 1, 0.01)

    // 発光（ライティング非依存で常時加算）。emissionIntensity=0（既定）で無効。
    // gEmissionTexはextraParametersでTextureRef型として設定する
    float4 emissionColor; // @Color
    float emissionIntensity; // @Range(0, 5, 0.01)
};