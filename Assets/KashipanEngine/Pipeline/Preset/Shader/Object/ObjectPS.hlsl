#ifdef Object2D
#include "Object2D.hlsli"
struct Material {
	float4 color;
	float4x4 uvTransform;
};
#endif

#ifdef Object3D
#include "../Common/Camera3D.hlsli"
#include "Object3D.hlsli"
struct Material {
	float4 color;
	float4x4 uvTransform;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
#endif

Texture2D gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

float Lambert(float3 normal, float3 lightDir) {
	float cos = saturate(dot(normalize(normal), -lightDir));
	return cos;
}

float HalfLambert(float3 normal, float3 lightDir) {
	float NdotL = dot(normalize(normal), -normalize(lightDir));
	float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
	return halfLambert;
}

PSOutput main(VSOutput input) {
	PSOutput output;
	Material mat = gMaterials[input.instanceId];
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
#ifdef Object2D
	output.color = mat.color * textureColor;
#endif

#ifdef Object3D
	if (gDirectionalLight.enabled) {
		float halfLambert = HalfLambert(input.normal, gDirectionalLight.direction);
		float4 lightColor = gDirectionalLight.color * gDirectionalLight.intensity * halfLambert;
		output.color = (mat.color * textureColor) * lightColor;
	} else {
		output.color = mat.color * textureColor;
	}
	output.color.a = mat.color.a * textureColor.a;
#endif
	return output;
}
