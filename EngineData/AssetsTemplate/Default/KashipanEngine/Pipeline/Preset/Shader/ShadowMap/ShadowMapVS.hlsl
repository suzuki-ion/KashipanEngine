#include "../Object/Object3D.hlsli"

struct VSInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal   : NORMAL0;
};

struct TransformationMatrix {
    float4x4 world;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);
// オブジェクト固有のディザ用シード値（本体描画のObjectVS.hlslと同じ考え方。ShadowMapPS.hlsl参照）
StructuredBuffer<float> gObjectIdSeeds : register(t1);

VSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
    VSOutput output;

    float4x4 world = gTransformationMatrices[instanceId].world;
    float4x4 worldLightVP = mul(world, gCamera3D.viewProjection);

    output.position = mul(input.position, worldLightVP);
    
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3)world));
    output.worldPosition = mul(input.position, world).xyz;
    output.instanceId = instanceId;
    output.idSeed = gObjectIdSeeds[instanceId];

    return output;
}
