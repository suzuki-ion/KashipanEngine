#ifdef Object2D
#include "../Common/Camera2D.hlsli"
#include "Object2D.hlsli"
#endif

#ifdef Object3D
#include "../Common/Camera3D.hlsli"
#include "Object3D.hlsli"
#endif

struct Material {
	float4 color;
};

ConstantBuffer<Material> gMaterial : register(b1);
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
