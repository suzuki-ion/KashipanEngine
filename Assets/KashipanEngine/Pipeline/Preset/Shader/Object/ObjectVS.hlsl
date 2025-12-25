#ifdef Object2D
#include "../Common/Camera2D.hlsli"
#include "Object2D.hlsli"
struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
};
#endif

#ifdef Object3D
#include "../Common/Camera3D.hlsli"
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

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

VSOutput main(VSInput input) {
	VSOutput output;
	float4x4 worldViewProjection = mul(gTransformationMatrix.world, gCamera.viewProjection);
	output.position = mul(input.position, worldViewProjection);
	output.texcoord = input.texcoord;
#ifdef Object3D
	output.normal = normalize(mul(input.normal, (float3x3)gTransformationMatrix.world));
#endif
	return output;
}