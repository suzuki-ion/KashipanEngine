// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// #includeは物理的な設置場所（Modules/Dissolve/）ではなく、生成先（Object/）からの相対パスで書くこと
#include "../Common/Noise.hlsli"

// ディゾルブ: ノイズ値がdissolveThreshold未満のピクセルを消し、境界をdissolveEdgeColorで縁取る
// （ALPHA_HOOKSの中でDistanceFadeより後、優先度20で呼ぶこと）
void ApplyDissolve(inout float4 outputColor, Material mat, float3 worldPosition) {
    if (mat.dissolveThreshold <= 0.0001f) return;
    float noise = Rand2DTo1D(worldPosition.xz * mat.dissolveNoiseScale);
    if (noise < mat.dissolveThreshold) {
        discard;
    }
    float edgeFactor = 1.0f - saturate((noise - mat.dissolveThreshold) / max(mat.dissolveEdgeWidth, 0.0001f));
    outputColor.rgb = lerp(outputColor.rgb, mat.dissolveEdgeColor.rgb, edgeFactor);
}
