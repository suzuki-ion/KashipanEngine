// AO用テクスチャ。extraParametersで"gAOTex"というTextureRef型パラメータを設定する
Texture2D gAOTex : register(t21);

// AO: 最終色にAOテクスチャの明るさを乗算する（COMPOSITE_HOOKSの中でMatcapより先、優先度5で呼ぶこと）
void ApplyAO(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.aoIntensity <= 0.0001f) return;
    float ao = gAOTex.Sample(gSampler, uv).r;
    outputColor.rgb *= lerp(1.0f, ao, mat.aoIntensity);
}
