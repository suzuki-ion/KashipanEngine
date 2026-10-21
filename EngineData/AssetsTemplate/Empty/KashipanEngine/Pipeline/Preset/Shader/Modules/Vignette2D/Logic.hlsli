// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// #includeは物理的な設置場所（Modules/Vignette2D/）ではなく、生成先（Object/）からの相対パスで書くこと

// ヴィネット: UV中心(0.5, 0.5)からの距離がvignetteRadiusを超えた範囲を減光する
// （COMPOSITE_HOOKS_2Dの中でColorGrading2Dより先、優先度10で呼ぶこと）
void ApplyVignette2D(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.vignetteIntensity <= 0.0001f) return;
    float dist = length(uv - float2(0.5f, 0.5f)) * 2.0f;
    float vignette = 1.0f - saturate((dist - mat.vignetteRadius) / max(mat.vignetteSoftness, 0.0001f));
    outputColor.rgb *= lerp(1.0f, vignette, mat.vignetteIntensity);
}
