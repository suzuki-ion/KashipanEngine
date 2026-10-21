// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// #includeは物理的な設置場所（Modules/GradientOverlay2D/）ではなく、生成先（Object/）からの相対パスで書くこと

// グラデーションオーバーレイ: UVのY座標でgradientColorBottom→gradientColorTopを線形補間し、
// gradientBlendでoutputColor.rgbへブレンドする（COMPOSITE_HOOKS_2Dの中でVignette2Dより先、優先度5で呼ぶこと）
void ApplyGradientOverlay2D(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.gradientBlend <= 0.0001f) return;
    float3 gradient = lerp(mat.gradientColorBottom.rgb, mat.gradientColorTop.rgb, saturate(uv.y));
    outputColor.rgb = lerp(outputColor.rgb, gradient, mat.gradientBlend);
}
