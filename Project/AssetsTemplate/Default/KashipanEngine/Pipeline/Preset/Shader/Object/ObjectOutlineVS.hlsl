// 押し出しアウトライン（Inverted Hull）用の頂点シェーダー。頂点をワールド空間法線方向へ
// mat.outlineWidth分押し出してから通常通り射影する。ObjectVS.hlslのObject3Dパスと同じ変換規約
#include "Object3D.hlsli"
#include "ObjectOutline.hlsli"

// 頂点バッファの実体（ResourceContainer::MeshVertex）は position/texcoord/normal/tangent の順で
// 詰まっている。AutoInputLayoutFromVSはD3D12_APPEND_ALIGNED_ELEMENTでこの構造体の宣言順に
// オフセットを詰めるため、texcoord/tangentを使わなくても同じ順番・型で宣言しておく必要がある
// （省略するとnormalが実際にはtexcoordの位置から読まれてしまう）
struct VSInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
	float3 normal : NORMAL0;
	float3 tangent : TANGENT0;
};

struct TransformationMatrix {
	float4x4 world;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);

OutlineVSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
	OutlineVSOutput output;
	float4x4 world = gTransformationMatrices[instanceId].world;
	Material mat = gMaterials[instanceId];

	float3 worldNormal = normalize(mul(input.normal, (float3x3)world));
	float3 worldPosition = mul(input.position, world).xyz + worldNormal * mat.outlineWidth;

	output.position = mul(float4(worldPosition, 1.0f), gCamera3D.viewProjection);
	output.instanceId = instanceId;
	return output;
}
