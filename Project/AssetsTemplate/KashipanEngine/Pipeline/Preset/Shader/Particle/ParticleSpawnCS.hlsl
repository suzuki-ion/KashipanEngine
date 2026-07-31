// GPUパーティクル用: 今フレーム新規発生したパーティクルをgParticlesへ書き込むコンピュートシェーダー
// CPU側（ParticleSystemBase::UpdateParticlesGPU）が発生タイミング・スポーンパラメータの抽選を行い、
// その結果（GPUParticleSpawnRequest）をgSpawnRequestsへ書き込んでからこのシェーダーを実行する。
// 書き込み先スロットはCPUでは決めず、gFreeList（空きスロット番号を積んだ手動スタック）から
// このシェーダーがpopして決定する（生存中のパーティクルを誤って上書きしないため）。
// スタックが枯渇していた場合、そのリクエストのスポーンは諦める（破棄する）。

cbuffer ParticleSpawnConstants : register(b0)
{
    uint gSpawnCount;
    uint gPadding0;
    uint gPadding1;
    uint gPadding2;
};

struct GPUParticleSpawnRequest
{
    float lifetime;
    float pad0;
    float pad1;
    float pad2;
    float3 position;
    float pad3;
    float3 velocity;
    float pad4;
    float3 acceleration;
    float pad5;
    float3 rotation;
    float pad6;
    float3 angularVelocity;
    float pad7;
    float3 angularAcceleration;
    float pad8;
    float3 startScale;
    float pad9;
    float3 endScale;
    float pad10;
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

StructuredBuffer<GPUParticleSpawnRequest> gSpawnRequests : register(t0);
RWStructuredBuffer<GPUParticleData> gParticles : register(u0);
// フリーリスト本体（下からgFreeListCounter[0]件が有効な空きスロット番号）
RWStructuredBuffer<uint> gFreeList : register(u1);
// フリーリストの残数（1要素のみ使用）
RWStructuredBuffer<int> gFreeListCounter : register(u2);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint requestIndex = dispatchThreadID.x;
    if (requestIndex >= gSpawnCount)
    {
        return;
    }

    // フリーリストから空きスロットを1つpopする。枯渇していた場合はカウンタを戻して諦める
    int oldCount;
    InterlockedAdd(gFreeListCounter[0], -1, oldCount);
    if (oldCount <= 0)
    {
        InterlockedAdd(gFreeListCounter[0], 1, oldCount);
        return;
    }
    uint slotIndex = gFreeList[oldCount - 1];

    GPUParticleSpawnRequest req = gSpawnRequests[requestIndex];

    GPUParticleData particle;
    particle.position = req.position;
    particle.age = 0.0f;
    particle.velocity = req.velocity;
    particle.lifetime = req.lifetime;
    particle.acceleration = req.acceleration;
    particle.alive = 1;
    particle.rotation = req.rotation;
    particle.pad0 = 0.0f;
    particle.angularVelocity = req.angularVelocity;
    particle.pad1 = 0.0f;
    particle.angularAcceleration = req.angularAcceleration;
    particle.pad2 = 0.0f;
    particle.startScale = req.startScale;
    particle.pad3 = 0.0f;
    particle.endScale = req.endScale;
    particle.pad4 = 0.0f;

    gParticles[slotIndex] = particle;
}
