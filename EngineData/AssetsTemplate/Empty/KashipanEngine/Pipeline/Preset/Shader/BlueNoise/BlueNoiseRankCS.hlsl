// ソート済みの(キー,インデックス)列から、元のインデックスごとに正規化した順位を書き出す。
// これが最終的にObjectPS.hlslから参照されるディザ閾値テーブルになる
cbuffer BlueNoiseRankConstants : register(b0) {
    uint gTotal;
    uint3 gRankPadding;
};

RWStructuredBuffer<uint> gSortIndices : register(u0);
RWStructuredBuffer<float> gDitherValues : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint sortedPosition = dispatchThreadID.x;
    if (sortedPosition >= gTotal) return;

    uint originalIndex = gSortIndices[sortedPosition];
    gDitherValues[originalIndex] = (float(sortedPosition) + 0.5f) / float(gTotal);
}
