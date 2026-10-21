#include "BlueNoiseCommon.hlsli"

// 2次元DFT（順方向・逆方向共通）。1回限りの起動時生成のため、共有メモリを使う
// バタフライ演算による高速フーリエ変換ではなく、直接計算（O(N^4)）で実装している。
// 64x64程度のサイズなら出力1点あたりN^2=4096回の複素乗算で済み、GPUの並列性で
// 十分高速（全体でミリ秒〜数十ミリ秒程度）に収まるため、実装・検証の単純さを優先した
cbuffer BlueNoiseDFTConstants : register(b0) {
    uint gSize;
    uint gInverse; // 0=順方向, 非0=逆方向
    float2 gDFTPadding;
};

// 読み取り専用として使うだけでも、後段でSRVとして別バインドする必要が無いよう
// RWStructuredBufferで統一する（コンピュートパイプラインは手動ルートディスクリプタのため、
// UAV用に確保したバッファをSRVとしてバインドしようとすると失敗するのを避けるため）
RWStructuredBuffer<float2> gSource : register(u0);
RWStructuredBuffer<float2> gDest : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint fx = dispatchThreadID.x;
    uint fy = dispatchThreadID.y;
    if (fx >= gSize || fy >= gSize) return;

    float sign = (gInverse != 0) ? 1.0f : -1.0f;
    float2 sum = float2(0.0f, 0.0f);
    for (uint y = 0; y < gSize; ++y) {
        for (uint x = 0; x < gSize; ++x) {
            float angle = sign * 2.0f * kPi * (float(fx * x) / float(gSize) + float(fy * y) / float(gSize));
            float c = cos(angle);
            float s = sin(angle);
            float2 value = gSource[y * gSize + x];
            sum.x += value.x * c - value.y * s;
            sum.y += value.x * s + value.y * c;
        }
    }
    if (gInverse != 0) {
        sum /= float(gSize * gSize);
    }
    gDest[fy * gSize + fx] = sum;
}
