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
};