// 押し出しアウトライン用のピクセルシェーダー。マテリアルのoutlineColorをそのまま塗るだけ
#include "ObjectOutline.hlsli"

StructuredBuffer<Material> gMaterials : register(t0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(OutlineVSOutput input) {
	PSOutput output;
	output.color = gMaterials[input.instanceId].outlineColor;
	return output;
}
