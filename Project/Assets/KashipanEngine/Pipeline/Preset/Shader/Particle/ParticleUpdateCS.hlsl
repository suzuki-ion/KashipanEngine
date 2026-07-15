// GPUパーティクル用: 毎フレームの移動・回転・スケール補間・寿命判定を行うコンピュートシェーダー
// 結果はgInstanceMatricesへ書き込まれ、後続の描画パス（Object2D/Object3Dパイプラインの
// gTransformationMatricesと同じ構造）がそのままインスタンス描画に使用する。
// 常にParticleCapacity件（プール容量）分ディスパッチし、死亡中のパーティクルはスケール0の
// 行列を書き込むことで非表示にする（コンパクションは行わない）。

cbuffer ParticleUpdateConstants : register(b0)
{
    float gDeltaTime;
    uint gParticleCount;
    uint gBillboard;
    uint gBillboardMode; // 0: SyncRotation（カメラ回転をコピー）, 1: LookAt（+Z軸がカメラを向く）
    float4x4 gCameraWorldMatrix;
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

struct TransformationMatrix
{
    float4x4 world;
};

RWStructuredBuffer<GPUParticleData> gParticles : register(u0);
RWStructuredBuffer<TransformationMatrix> gInstanceMatrices : register(u1);

// 以下、行ベクトル規約（v' = mul(v, M)）でMatrix4x4MakeAffine相当を組み立てるヘルパー群
// （KashipanEngine::MathUtils::Matrix4x4MakeRotateX/Y/Z, MakeScale, MakeTranslateと同じ式）

float4x4 MakeScaleMatrix(float3 s)
{
    return float4x4(
        s.x, 0.0f, 0.0f, 0.0f,
        0.0f, s.y, 0.0f, 0.0f,
        0.0f, 0.0f, s.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 MakeRotateXMatrix(float r)
{
    float c = cos(r);
    float s = sin(r);
    return float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, c, s, 0.0f,
        0.0f, -s, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 MakeRotateYMatrix(float r)
{
    float c = cos(r);
    float s = sin(r);
    return float4x4(
        c, 0.0f, -s, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        s, 0.0f, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 MakeRotateZMatrix(float r)
{
    float c = cos(r);
    float s = sin(r);
    return float4x4(
        c, s, 0.0f, 0.0f,
        -s, c, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 MakeTranslateMatrix(float3 t)
{
    return float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        t.x, t.y, t.z, 1.0f);
}

// 自身の+Z軸がforward方向を向く回転行列（行ベクトル規約）。TargetLookAt::MakeLookRotationと同じ式
float4x4 MakeLookRotationMatrix(float3 forward)
{
    float3 up = float3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.999f)
    {
        up = float3(0.0f, 0.0f, 1.0f);
    }
    float3 right = normalize(cross(up, forward));
    float3 realUp = cross(forward, right);

    return float4x4(
        right.x, right.y, right.z, 0.0f,
        realUp.x, realUp.y, realUp.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    if (index >= gParticleCount)
    {
        return;
    }

    GPUParticleData particle = gParticles[index];

    if (particle.alive == 0)
    {
        TransformationMatrix hidden;
        hidden.world = MakeScaleMatrix(float3(0.0f, 0.0f, 0.0f));
        gInstanceMatrices[index] = hidden;
        return;
    }

    particle.age += gDeltaTime;
    if (particle.age >= particle.lifetime)
    {
        particle.alive = 0;
        gParticles[index] = particle;
        TransformationMatrix hidden;
        hidden.world = MakeScaleMatrix(float3(0.0f, 0.0f, 0.0f));
        gInstanceMatrices[index] = hidden;
        return;
    }

    particle.velocity += particle.acceleration * gDeltaTime;
    particle.position += particle.velocity * gDeltaTime;
    particle.rotation += (particle.angularVelocity + 0.5f * particle.angularAcceleration * gDeltaTime) * gDeltaTime;
    particle.angularVelocity += particle.angularAcceleration * gDeltaTime;

    gParticles[index] = particle;

    float t = particle.lifetime > 0.0f ? saturate(particle.age / particle.lifetime) : 1.0f;
    float3 scale = lerp(particle.startScale, particle.endScale, t);

    float4x4 rotateMatrix;
    if (gBillboard != 0)
    {
        if (gBillboardMode == 0)
        {
            // SyncRotation: カメラのワールド回転（上3x3）をそのままコピーする
            rotateMatrix = float4x4(
                gCameraWorldMatrix._11, gCameraWorldMatrix._12, gCameraWorldMatrix._13, 0.0f,
                gCameraWorldMatrix._21, gCameraWorldMatrix._22, gCameraWorldMatrix._23, 0.0f,
                gCameraWorldMatrix._31, gCameraWorldMatrix._32, gCameraWorldMatrix._33, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);
        }
        else
        {
            // LookAt: 自身からカメラ方向へ+Z軸を向ける
            float3 cameraPosition = float3(gCameraWorldMatrix._41, gCameraWorldMatrix._42, gCameraWorldMatrix._43);
            float3 direction = cameraPosition - particle.position;
            if (dot(direction, direction) > 1.0e-12f)
            {
                rotateMatrix = MakeLookRotationMatrix(normalize(direction));
            }
            else
            {
                rotateMatrix = MakeRotateXMatrix(particle.rotation.x);
                rotateMatrix = mul(rotateMatrix, MakeRotateYMatrix(particle.rotation.y));
                rotateMatrix = mul(rotateMatrix, MakeRotateZMatrix(particle.rotation.z));
            }
        }
    }
    else
    {
        rotateMatrix = MakeRotateXMatrix(particle.rotation.x);
        rotateMatrix = mul(rotateMatrix, MakeRotateYMatrix(particle.rotation.y));
        rotateMatrix = mul(rotateMatrix, MakeRotateZMatrix(particle.rotation.z));
    }

    float4x4 world = mul(mul(MakeScaleMatrix(scale), rotateMatrix), MakeTranslateMatrix(particle.position));

    TransformationMatrix result;
    result.world = world;
    gInstanceMatrices[index] = result;
}
