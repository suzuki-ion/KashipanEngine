// 発光: ライティング非依存で常時加算する（COMPOSITE_HOOKSの中でGradationの後、優先度30で呼ぶこと）。
// テクスチャ本体はgTextures[]（バインドレステクスチャ配列）からmat.emissionTextureIndexで参照する
void ApplyEmission(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.emissionIntensity <= 0.0f) return;
    float3 emission = gTextures[mat.emissionTextureIndex].Sample(gSamplers[mat.samplerIndex], uv).rgb * mat.emissionColor.rgb * mat.emissionIntensity;
    outputColor.rgb += emission;
}
