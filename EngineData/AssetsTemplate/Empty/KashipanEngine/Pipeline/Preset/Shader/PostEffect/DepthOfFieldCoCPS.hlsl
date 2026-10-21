// 被写界深度: 深度とフォーカス距離からCoC（錯乱円、ぼかし量）を計算する
// r チャンネル = 遠景CoC（フォーカスより奥）、g チャンネル = 近景CoC（フォーカスより手前）
// 前景と背景を別々にブラーするための入力として、この2つを分離して出力する
#include "FullScreenTriangle.hlsli"
#include "DepthOfFieldCoCCB.hlsli"

Texture2D gDepthTexture : register(t0);
SamplerState gDepthSampler : register(s0);

float LinearizeDepth(float nonLinearDepth) {
	return (gNearClip * gFarClip) / (gFarClip - nonLinearDepth * (gFarClip - gNearClip));
}

float4 main(VSOutput input) : SV_Target0 {
	float rawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (rawDepth >= 1.0f) {
		// 遠平面（スカイボックス等）は最大の遠景ブラーとして扱う
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	float linearDepth = LinearizeDepth(rawDepth);
	float focusStart = gFocusDistance - gFocusRange * 0.5f;
	float focusEnd = gFocusDistance + gFocusRange * 0.5f;

	float farCoC = 0.0f;
	float nearCoC = 0.0f;
	if (linearDepth > focusEnd) {
		farCoC = saturate((linearDepth - focusEnd) / max(gFarBlurDistance, 1e-4f));
	} else if (linearDepth < focusStart) {
		nearCoC = saturate((focusStart - linearDepth) / max(gNearBlurDistance, 1e-4f));
	}

	return float4(farCoC, nearCoC, 0.0f, 1.0f);
}
