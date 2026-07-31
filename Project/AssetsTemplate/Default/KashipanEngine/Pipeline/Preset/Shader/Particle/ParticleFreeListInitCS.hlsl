// GPUパーティクル用: gFreeList（手動スタック方式のフリーリスト）を初期状態へ積み直すコンピュートシェーダー
// パーティクルバッファが新規生成・容量変更された直後に一度だけ実行され、
// 0..capacity-1 の全スロットを「空き」として積み、残数カウンタをcapacityにする。

cbuffer ParticleFreeListInitConstants : register(b0)
{
    uint gCapacity;
    uint gPadding0;
    uint gPadding1;
    uint gPadding2;
};

RWStructuredBuffer<uint> gFreeList : register(u0);
RWStructuredBuffer<int> gFreeListCounter : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    if (index >= gCapacity)
    {
        return;
    }

    // 各スレッドが自分の担当インデックスだけを書くため競合しない。
    // 残数カウンタは代表して1スレッドだけが設定すればよい
    gFreeList[index] = index;
    if (index == 0)
    {
        gFreeListCounter[0] = (int) gCapacity;
    }
}
