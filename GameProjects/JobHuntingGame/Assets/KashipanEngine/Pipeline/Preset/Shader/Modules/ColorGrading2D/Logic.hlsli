// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// #includeは物理的な設置場所（Modules/ColorGrading2D/）ではなく、生成先（Object/）からの相対パスで書くこと
#include "../Common/ColorUtility.hlsli"

// 色調整（Hue/Saturation/Brightness/Gamma）。3D版ColorGradingと同じ数式。
// COMPOSITE_HOOKS_2Dの中で最後（優先度100）に呼ぶこと
void ApplyColorGrading2D(inout float4 outputColor, Material mat) {
    float3 hsv = RgbToHsv(outputColor.rgb);
    hsv.x = frac(hsv.x + mat.hueShift / 360.0f + 1.0f);
    hsv.y = saturate(hsv.y * (1.0f + mat.saturation));
    float3 adjusted = HsvToRgb(hsv);
    adjusted *= (1.0f + mat.brightness);
    float gammaValue = max(1.0f + mat.gamma, 0.01f);
    adjusted = pow(max(adjusted, 0.0f), 1.0f / gammaValue);
    outputColor.rgb = adjusted;
}
