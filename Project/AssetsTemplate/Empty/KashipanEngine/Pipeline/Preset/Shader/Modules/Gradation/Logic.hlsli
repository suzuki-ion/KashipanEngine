// グラデーションカラー: 陰影度(toonFactor)をU座標としてサンプルし、乗算で合成する
// （COMPOSITE_HOOKSの中でMatcapの後、優先度20で呼ぶこと）。テクスチャ本体はgTextures[]
// （バインドレステクスチャ配列）からmat.gradationTextureIndexで参照する
void ApplyGradation(inout float4 outputColor, Material mat, float toonFactor) {
    if (mat.gradationBlend <= 0.0001f) return;
    float3 gradationColor = gTextures[mat.gradationTextureIndex].Sample(gSamplers[mat.samplerIndex], float2(toonFactor, 0.5f)).rgb;
    outputColor.rgb = lerp(outputColor.rgb, outputColor.rgb * gradationColor, mat.gradationBlend);
}
