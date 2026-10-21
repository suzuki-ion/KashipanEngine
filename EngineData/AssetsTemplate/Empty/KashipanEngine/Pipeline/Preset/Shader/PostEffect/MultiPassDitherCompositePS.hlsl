#include "FullScreenTriangle.hlsli"

// Nパス分蓄積済みの色（プリマルチプライドアルファ: 覆われた割合だけ色が乗り、
// 残りは背景がそのまま透けるようアルファも同じ割合だけ蓄積されている）
Texture2D gAccumTexture : register(t0);
SamplerState gSampler : register(s0);

// そのまま出力し、BlendState（PremultipliedOver: Src=ONE, Dest=INV_SRC_ALPHA）で
// オーナーの既存シーン色の上へ合成する
float4 main(VSOutput input) : SV_Target0 {
    return gAccumTexture.Sample(gSampler, input.uv);
}
