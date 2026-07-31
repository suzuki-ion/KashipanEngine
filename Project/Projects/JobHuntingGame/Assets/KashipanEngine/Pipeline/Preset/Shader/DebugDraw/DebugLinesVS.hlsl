#include "../Common/Camera3D.hlsli"

struct VSInput {
	float3 position : POSITION0;
	float4 color : COLOR0;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : COLOR0;
};

VSOutput main(VSInput input) {
	VSOutput output;
	output.position = mul(float4(input.position, 1.0), gCamera3D.viewProjection);
	output.color = input.color;
	return output;
}
