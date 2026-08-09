// 被写界深度: 近景CoCの最大値フィルタ（ダイレーション）
// 前景オブジェクトのボケが、その物体本来のシルエットより外側（背景側）にもにじみ出るように、
// 近景CoCを周辺の最大値で膨張させる（これを行わないと前景ボケが輪郭でスパッと切れて不自然になる）
#include "FullScreenTriangle.hlsli"
#include "DepthOfFieldDilateCB.hlsli"

Texture2D gCoCTexture : register(t0); // gチャンネルに近景CoCが入っている
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
	float maxNearCoC = 0.0f;
	for (int y = -gRadius; y <= gRadius; ++y) {
		for (int x = -gRadius; x <= gRadius; ++x) {
			float2 uv = input.uv + float2(x, y) * gTexelSize;
			maxNearCoC = max(maxNearCoC, gCoCTexture.Sample(gSampler, uv).g);
		}
	}
	return float4(maxNearCoC, maxNearCoC, maxNearCoC, 1.0f);
}
