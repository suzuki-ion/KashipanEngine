#include "FullScreenTriangle.hlsli"

cbuffer GaussianCB : register(b0)
{
	float2 invResolution;
	int radius;
	float sigma;
	float pad;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float Gaussian(float x, float sigma) {
	return exp(- (x * x) / (2.0 * sigma * sigma));
}

float4 main(VSOutput input) : SV_Target {
	float2 uv = input.uv;
	float2 texel = invResolution;
	float4 sum = float4(0,0,0,0);
	float wsum = 0.0f;
	for (int y = -radius; y <= radius; ++y) {
		for (int x = -radius; x <= radius; ++x) {
			float2 offset = float2(x, y) * texel;
			float weight = Gaussian(length(offset), sigma);
			sum += gTexture.Sample(gSampler, uv + offset) * weight;
			wsum += weight;
		}
	}
	return sum / wsum;
}
