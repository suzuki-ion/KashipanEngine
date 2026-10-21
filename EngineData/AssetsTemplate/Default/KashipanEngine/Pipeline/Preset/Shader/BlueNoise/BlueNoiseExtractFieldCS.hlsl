// 逆DFT後の複素数バッファから実部を取り出し、Bitonic Sortにかけるための
// (キー=値, インデックス=元の位置) のペアを初期化する
cbuffer BlueNoiseExtractConstants : register(b0) {
    uint gSize;
    uint3 gExtractPadding;
};

RWStructuredBuffer<float2> gSpatial : register(u0);
RWStructuredBuffer<float> gSortKeys : register(u1);
RWStructuredBuffer<uint> gSortIndices : register(u2);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint index = dispatchThreadID.x;
    uint total = gSize * gSize;
    if (index >= total) return;

    gSortKeys[index] = gSpatial[index].x;
    gSortIndices[index] = index;
}
