// AO: 最終色にAOテクスチャの明るさを乗算する（COMPOSITE_HOOKSの中でMatcapより先、優先度5で呼ぶこと）。
// テクスチャ本体はgTextures[]（バインドレステクスチャ配列）からmat.aoTextureIndexで参照する
void ApplyAO(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.aoIntensity <= 0.0001f) return;
    float ao = gTextures[mat.aoTextureIndex].Sample(gSamplers[mat.samplerIndex], uv).r;
    outputColor.rgb *= lerp(1.0f, ao, mat.aoIntensity);
}
