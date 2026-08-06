// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// #includeは物理的な設置場所（Modules/Dissolve2D/）ではなく、生成先（Object/）からの相対パスで書くこと
#include "../Common/Noise.hlsli"

// ディゾルブ: ノイズ値がdissolveThreshold未満のピクセルを消し、境界をdissolveEdgeColorで縁取る。
// 3D版ApplyDissolveと同じ数式だが、worldPositionが無いためUVをノイズの種にする
// （ALPHA_HOOKS_2Dの中で呼ぶこと）
void ApplyDissolve2D(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.dissolveThreshold <= 0.0001f) return;
    float noise = Rand2DTo1D(uv * mat.dissolveNoiseScale);
    if (noise < mat.dissolveThreshold) {
        discard;
    }
    float edgeFactor = 1.0f - saturate((noise - mat.dissolveThreshold) / max(mat.dissolveEdgeWidth, 0.0001f));
    outputColor.rgb = lerp(outputColor.rgb, mat.dissolveEdgeColor.rgb, edgeFactor);
}
