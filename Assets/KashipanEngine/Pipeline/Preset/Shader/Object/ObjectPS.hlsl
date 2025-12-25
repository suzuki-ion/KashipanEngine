#ifdef Object2D
#include "Object2D.hlsli"
struct Material {
	float4 color;
};
#endif

#ifdef Object3D
#include "../Common/Camera3D.hlsli"
#include "Object3D.hlsli"
struct Material {
	float4 color;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
#endif

ConstantBuffer<Material> gMaterial : register(b1);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
	PSOutput output;
	float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
#ifdef Object2D
	output.color = gMaterial.color * textureColor;
#endif

#ifdef Object3D
	if (gDirectionalLight.enabled) {
		float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
		float4 lightColor = gDirectionalLight.color * gDirectionalLight.intensity * cos;
		output.color = (gMaterial.color * textureColor) * lightColor;
	} else {
		output.color = gMaterial.color * textureColor;
	}
	output.color.a = gMaterial.color.a * textureColor.a;
#endif
	return output;
}
