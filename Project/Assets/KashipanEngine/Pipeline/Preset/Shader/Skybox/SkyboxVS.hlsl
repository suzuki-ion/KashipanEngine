#ifdef Skybox
#include "../Common/Camera3D.hlsli"

struct VSInput {
    float4 position : POSITION0;
    float3 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

struct TransformationMatrix {
    float4x4 world;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

VSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
    VSOutput output;
    float4x4 world = gTransformationMatrices[instanceId].world;
    float4x4 wvp = mul(world, gCamera3D.viewProjection);
    output.position = mul(input.position, wvp).xyww;
    output.texcoord = input.position.xyz;
    return output;
}
#endif
