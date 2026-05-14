#ifdef Skybox
#include "../Common/Camera3D.hlsli"

struct Material {
	float enableLighting;
	float enableShadowMapProjection;
	float4 color;
	float4x4 uvTransform;
	float shininess;
	float4 specularColor;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_TARGET0;
};

TextureCube gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
SamplerState gSampler : register(s0);

PSOutput main(PSInput input) {
    PSOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    Material mat = gMaterials[0];
    output.color = mat.color * textureColor;
    return output;
}
#endif
