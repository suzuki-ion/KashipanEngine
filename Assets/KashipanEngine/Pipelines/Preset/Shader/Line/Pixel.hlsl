#include "Line.hlsli"

struct PixelShaderOutput {
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(GeometryShaderOutput input) {
	PixelShaderOutput output;
	output.color = input.color;
	return output;
}