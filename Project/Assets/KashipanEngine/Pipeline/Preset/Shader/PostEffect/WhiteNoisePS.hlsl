#include "FullScreenTriangle.hlsli"
#include "WhiteNoiseCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float Rand2DTo1D(float2 seed) {
	float2 smallValue = sin(seed);
	float random = dot(smallValue, float2(12.9898, 78.233));
	random = frac(sin(random) * 143758.5453);
	return random;
}

float4 main(VSOutput input) : SV_Target0 {
	float4 color = gTexture.Sample(gSampler, input.uv);
	float noise = Rand2DTo1D(input.uv * gNoiseScale + gNoiseSeed + gNoiseTime);
	float4 noiseColor = gNoiseColor * noise;
	color.rgb = lerp(color.rgb, noiseColor.rgb, gNoiseIntensity);
	return color;
}