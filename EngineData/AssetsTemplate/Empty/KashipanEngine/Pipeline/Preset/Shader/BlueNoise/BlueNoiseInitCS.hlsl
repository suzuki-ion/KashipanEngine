#include "BlueNoiseCommon.hlsli"

cbuffer BlueNoiseInitConstants : register(b0) {
    uint gSize;
    uint3 gInitPadding;
};

// 白色ノイズ（実部=乱数[0,1)、虚部=0）を書き込む。この後のDFTの入力になる
RWStructuredBuffer<float2> gSpectrum : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint index = dispatchThreadID.x;
    uint total = gSize * gSize;
    if (index >= total) return;

    gSpectrum[index] = float2(BlueNoiseRandomUnit(index * 2654435761u + 1u), 0.0f);
}
