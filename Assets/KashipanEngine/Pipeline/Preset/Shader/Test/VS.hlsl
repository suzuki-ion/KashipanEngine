#include "../Common/Camera3D.hlsli"

struct TransformationMatrix {
	float4x4 world;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

struct VSInput {
	float4 position : POSITION0;
};

struct VSOutput {
	float4 position : SV_POSITION;
};

VSOutput main(VSInput input) {
	VSOutput output;
	float4x4 worldViewProjection = mul(gTransformationMatrix.world, gCamera.viewProjection);
	output.position = mul(input.position, worldViewProjection);
	return output;
}