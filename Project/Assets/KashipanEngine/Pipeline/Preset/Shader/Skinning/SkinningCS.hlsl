// SkinnedMeshRenderer用のGPUスキニングComputeシェーダー
// バインドポーズの頂点(gSourceVertices)へまずBlendShape(モーフターゲット)の差分を加重合成し、
// その結果を頂点ごとのボーンインデックス・ウェイト(gSkinWeights)と現フレームのボーン行列パレット
// (gBoneMatrices)を使って線形ブレンドスキニングし、結果をgOutputVerticesへ書き込む
// （後続の描画パスで頂点バッファとして使用される）。

cbuffer SkinningConstants : register(b0)
{
    uint gVertexCount;
    uint gMaxBonesPerVertex;
    uint gBlendShapeCount;
    uint gPadding;
};

struct SkinVertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

struct SkinWeight
{
    uint4 boneIndices;
    float4 boneWeights;
};

struct BlendShapeDelta
{
    float3 deltaPosition;
    float3 deltaNormal;
};

StructuredBuffer<SkinVertex> gSourceVertices : register(t0);
StructuredBuffer<SkinWeight> gSkinWeights : register(t1);
StructuredBuffer<float4x4> gBoneMatrices : register(t2);
// [shapeIndex * gVertexCount + vertexIndex] で参照する連結バッファ
StructuredBuffer<BlendShapeDelta> gBlendShapeDeltas : register(t3);
// BlendShapeごとのウェイト（0～100、Unityと同じ表現）
StructuredBuffer<float> gBlendShapeWeights : register(t4);
RWStructuredBuffer<SkinVertex> gOutputVertices : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint vertexIndex = dispatchThreadID.x;
    if (vertexIndex >= gVertexCount)
    {
        return;
    }

    SkinVertex src = gSourceVertices[vertexIndex];

    // BlendShape（モーフターゲット）をバインドポーズへ加重合成してから通常のスキニングを行う
    float3 blendedPosition = src.position.xyz;
    float3 blendedNormal = src.normal;
    for (uint b = 0; b < gBlendShapeCount; ++b)
    {
        float weight = gBlendShapeWeights[b] * 0.01f;
        if (weight == 0.0f)
        {
            continue;
        }
        BlendShapeDelta delta = gBlendShapeDeltas[b * gVertexCount + vertexIndex];
        blendedPosition += delta.deltaPosition * weight;
        blendedNormal += delta.deltaNormal * weight;
    }

    SkinWeight w = gSkinWeights[vertexIndex];

    uint boneIndices[4] = { w.boneIndices.x, w.boneIndices.y, w.boneIndices.z, w.boneIndices.w };
    float boneWeights[4] = { w.boneWeights.x, w.boneWeights.y, w.boneWeights.z, w.boneWeights.w };

    uint boneCount = min(gMaxBonesPerVertex, 4);

    float3 skinnedPosition = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    for (uint i = 0; i < boneCount; ++i)
    {
        float weight = boneWeights[i];
        if (weight <= 0.0f)
        {
            continue;
        }
        float4x4 boneMatrix = gBoneMatrices[boneIndices[i]];
        skinnedPosition += mul(float4(blendedPosition, 1.0f), boneMatrix).xyz * weight;
        skinnedNormal += mul(blendedNormal, (float3x3) boneMatrix) * weight;
        totalWeight += weight;
    }

    SkinVertex outVertex;
    if (totalWeight > 0.0001f)
    {
        outVertex.position = float4(skinnedPosition / totalWeight, 1.0f);
        outVertex.normal = normalize(skinnedNormal);
    }
    else
    {
        // ウェイトが1つも無い頂点はBlendShape適用後のバインドポーズのまま出力する
        outVertex.position = float4(blendedPosition, 1.0f);
        outVertex.normal = normalize(blendedNormal);
    }
    outVertex.texcoord = src.texcoord;

    gOutputVertices[vertexIndex] = outVertex;
}
