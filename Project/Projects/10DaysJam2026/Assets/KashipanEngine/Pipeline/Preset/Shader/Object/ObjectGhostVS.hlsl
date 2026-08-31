// Prefabのシーンビューへのドラッグ配置プレビュー（ゴーストメッシュ）用の頂点シェーダー。
// ObjectOutlineVS.hlslと異なり法線方向への押し出しは行わず、通常通りワールド→クリップ空間へ変換するだけ
#include "Object3D.hlsli"
#include "ObjectGhost.hlsli"

// 頂点バッファの実体（ResourceContainer::MeshVertex）は position/texcoord/normal/tangent の順で
// 詰まっている。AutoInputLayoutFromVSはD3D12_APPEND_ALIGNED_ELEMENTでこの構造体の宣言順に
// オフセットを詰めるため、texcoord/normal/tangentを使わなくても同じ順番・型で宣言しておく必要がある
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

GhostVSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
	GhostVSOutput output;
	float4x4 world = gTransformationMatrices[instanceId].world;
	float3 worldPosition = mul(input.position, world).xyz;

	output.position = mul(float4(worldPosition, 1.0f), gCamera3D.viewProjection);
	output.instanceId = instanceId;
	return output;
}
