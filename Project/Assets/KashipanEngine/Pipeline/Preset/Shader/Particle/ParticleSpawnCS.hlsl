// GPUパーティクル用: 今フレーム新規発生したパーティクルをgParticlesへ書き込むコンピュートシェーダー
// CPU側（ParticleSystemBase::UpdateParticlesGPU）が発生タイミング・スポーンパラメータの抽選を行い、
// その結果（GPUParticleSpawnRequest）をgSpawnRequestsへ書き込んでからこのシェーダーを実行する。
// リングバッファ方式のため、スロットが生存中でも新規リクエストで上書きされる（仕様通り）。

cbuffer ParticleSpawnConstants : register(b0)
{
    uint gSpawnCount;
    uint gPadding0;
    uint gPadding1;
    uint gPadding2;
};

struct GPUParticleSpawnRequest
{
    uint slotIndex;
    float lifetime;
    float pad0;
    float pad1;
    float3 position;
    float pad2;
    float3 velocity;
    float pad3;
    float3 acceleration;
    float pad4;
    float3 rotation;
    float pad5;
    float3 angularVelocity;
    float pad6;
    float3 angularAcceleration;
    float pad7;
    float3 startScale;
    float pad8;
    float3 endScale;
    float pad9;
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

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint requestIndex = dispatchThreadID.x;
    if (requestIndex >= gSpawnCount)
    {
        return;
    }

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

    gParticles[req.slotIndex] = particle;
}
