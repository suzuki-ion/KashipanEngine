#include "Object3D.hlsli"

struct Material {
	float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
	PSOutput output;
	float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
	output.color = gMaterial.color * textureColor;
	return output;
}
