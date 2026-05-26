#include "FullScreenTriangle.hlsli"
#include "ColorAdjustCB.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float3 ApplyTemperature(float3 color, float temperature) {
	float warm = saturate(temperature);
	float cool = saturate(-temperature);
	float3 warmTint = float3(1.0 + warm * 0.1, 1.0, 1.0 - warm * 0.1);
	float3 coolTint = float3(1.0 - cool * 0.1, 1.0, 1.0 + cool * 0.1);
	return color * warmTint * coolTint;
}

float3 ApplyContrast(float3 color, float contrast) {
	return (color - 0.5f) * contrast + 0.5f;
}

float3 ApplySaturation(float3 color, float saturation) {
	float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
	return lerp(float3(lum, lum, lum), color, saturation);
}

// Rotate hue around the luminance axis. hueDegrees in degrees.
float3 ApplyHue(float3 color, float hueDegrees) {
	float a = radians(hueDegrees);
	float cosA = cos(a);
	float sinA = sin(a);
	float3x3 m = float3x3(
		0.299 + 0.701 * cosA + 0.168 * sinA, 0.587 - 0.587 * cosA + 0.330 * sinA, 0.114 - 0.114 * cosA - 0.497 * sinA,
		0.299 - 0.299 * cosA - 0.328 * sinA, 0.587 + 0.413 * cosA + 0.035 * sinA, 0.114 - 0.114 * cosA + 0.292 * sinA,
		0.299 - 0.300 * cosA + 1.250 * sinA, 0.587 - 0.588 * cosA - 1.050 * sinA, 0.114 + 0.886 * cosA - 0.203 * sinA
	);
	return mul(m, color);
}

float4 main(VSOutput input) : SV_Target0 {
	float2 uv = saturate(input.uv);
	float4 baseColor = gTexture.Sample(gSampler, uv);

	float3 color = baseColor.rgb + gBrightness;
	color = ApplyContrast(color, gContrast);
	color = ApplySaturation(color, gSaturation);
	color = ApplyHue(color, gHue);
	color = ApplyTemperature(color, gTemperature);
	color += gColorBalance;

	return float4(saturate(color), baseColor.a);
}
