// 被写界深度: シャープな画像へ遠景ブラー→近景ブラーの順でCoCに応じて合成する
// 近景を最後に合成することで、前景のボケが背景の上に正しく重なって見える
#include "FullScreenTriangle.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gFarBlurTexture : register(t1);
Texture2D gNearBlurTexture : register(t2);
Texture2D gCoCTexture : register(t3);      // r チャンネル = 遠景CoC
Texture2D gNearMaskTexture : register(t4); // r チャンネル = ダイレーション後の近景CoC
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
	float4 sharp = gSceneTexture.Sample(gSampler, input.uv);
	float farCoC = gCoCTexture.Sample(gSampler, input.uv).r;
	float nearCoC = gNearMaskTexture.Sample(gSampler, input.uv).r;

	float4 color = lerp(sharp, gFarBlurTexture.Sample(gSampler, input.uv), farCoC);
	color = lerp(color, gNearBlurTexture.Sample(gSampler, input.uv), nearCoC);
	return color;
}
