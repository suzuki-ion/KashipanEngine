#include "../Common/Camera3D.hlsli"

cbuffer GridCB : register(b1) {
	float gFadeDistance;
	float3 gGridPad;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float3 worldPosition : WORLDPOSITION0;
};

// カメラの周囲を覆う大きな板ポリゴン（XZ平面、Y=0）を SV_VertexID から手続き的に生成する。
// 頂点バッファは使用しない（PostEffectのフルスクリーン三角形と同じ手法）。
VSOutput main(uint vid : SV_VertexID) {
	static const float2 kCorners[6] = {
		float2(-1.0, -1.0), float2(1.0, -1.0), float2(1.0, 1.0),
		float2(-1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0)
	};
	float2 corner = kCorners[vid];
	float extent = max(1.0, gFadeDistance) * 1.5;
	float3 cameraPosition = gCamera3D.eyePosition.xyz;
	float3 worldPos = float3(cameraPosition.x + corner.x * extent, 0.0, cameraPosition.z + corner.y * extent);

	VSOutput output;
	output.worldPosition = worldPos;
	output.position = mul(float4(worldPos, 1.0), gCamera3D.viewProjection);
	return output;
}
