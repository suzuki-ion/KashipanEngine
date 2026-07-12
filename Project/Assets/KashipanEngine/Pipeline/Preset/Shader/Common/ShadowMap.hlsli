#pragma once
#include "Camera3D.hlsli"

// シャドウマッピング
// 影を生成する全ライトのシャドウマップを1つの Texture2DArray にまとめて保持する。
// - Directional : 4スライス（カスケード。カメラ視錐台の分割距離で選択）
// - Spot        : 1スライス（ライトからの透視投影）
// - Point       : 6スライス（キューブ6面。+X,-X,+Y,-Y,+Z,-Z の順）
// 各ライトの使用スライスは gShadowLights[].params.y（先頭スライス番号）から連続で並ぶ。

#define KE_MAX_SHADOW_LIGHTS 16
#define KE_SHADOW_CASCADE_COUNT 4

struct ShadowLightData {
	// Directional: 0..3=カスケード / Spot: 0のみ / Point: 0..5=キューブ6面
	float4x4 viewProjections[6];
	float4 cascadeSplits;     // 各カスケードの適用終端（カメラビュー空間の深度。Directionalのみ使用）
	float4 params;            // x: 1テクセルのUVサイズ / y: 先頭スライス番号 / z: ライト種別 / w: 透視投影の深度バイアス係数
	float4 cascadeBiasScales; // カスケードごとの深度バイアス係数（Directionalのみ使用）
};

cbuffer ShadowMapConstants : register(b10) {
	ShadowLightData gShadowLights[KE_MAX_SHADOW_LIGHTS];
	uint gShadowLightCount;
};

Texture2DArray gShadowMaps : register(t7);
SamplerComparisonState gShadowSamplerCmp : register(s1);

inline float2 ShadowNdcToUv(float3 ndc) {
	float2 uv;
	uv.x = ndc.x * 0.5f + 0.5f;
	uv.y = -ndc.y * 0.5f + 0.5f;
	return uv;
}

inline bool ShadowIsOutside(float2 uv, float depth) {
	return (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f);
}

/// 面がライトに対して浅い角度なほどバイアスを増やす係数（テクセル数相当）を求める
/// @param toLightDir 表面からライトへ向かう方向
/// @return 1.0（正面）〜 4.0（すれすれ）テクセル相当の係数
inline float ShadowSlopeFactor(float3 normal, float3 toLightDir) {
	float slope = 1.0f - saturate(dot(normalize(normal), normalize(toLightDir)));
	return 1.0f + 3.0f * slope;
}

inline float ShadowPcf3x3(float slice, float2 uv, float depthRef, float2 texel) {
	float sum = 0.0f;
	[unroll]
	for (int y = -1; y <= 1; ++y) {
		[unroll]
		for (int x = -1; x <= 1; ++x) {
			sum += gShadowMaps.SampleCmpLevelZero(gShadowSamplerCmp, float3(uv + float2(x, y) * texel, slice), depthRef);
		}
	}
	return sum / 9.0f;
}

/// 指定のビュー射影行列でワールド座標を射影し、指定スライスからPCFで影係数を求める
/// @details 深度バイアスは「その位置での1テクセルのワールドサイズ」に比例した値をNDC深度へ換算して使う。
///          定数のNDCバイアスだとライトから離れるほどワールド空間でのバイアスが巨大化して
///          影が浮いてしまう（ピーターパン現象）ため、常にテクセル数基準のごく小さな値になるようにする
/// @param slopeFactor ShadowSlopeFactor() の結果（バイアスのテクセル数係数）
/// @param isPerspective 透視投影（Spot/Point）かどうか
/// @return 1.0 = 影なし ～ 0.5 = 影（範囲外は 1.0）
inline float ShadowProjectAndSample(ShadowLightData data, uint vpIndex, uint slice, float3 worldPos, float slopeFactor, bool isPerspective) {
	float4 lightClip = mul(float4(worldPos, 1.0f), data.viewProjections[vpIndex]);
	if (lightClip.w <= 0.0f) {
		return 1.0f; // 透視投影の背面
	}
	float3 ndc = lightClip.xyz / lightClip.w;
	float2 uv = ShadowNdcToUv(ndc);
	if (ShadowIsOutside(uv, ndc.z)) {
		return 1.0f;
	}

	float bias;
	if (isPerspective) {
		// 透視投影はNDC深度が非線形（遠方ほど1NDCあたりのワールド距離が大きい）ため、
		// ビュー深度（lightClip.w）で割ることで距離によらずテクセル比例のバイアスになる
		bias = data.params.w * slopeFactor / max(lightClip.w, 1e-3f);
	} else {
		// 正射影はカスケードごとに事前計算した係数（1テクセルのワールドサイズ/深度レンジ）を使う
		bias = data.cascadeBiasScales[vpIndex] * slopeFactor;
	}

	float pcf = ShadowPcf3x3((float)slice, uv, ndc.z - bias, data.params.xx);
	return lerp(0.5f, 1.0f, saturate(pcf));
}

/// Directionalライトの影係数（カメラビュー空間深度でカスケードを選択する）
inline float ComputeDirectionalShadowFactor(uint shadowLightIndex, float3 worldPos, float3 normal, float3 lightDirection) {
	if (shadowLightIndex >= gShadowLightCount) {
		return 1.0f;
	}
	ShadowLightData data = gShadowLights[shadowLightIndex];

	float viewDepth = mul(float4(worldPos, 1.0f), gCamera3D.view).z;
	uint cascade = KE_SHADOW_CASCADE_COUNT - 1;
	[unroll]
	for (uint c = 0; c < KE_SHADOW_CASCADE_COUNT; ++c) {
		if (viewDepth <= data.cascadeSplits[c]) {
			cascade = c;
			break;
		}
	}

	float slopeFactor = ShadowSlopeFactor(normal, -lightDirection);
	uint baseSlice = (uint)data.params.y;
	return ShadowProjectAndSample(data, cascade, baseSlice + cascade, worldPos, slopeFactor, false);
}

/// Spotライトの影係数
/// @param toLightDir 表面からライトへ向かう方向
inline float ComputeSpotShadowFactor(uint shadowLightIndex, float3 worldPos, float3 normal, float3 toLightDir) {
	if (shadowLightIndex >= gShadowLightCount) {
		return 1.0f;
	}
	ShadowLightData data = gShadowLights[shadowLightIndex];
	float slopeFactor = ShadowSlopeFactor(normal, toLightDir);
	uint baseSlice = (uint)data.params.y;
	return ShadowProjectAndSample(data, 0, baseSlice, worldPos, slopeFactor, true);
}

/// Pointライトの影係数（ライトからの方向でキューブ面を選択する）
inline float ComputePointShadowFactor(uint shadowLightIndex, float3 worldPos, float3 normal, float3 lightPosition) {
	if (shadowLightIndex >= gShadowLightCount) {
		return 1.0f;
	}
	ShadowLightData data = gShadowLights[shadowLightIndex];

	// ライトからピクセルへの方向の主軸で面を選択する（C++側の面順 +X,-X,+Y,-Y,+Z,-Z と一致させる）
	float3 v = worldPos - lightPosition;
	float3 a = abs(v);
	uint face;
	if (a.x >= a.y && a.x >= a.z) {
		face = (v.x >= 0.0f) ? 0 : 1;
	} else if (a.y >= a.z) {
		face = (v.y >= 0.0f) ? 2 : 3;
	} else {
		face = (v.z >= 0.0f) ? 4 : 5;
	}

	float slopeFactor = ShadowSlopeFactor(normal, -v);
	uint baseSlice = (uint)data.params.y;
	return ShadowProjectAndSample(data, face, baseSlice + face, worldPos, slopeFactor, true);
}
