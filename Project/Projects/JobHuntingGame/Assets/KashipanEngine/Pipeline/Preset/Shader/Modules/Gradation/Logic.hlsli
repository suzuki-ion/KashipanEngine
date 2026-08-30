// グラデーションカラー用テクスチャ。extraParametersで"gGradationTex"というTextureRef型パラメータを設定する
Texture2D gGradationTex : register(t16);

// グラデーションカラー: 陰影度(toonFactor)をU座標としてサンプルし、乗算で合成する
// （COMPOSITE_HOOKSの中でMatcapの後、優先度20で呼ぶこと）
void ApplyGradation(inout float4 outputColor, Material mat, float toonFactor) {
    if (mat.gradationBlend <= 0.0001f) return;
    float3 gradationColor = gGradationTex.Sample(gSamplers[mat.samplerIndex], float2(toonFactor, 0.5f)).rgb;
    outputColor.rgb = lerp(outputColor.rgb, outputColor.rgb * gradationColor, mat.gradationBlend);
}
