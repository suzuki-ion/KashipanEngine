#ifdef Object2D
#include "Object2D.hlsli"
struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
};
#endif

#ifdef Object3D
#include "Object3D.hlsli"
struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
	float3 normal : NORMAL0;
};

#endif

struct TransformationMatrix {
	float4x4 world;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

VSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
	VSOutput output;
	float4x4 world = gTransformationMatrices[instanceId].world;
	
#ifdef Object2D
	float4x4 worldViewProjection = mul(world, gCamera2D.viewProjection);
#endif
#ifdef Object3D
	float4x4 worldViewProjection = mul(world, gCamera3D.viewProjection);
#endif

	output.position = mul(input.position, worldViewProjection);
	output.texcoord = input.texcoord;
#ifdef Object3D
	output.normal = normalize(mul(input.normal, (float3x3)world));
	output.worldPosition = mul(input.position, world).xyz;
#endif
	output.instanceId = instanceId;
	return output;
}