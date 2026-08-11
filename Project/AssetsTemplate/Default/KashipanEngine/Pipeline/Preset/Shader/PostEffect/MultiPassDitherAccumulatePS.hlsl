#include "FullScreenTriangle.hlsli"
#include "MultiPassDitherAccumulateCB.hlsli"

// このパス（位相違いの1回分）の描画結果。discardされた画素は(0,0,0,0)のまま
Texture2D gSceneTexture : register(t0);
SamplerState gSampler : register(s0);

// 1/N倍してから加算合成（BlendState: AddUnweighted）で蓄積バッファへ足し込む。
// discardされた画素は(0,0,0,0)のままなので蓄積には寄与せず、
// 通った画素だけがオブジェクト自身の色を1/N分だけ積み上げていく
float4 main(VSOutput input) : SV_Target0 {
    float4 color = gSceneTexture.Sample(gSampler, input.uv);
    return color * gInvPassCount;
}
