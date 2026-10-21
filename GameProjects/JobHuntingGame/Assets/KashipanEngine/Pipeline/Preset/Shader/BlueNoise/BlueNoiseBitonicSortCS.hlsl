// Bitonic Sortの1パス分の比較・交換のみを行う（groupshared/GroupMemoryBarrierを使う
// 高速な「ローカル」実装ではなく、C++側がステージ・ステップの数だけDispatchを繰り返し呼ぶ
// 「グローバル」な実装。このエンジンのコンピュートシェーダーにgroupsharedを使う前例が無く、
// 1回限りの起動時生成であれば多少ディスパッチ回数が多くてもボトルネックにならないため、
// 実装・検証の単純さを優先した
cbuffer BlueNoiseSortConstants : register(b0) {
    uint gTotal;
    uint gBlockSize;        // 2^stage
    uint gCompareDistance;  // 2^step
    uint gSortPadding;
};

RWStructuredBuffer<float> gSortKeys : register(u0);
RWStructuredBuffer<uint> gSortIndices : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint i = dispatchThreadID.x;
    if (i >= gTotal) return;

    uint partner = i ^ gCompareDistance;
    // 各ペアは(小さい方のインデックス)側のスレッドだけが処理する（重複処理・範囲外参照を防ぐ）
    if (partner <= i || partner >= gTotal) return;

    bool ascending = ((i & gBlockSize) == 0);
    float keyA = gSortKeys[i];
    float keyB = gSortKeys[partner];
    bool shouldSwap = ascending ? (keyA > keyB) : (keyA < keyB);
    if (shouldSwap) {
        gSortKeys[i] = keyB;
        gSortKeys[partner] = keyA;
        uint indexA = gSortIndices[i];
        uint indexB = gSortIndices[partner];
        gSortIndices[i] = indexB;
        gSortIndices[partner] = indexA;
    }
}
