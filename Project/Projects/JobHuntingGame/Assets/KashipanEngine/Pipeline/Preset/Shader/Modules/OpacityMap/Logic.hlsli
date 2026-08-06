// オパシティマップ用テクスチャ。extraParametersで"gOpacityTex"というTextureRef型パラメータを設定する
Texture2D gOpacityTex : register(t19);

// オパシティマップ: アルファへ乗算する（ALPHA_HOOKSの中でDistanceFadeより先、優先度5で呼ぶこと）
void ApplyOpacityMap(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.opacityMapIntensity <= 0.0001f) return;
    float opacity = gOpacityTex.Sample(gSampler, uv).r;
    outputColor.a *= opacity;
}
