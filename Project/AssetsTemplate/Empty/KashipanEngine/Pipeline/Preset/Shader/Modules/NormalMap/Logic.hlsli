// ノーマルマップ用テクスチャ。extraParametersで"gNormalMap"というTextureRef型パラメータを
// 設定すると、描画時に自動でここへバインドされる（RendererInternal::BindExtraTextureParameters参照）
Texture2D gNormalMap : register(t13);

// 法線マップ: 接空間（TBN）で摂動した法線を返す。mat.useNormalMap<=0.5なら無変化のshadingNormalを返す
// （PRELIGHTING_HOOKSの中で優先度10、ライティング計算より前に呼ぶこと）
float3 ApplyNormalMap(float3 shadingNormal, Material mat, float3 tangent, float2 uv) {
    if (mat.useNormalMap <= 0.5f) return shadingNormal;
    float3 t = normalize(tangent - shadingNormal * dot(shadingNormal, tangent)); // Gram-Schmidtで再直交化
    float3 b = cross(shadingNormal, t);
    float3x3 tbn = float3x3(t, b, shadingNormal);
    float3 sampledNormal = gNormalMap.Sample(gSamplers[mat.samplerIndex], uv).rgb * 2.0f - 1.0f;
    return normalize(mul(sampledNormal, tbn));
}
