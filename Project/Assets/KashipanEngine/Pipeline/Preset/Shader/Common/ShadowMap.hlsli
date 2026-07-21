#pragma once
#include "Camera3D.hlsli"

// シャドウマッピング
// 影を生成する全ライトのシャドウマップを1つの Texture2DArray にまとめて保持する。
// - Directional        : 4スライス（カスケード。カメラ視錐台の分割距離で選択）
// - Spot / Disc / Rect  : 1スライス（ライトからの透視投影。Disc/Rectは片面発光を広角FOVで近似）
// - Point / Sphere / Tube : 6スライス（キューブ6面。+X,-X,+Y,-Y,+Z,-Z の順。Tubeは中心点からの近似）
// 各ライトの使用スライスは gShadowLights[].params.y（先頭スライス番号）から連続で並ぶ。
//
// 半影のソフト化（PCSS）: 各ライトの pcssParams.x にワールド単位の光源サイズを持たせ、0より大きい場合は
// ブロッカーサーチ＋可変半径PCFで光源サイズに応じたソフトシャドウを、0の場合は従来通りの固定3x3 PCFを行う。

#define KE_MAX_SHADOW_LIGHTS 16
#define KE_SHADOW_CASCADE_COUNT 4

struct ShadowLightData {
	// Directional: 0..3=カスケード / Spot,Disc,Rect: 0のみ / Point,Sphere,Tube: 0..5=キューブ6面
	float4x4 viewProjections[6];
	float4 cascadeSplits;     // 各カスケードの適用終端（カメラビュー空間の深度。Directionalのみ使用）
	float4 params;            // x: 1テクセルのUVサイズ / y: 先頭スライス番号 / z: ライト種別 / w: 透視投影の深度バイアス係数
	float4 cascadeBiasScales; // カスケードごとの深度バイアス係数（Directionalのみ使用）
	float4 pcssParams;        // x: 光源サイズ（ワールド単位、0=硬い影）/ yzw: 予約
};

cbuffer ShadowMapConstants : register(b10) {
	ShadowLightData gShadowLights[KE_MAX_SHADOW_LIGHTS];
	uint gShadowLightCount;
};

Texture2DArray gShadowMaps : register(t7);
SamplerComparisonState gShadowSamplerCmp : register(s1);
// PCSSのブロッカーサーチ用（比較無しで生の深度値を読む点サンプラー）
SamplerState gShadowSamplerPoint : register(s2);

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
/// @return 2.5（正面）〜 9.5（すれすれ）テクセル相当の係数
inline float ShadowSlopeFactor(float3 normal, float3 toLightDir) {
	float slope = 1.0f - saturate(dot(normalize(normal), normalize(toLightDir)));
	return 2.5f + 7.0f * slope;
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

/// @brief Vogel disk（黄金角スパイラル）で単位円内に均等分布したサンプルオフセットを生成する
/// @details 外部データ（Poisson disk等の固定テーブル）を持たずに再現可能なサンプルパターンとして使う
inline float2 VogelDiskSample(int index, int count, float rotation) {
	const float kGoldenAngle = 2.39996323f; // ラジアン
	float r = sqrt((index + 0.5f) / count);
	float theta = index * kGoldenAngle + rotation;
	float s, c;
	sincos(theta, s, c);
	return float2(r * c, r * s);
}

/// @brief PCSS: ブロッカーサーチ＋半影サイズ推定＋可変半径PCFで光源サイズに応じたソフトシャドウを求める
/// @param lightSize ワールド単位の光源サイズ（0の場合は呼び出し側で固定3x3 PCFにフォールバックする）
inline float ShadowPcss(float slice, float2 uv, float depthRef, float2 texel, float lightSize, bool isPerspective) {
	// 探索・フィルタ半径のUVスケール（経験的な係数。光源サイズが大きいほど広い範囲を探索・平滑化する）
	const int kSearchTaps = 12;
	const int kFilterTaps = 12;
	float baseRadiusUv = lightSize * texel.x * 6.0f;
	float searchRadiusUv = clamp(baseRadiusUv, texel.x, texel.x * 24.0f);

	float blockerSum = 0.0f;
	int blockerCount = 0;
	[unroll]
	for (int i = 0; i < kSearchTaps; ++i) {
		float2 offset = VogelDiskSample(i, kSearchTaps, 0.0f) * searchRadiusUv;
		float sampleDepth = gShadowMaps.SampleLevel(gShadowSamplerPoint, float3(uv + offset, slice), 0).r;
		if (sampleDepth < depthRef) {
			blockerSum += sampleDepth;
			++blockerCount;
		}
	}

	if (blockerCount == 0) {
		return 1.0f; // 探索範囲内にブロッカーが無い＝影なし
	}
	float avgBlockerDepth = blockerSum / blockerCount;

	// 半影サイズ: 透視投影は (受光深度-ブロッカー深度)/ブロッカー深度 の相似則、正射影は簡易的な線形スケールを使う
	float penumbraRatio = isPerspective
		? saturate((depthRef - avgBlockerDepth) / max(avgBlockerDepth, 1e-4f))
		: saturate((depthRef - avgBlockerDepth) * 4.0f);
	float filterRadiusUv = clamp(baseRadiusUv * penumbraRatio, texel.x, texel.x * 24.0f);

	float sum = 0.0f;
	[unroll]
	for (int j = 0; j < kFilterTaps; ++j) {
		float2 offset = VogelDiskSample(j, kFilterTaps, 1.5707963f) * filterRadiusUv;
		sum += gShadowMaps.SampleCmpLevelZero(gShadowSamplerCmp, float3(uv + offset, slice), depthRef);
	}
	return lerp(0.5f, 1.0f, saturate(sum / kFilterTaps));
}

/// 指定のビュー射影行列でワールド座標を射影し、指定スライスから影係数を求める
/// @details 深度バイアスは「その位置での1テクセルのワールドサイズ」に比例した値をNDC深度へ換算して使う。
///          定数のNDCバイアスだとライトから離れるほどワールド空間でのバイアスが巨大化して
///          影が浮いてしまう（ピーターパン現象）ため、常にテクセル数基準のごく小さな値になるようにする。
///          lightSizeが0より大きい場合はPCSSで、0の場合は従来通りの固定3x3 PCFで影係数を求める
/// @param slopeFactor ShadowSlopeFactor() の結果（バイアスのテクセル数係数）
/// @param isPerspective 透視投影（Spot/Point/Sphere/Disc/Rect/Tube）かどうか
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
	float depthRef = ndc.z - bias;

	float lightSize = data.pcssParams.x;
	if (lightSize > 0.0f) {
		return ShadowPcss((float)slice, uv, depthRef, data.params.xx, lightSize, isPerspective);
	}
	float pcf = ShadowPcf3x3((float)slice, uv, depthRef, data.params.xx);
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

/// Spot/Disc/Rectライトの影係数（片面発光の広角単一透視投影。Disc/Rectはこの関数をそのまま再利用する）
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

/// Point/Sphere/Tubeライトの影係数（ライトからの方向でキューブ面を選択する。Sphereは中心、Tubeは中点を渡して再利用する）
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
