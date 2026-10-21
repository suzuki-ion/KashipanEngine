// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。

// ヒットフラッシュ: outputColor.rgbをflashColor.rgbへflashIntensityでlerpする。
// 色調整の影響を受けない最終的な上書きにするため、COMPOSITE_HOOKS_2Dの中で最後（優先度110）に呼ぶこと
void ApplyFlashColor2D(inout float4 outputColor, Material mat) {
    if (mat.flashIntensity <= 0.0001f) return;
    outputColor.rgb = lerp(outputColor.rgb, mat.flashColor.rgb, mat.flashIntensity);
}
