#include "ObjectVelocity.hlsli"

struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
	float3 normal : NORMAL0;
};

struct TransformationMatrix {
	float4x4 world;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

ObjectVelocityVSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
	ObjectVelocityVSOutput output;
	float4x4 world = gTransformationMatrices[instanceId].world;

	float4x4 worldViewProjection = mul(world, gObjectVelocityCamera.viewProjection);
	output.position = mul(input.position, worldViewProjection);

	float4x4 prevWorldViewProjection = mul(world, gObjectVelocityCamera.prevViewProjection);
	output.prevPosition = mul(input.position, prevWorldViewProjection);

	output.texcoord = input.texcoord;
	output.instanceId = instanceId;
	
	return output;
}
