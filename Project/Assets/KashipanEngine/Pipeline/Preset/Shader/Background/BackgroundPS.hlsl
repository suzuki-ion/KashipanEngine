#include "../PostEffect/FullScreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BackgroundCB : register(b0) {
	float4 gColor;
	float gUseTexture;
	float3 gPadding;
};

float4 main(VSOutput input) : SV_Target0 {
	if (gUseTexture > 0.5f) {
		return gTexture.Sample(gSampler, input.uv);
	}
	return gColor;
}
