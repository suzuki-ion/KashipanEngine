#include "../Object/Object3D.hlsli"
#include "../Common/Material3D.hlsli"

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
