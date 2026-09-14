// GPUパーティクル用: gFreeList（手動スタック方式のフリーリスト）を初期状態へ積み直すコンピュートシェーダー
// パーティクルバッファが新規生成・容量変更された直後に一度だけ実行され、
// 0..capacity-1 の全スロットを「空き」として積み、残数カウンタをcapacityにする。
// 併せてgParticles[index].aliveも0にクリアする。DEFAULTヒープ上に新規生成されたバッファは
// 内容が不定（前回この容量で確保されていた別用途のGPUメモリを再利用した場合、aliveが非0の
// ゴミ値を含み得る）であり、これをクリアしないとParticleUpdateCSがゴミ値を「生存中」と誤認し、
// 既にここで積んだフリーリストへ寿命判定時に重複してpushしてgFreeListの範囲外書き込み
// （＝GPUクラッシュ/デバイスリムーブ）を引き起こす。

cbuffer ParticleFreeListInitConstants : register(b0)
{
    uint gCapacity;
    uint gPadding0;
    uint gPadding1;
    uint gPadding2;
};

struct GPUParticleData
{
    float3 position;
    float age;
    float3 velocity;
    float lifetime;
    float3 acceleration;
    uint alive;
    float3 rotation;
    float pad0;
    float3 angularVelocity;
    float pad1;
    float3 angularAcceleration;
    float pad2;
    float3 startScale;
    float pad3;
    float3 endScale;
    float pad4;
};

RWStructuredBuffer<uint> gFreeList : register(u0);
RWStructuredBuffer<int> gFreeListCounter : register(u1);
RWStructuredBuffer<GPUParticleData> gParticles : register(u2);

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

    gParticles[index].alive = 0;
}
