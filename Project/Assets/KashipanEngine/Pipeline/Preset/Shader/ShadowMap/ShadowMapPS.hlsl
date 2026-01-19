#include "../Object/Object3D.hlsli"

struct Material {
	float enableLighting;
	float enableShadowMapProjection;
	float4 color;
	float4x4 uvTransform;
	float shininess;
	float4 specularColor;
};
Texture2D gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
SamplerState gSampler : register(s0);

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput o;
	Material mat = gMaterials[input.instanceId];
	float4 color = mat.color;
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	o.color = color * textureColor;
	if (o.color.a < 0.1f) {
		discard;
	}
    return o;
}
