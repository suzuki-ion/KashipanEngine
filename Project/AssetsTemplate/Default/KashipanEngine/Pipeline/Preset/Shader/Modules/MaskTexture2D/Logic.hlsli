// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// マスクテクスチャ用テクスチャ。extraParametersで"gMaskTex"というTextureRef型パラメータを設定する
Texture2D gMaskTex : register(t1);

// マスクテクスチャ: gMaskTexの赤成分をアルファへmaskIntensityでlerp乗算する
// （ALPHA_HOOKS_2Dの中でDissolve2Dより先、優先度5で呼ぶこと）
void ApplyMaskTexture2D(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.maskIntensity <= 0.0001f) return;
    float mask = gMaskTex.Sample(gSampler, uv).r;
    outputColor.a *= lerp(1.0f, mask, mat.maskIntensity);
}
