// 発光用テクスチャ。extraParametersで"gEmissionTex"というTextureRef型パラメータを設定する
Texture2D gEmissionTex : register(t17);

// 発光: ライティング非依存で常時加算する（COMPOSITE_HOOKSの中でGradationの後、優先度30で呼ぶこと）
void ApplyEmission(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.emissionIntensity <= 0.0f) return;
    float3 emission = gEmissionTex.Sample(gSampler, uv).rgb * mat.emissionColor.rgb * mat.emissionIntensity;
    outputColor.rgb += emission;
}
