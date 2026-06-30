#include "Object3D.hlsli"

static const uint kMaxSkinningMatrices = 256;

struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
	float3 normal : NORMAL0;
	uint4 boneIndices : BLENDINDICES0;
	float4 boneWeights : BLENDWEIGHT0;
};

struct TransformationMatrix {
	float4x4 world;
};

struct Skinned {
	float4 position;
	float3 normal;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

cbuffer gSkinningMatrices : register(b1) {
	float4x4 skinningMatrices[kMaxSkinningMatrices];
	float4x4 skinningNormalMatrices[kMaxSkinningMatrices];
};

Skinned Skinning(VSInput input) {
	Skinned skinned;
	
	skinned.position = mul(float4(input.position.xyz, 1.0), skinningMatrices[input.boneIndices.x]) * input.boneWeights.x;
	skinned.position += mul(float4(input.position.xyz, 1.0), skinningMatrices[input.boneIndices.y]) * input.boneWeights.y;
	skinned.position += mul(float4(input.position.xyz, 1.0), skinningMatrices[input.boneIndices.z]) * input.boneWeights.z;
	skinned.position += mul(float4(input.position.xyz, 1.0), skinningMatrices[input.boneIndices.w]) * input.boneWeights.w;
	skinned.position.w = 1.0f;
	
	skinned.normal = mul(input.normal, (float3x3) skinningNormalMatrices[input.boneIndices.x]) * input.boneWeights.x;
	skinned.normal += mul(input.normal, (float3x3) skinningNormalMatrices[input.boneIndices.y]) * input.boneWeights.y;
	skinned.normal += mul(input.normal, (float3x3) skinningNormalMatrices[input.boneIndices.z]) * input.boneWeights.z;
	skinned.normal += mul(input.normal, (float3x3) skinningNormalMatrices[input.boneIndices.w]) * input.boneWeights.w;
	skinned.normal = normalize(skinned.normal);
	
	return skinned;
};

VSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
	VSOutput output;
	Skinned skinned = Skinning(input);
	float4x4 world = gTransformationMatrices[instanceId].world;
	float4x4 worldViewProjection = mul(world, gCamera3D.viewProjection);
	output.position = mul(float4(skinned.position.xyz, 1.0), worldViewProjection);
	output.worldPosition = mul(float4(skinned.position.xyz, 1.0), world).xyz;
	output.texcoord = input.texcoord;
	output.normal = normalize(mul(skinned.normal, (float3x3) world));
	output.instanceId = instanceId;
	return output;
}
