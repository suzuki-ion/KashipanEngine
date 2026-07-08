#include "../Common/Camera3D.hlsli"

cbuffer GridCB : register(b1) {
	float gFadeDistance;
	float3 gGridPad;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float3 worldPosition : WORLDPOSITION0;
};

// Blender風の無限グリッド。fwidth による画面空間微分でアンチエイリアスするため、
// 遠方でもモアレやジャギーが出ず、距離に応じてフェードアウトするので境界も目立たない。
float GridLine(float2 coord, float cellSize) {
	float2 c = coord / cellSize;
	float2 deriv = fwidth(c) + 1e-6;
	float2 g = abs(frac(c - 0.5) - 0.5) / deriv;
	float lineDist = min(g.x, g.y);
	return 1.0 - saturate(lineDist);
}

float4 main(VSOutput input) : SV_Target0 {
	float2 coord = input.worldPosition.xz;

	// 細かいマス目と粗いマス目の2段階を合成する
	float smallGrid = GridLine(coord, 1.0);
	float largeGrid = GridLine(coord, 10.0);

	float dist = length(input.worldPosition - gCamera3D.eyePosition.xyz);
	float fade = saturate(1.0 - dist / max(1.0, gFadeDistance));
	fade *= fade;

	float3 baseColor = lerp(float3(0.32, 0.32, 0.32), float3(0.55, 0.55, 0.55), largeGrid);
	float alpha = saturate(max(smallGrid * 0.6, largeGrid)) * fade;

	// X軸(赤)・Z軸(青)を強調表示する
	float axisAA = fwidth(coord.y) * 1.5 + 1e-4;
	if (abs(coord.y) < axisAA) {
		baseColor = float3(0.85, 0.25, 0.3);
		alpha = max(alpha, fade);
	}
	axisAA = fwidth(coord.x) * 1.5 + 1e-4;
	if (abs(coord.x) < axisAA) {
		baseColor = float3(0.25, 0.4, 0.85);
		alpha = max(alpha, fade);
	}

	clip(alpha - 0.001);
	return float4(baseColor, alpha);
}
