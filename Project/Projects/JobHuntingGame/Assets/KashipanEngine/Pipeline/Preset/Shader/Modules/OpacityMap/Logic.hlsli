// オパシティマップ: アルファへ乗算する（ALPHA_HOOKSの中でDistanceFadeより先、優先度5で呼ぶこと）。
// テクスチャ本体はgTextures[]（バインドレステクスチャ配列）からmat.opacityTextureIndexで参照する
// （extraParametersで"opacityTextureIndex"というTextureRef型パラメータを設定する）
void ApplyOpacityMap(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.opacityMapIntensity <= 0.0001f) return;
    float opacity = gTextures[mat.opacityTextureIndex].Sample(gSamplers[mat.samplerIndex], uv).r;
    outputColor.a *= opacity;
}
