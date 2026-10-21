#include "FullScreenTriangle.hlsli"
#include "DissolveCB.hlsli"

Texture2D gTexture : register(t0);
Texture2D gBaseTexture : register(t1);
Texture2D gMaskTexture : register(t2);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
	float4 color = float4(0, 0, 0, 1);
	if (useBaseTexture) {
		color = gBaseTexture.Sample(gSampler, input.uv);
		color *= baseTextureColor;
	}
	float4 screenColor = gTexture.Sample(gSampler, input.uv);
	if (useMaskTexture) {
		float maskValue = gMaskTexture.Sample(gSampler, input.uv).r;
		if (maskValue < maskThreshold) {
			return screenColor;
		} else if (maskValue < maskThreshold + edgeThickness) {
			float edge = 1.0 - smoothstep(maskThreshold, maskThreshold + edgeThickness, maskValue);
			color += edge * edgeColor;
		}
	} else {
		if (screenColor.a < maskThreshold) {
			return screenColor;
		} else if (screenColor.a < maskThreshold + edgeThickness) {
			float edge = 1.0 - smoothstep(maskThreshold, maskThreshold + edgeThickness, screenColor.a);
			color += edge * edgeColor;
		}
	}
	// 置き換え色のアルファは色の合成率として使い、シーンの透明度を維持する。
	return float4(lerp(screenColor.rgb, color.rgb, saturate(color.a)), screenColor.a);
}
