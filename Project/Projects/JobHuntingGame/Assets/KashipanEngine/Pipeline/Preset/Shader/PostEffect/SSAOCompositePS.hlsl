// AOを既にシェーディング済みのシーン色へ乗算合成する
// （深度専用のG-bufferを持たないための簡略化として、間接光だけでなく鏡面成分にも
//  多少影響してしまう点はこの方式の既知の妥協点）
#include "FullScreenTriangle.hlsli"

Texture2D gSceneTexture : register(t0);
Texture2D gSSAOTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_Target0 {
	float4 sceneColor = gSceneTexture.Sample(gSampler, input.uv);
	float ao = gSSAOTexture.Sample(gSampler, input.uv).r;
	return float4(sceneColor.rgb * ao, sceneColor.a);
}
