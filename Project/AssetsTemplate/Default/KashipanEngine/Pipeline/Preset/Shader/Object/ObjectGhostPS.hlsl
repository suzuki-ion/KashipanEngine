// Prefabのシーンビューへのドラッグ配置プレビュー用のピクセルシェーダー。マテリアルのghostColorを
// そのまま塗るだけ（ObjectPS.hlslのようなディザ抜き半透明ではなく、BlendStateによる通常のアルファ
// ブレンドで滑らかな半透明にする。ObjectOutlinePS.hlslと同じ考え方）
#include "ObjectGhost.hlsli"

StructuredBuffer<Material> gMaterials : register(t0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(GhostVSOutput input) {
	PSOutput output;
	output.color = gMaterials[input.instanceId].ghostColor;
	return output;
}
