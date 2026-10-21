#include "BlueNoiseCommon.hlsli"

// 周波数領域で低周波成分を抑制する（高周波を残す＝ブルーノイズらしさの核心部分）
cbuffer BlueNoiseFilterConstants : register(b0) {
    uint gSize;
    float gLowFreqCutoff; // 中心（DC成分）からの正規化距離。この値より内側の低周波を弱める
    float2 gFilterPadding;
};

RWStructuredBuffer<float2> gSource : register(u0);
RWStructuredBuffer<float2> gDest : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint fx = dispatchThreadID.x;
    uint fy = dispatchThreadID.y;
    if (fx >= gSize || fy >= gSize) return;

    // DFT結果はインデックス0がDC成分（周波数0）で、負の周波数は後半（gSize/2以降）に
    // 折り返して格納される配置になっているため、中心からの距離を求めるにはラップアラウンドを
    // 考慮する必要がある
    int half_ = int(gSize) / 2;
    int dx = int(fx);
    if (dx > half_) dx -= int(gSize);
    int dy = int(fy);
    if (dy > half_) dy -= int(gSize);
    float dist = length(float2(dx, dy)) / float(max(half_, 1));

    float weight = saturate(dist / max(gLowFreqCutoff, 0.0001f));
    uint idx = fy * gSize + fx;
    gDest[idx] = gSource[idx] * weight;
}
