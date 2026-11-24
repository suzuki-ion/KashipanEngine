// GTAO（Ground-Truth Ambient Occlusion）を半分の解像度で実行し、空間方向の処理のみを行う。
// 深度バッファを高さ場として扱う。スクリーン空間の各スライスで
// 求めた 2 つの地平線角度から、可視率を解析的に積分する。
#include "FullScreenTriangle.hlsli"
#include "GTAOCB.hlsli"

Texture2D gDepthTexture : register(t0);
SamplerState gDepthSampler : register(s0);

static const float kPi = 3.14159265359f;
static const float kHalfPi = 1.57079632679f;

float3 ReconstructWorldPos(float2 uv, float rawDepth) {
	float2 ndcXY = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 worldPos = mul(float4(ndcXY, rawDepth, 1.0f), gInvViewProjection);
	return worldPos.xyz / worldPos.w;
}

float2 ProjectWorldToUv(float3 worldPos) {
	float4 clipPos = mul(float4(worldPos, 1.0f), gViewProjection);
	float2 ndc = clipPos.xy / max(clipPos.w, 1e-6f);
	return ndc * float2(0.5f, -0.5f) + 0.5f;
}

float InterleavedGradientNoise(float2 pixel) {
	return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

void ReconstructNormalAndTangents(float2 uv, float rawDepth, float3 center,
	out float3 normal, out float3 tangentX, out float3 tangentY) {
	float rawL = gDepthTexture.Sample(gDepthSampler, uv - float2(gDepthTexelSize.x, 0.0f)).r;
	float rawR = gDepthTexture.Sample(gDepthSampler, uv + float2(gDepthTexelSize.x, 0.0f)).r;
	float rawU = gDepthTexture.Sample(gDepthSampler, uv - float2(0.0f, gDepthTexelSize.y)).r;
	float rawD = gDepthTexture.Sample(gDepthSampler, uv + float2(0.0f, gDepthTexelSize.y)).r;

	float3 posL = ReconstructWorldPos(uv - float2(gDepthTexelSize.x, 0.0f), rawL);
	float3 posR = ReconstructWorldPos(uv + float2(gDepthTexelSize.x, 0.0f), rawR);
	float3 posU = ReconstructWorldPos(uv - float2(0.0f, gDepthTexelSize.y), rawU);
	float3 posD = ReconstructWorldPos(uv + float2(0.0f, gDepthTexelSize.y), rawD);

	tangentX = (abs(rawL - rawDepth) < abs(rawR - rawDepth)) ? (center - posL) : (posR - center);
	tangentY = (abs(rawU - rawDepth) < abs(rawD - rawDepth)) ? (center - posU) : (posD - center);
	normal = normalize(cross(tangentY, tangentX));
	float3 toCamera = normalize(gCameraWorldPosition - center);
	if (dot(normal, toCamera) < 0.0f) normal = -normal;
}

float IntegrateSliceVisibility(float horizonNegativeCos, float horizonPositiveCos,
	float projectedLength, float n, float cosN) {
	// アルゴリズム 1。投影した面法線の両側にある 2 つの可視弧を積分する。
	// 画面上の負方向が h0、正方向が h1 に対応する。
	float h0 = -acos(clamp(horizonNegativeCos, -1.0f, 1.0f));
	float h1 = acos(clamp(horizonPositiveCos, -1.0f, 1.0f));
	h0 = n + clamp(h0 - n, -kHalfPi, kHalfPi);
	h1 = n + clamp(h1 - n, -kHalfPi, kHalfPi);
	float sinN = sin(n);
	float arc0 = (cosN + 2.0f * h0 * sinN - cos(2.0f * h0 - n)) * 0.25f;
	float arc1 = (cosN + 2.0f * h1 * sinN - cos(2.0f * h1 - n)) * 0.25f;
	return saturate(projectedLength * (arc0 + arc1));
}

float4 main(VSOutput input) : SV_Target0 {
	float rawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (rawDepth >= 1.0f) return 1.0f.xxxx;

	float3 worldPos = ReconstructWorldPos(input.uv, rawDepth);
	float3 normal;
	float3 tangentX;
	float3 tangentY;
	ReconstructNormalAndTangents(input.uv, rawDepth, worldPos, normal, tangentX, tangentY);

	float3 viewDirection = normalize(gCameraWorldPosition - worldPos);
	// GTAO のスライス方向はカメラの画面上で定義する。
	// 再構成した面の接線を使うと、斜面で画面全体が誤って暗くなる。
	float3 screenX = ReconstructWorldPos(input.uv + float2(gDepthTexelSize.x, 0.0f), rawDepth) - worldPos;
	float3 screenY = ReconstructWorldPos(input.uv + float2(0.0f, gDepthTexelSize.y), rawDepth) - worldPos;
	if (dot(screenX, screenX) < 1e-8f || dot(screenY, screenY) < 1e-8f) return 1.0f.xxxx;
	screenX = normalize(screenX);
	screenY = normalize(screenY);

	float noise = InterleavedGradientNoise(floor(input.pos.xy));
	float rotation = noise * kPi;
	float visibility = 0.0f;
	// 深度精度を補う GTAO のバイアスとして、中心点をカメラ側にずらす。
	// これにより、法線方向にずらした場合に起こり得る平面の地平線の傾きを防ぐ。
	float3 biasedCenter = worldPos + viewDirection * gBias;

	[loop]
	for (uint directionIndex = 0; directionIndex < gDirectionCount; ++directionIndex) {
		float phi = rotation + (float(directionIndex) + 0.5f) * (kPi / float(gDirectionCount));
		float2 directionUv = float2(cos(phi), sin(phi));
		float3 screenDirection = normalize(screenX * directionUv.x + screenY * directionUv.y);
		float3 sliceDirection = screenDirection - viewDirection * dot(screenDirection, viewDirection);
		if (dot(sliceDirection, sliceDirection) < 1e-8f) {
			visibility += 1.0f;
			continue;
		}
		sliceDirection = normalize(sliceDirection);
		float3 sliceAxis = normalize(cross(sliceDirection, viewDirection));
		float3 projectedNormal = normal - sliceAxis * dot(normal, sliceAxis);
		float projectedLength = length(projectedNormal);
		if (projectedLength < 1e-5f) {
			visibility += 1.0f;
			continue;
		}
		float3 projectedNormalDirection = projectedNormal / projectedLength;
		float signN = (dot(sliceDirection, projectedNormalDirection) < 0.0f) ? -1.0f : 1.0f;
		float cosN = saturate(dot(projectedNormalDirection, viewDirection));
		float n = signN * acos(cosN);

		float2 radiusUvPosition = ProjectWorldToUv(worldPos + sliceDirection * gRadius);
		float radiusUv = min(length(radiusUvPosition - input.uv), 0.25f);
		float minimumRadiusUv = max(gAOTexelSize.x, gAOTexelSize.y) * 1.5f;
		if (radiusUv < minimumRadiusUv) {
			visibility += 1.0f;
			continue;
		}

		// サンプルがない場合は、地平線が法線を軸とする可視半球より下にあるとみなす。
		// 余弦を一律に -1 とする場合と異なり、遮蔽のない平面では AO=1 が保証される。
		float horizons[2] = { cos(n - kHalfPi), cos(n + kHalfPi) };
		[unroll]
		for (uint side = 0; side < 2; ++side) {
			float sideSign = (side == 0) ? -1.0f : 1.0f;
			[loop]
			for (uint stepIndex = 0; stepIndex < gStepCount; ++stepIndex) {
				float stepJitter = frac(noise + float(stepIndex) * 0.61803398875f);
				float stepFraction = (float(stepIndex) + 0.35f + stepJitter * 0.3f) / float(gStepCount);
				stepFraction = max(pow(stepFraction, 2.0f), minimumRadiusUv / radiusUv);
				float2 sampleUv = input.uv + directionUv * sideSign * radiusUv * stepFraction;
				if (any(sampleUv <= 0.0f) || any(sampleUv >= 1.0f)) continue;

				float sampleDepth = gDepthTexture.Sample(gDepthSampler, sampleUv).r;
				if (sampleDepth >= 1.0f) continue;
				float3 sampleWorldPos = ReconstructWorldPos(sampleUv, sampleDepth);
				float3 horizonVector = sampleWorldPos - biasedCenter;
				float distanceToSample = length(horizonVector);
				if (distanceToSample <= 1e-5f || distanceToSample > gRadius) continue;

				float horizonCos = dot(horizonVector / distanceToSample, viewDirection);
				float falloffStart = gRadius * 0.8f;
				float rangeWeight = saturate((gRadius - distanceToSample) / max(gRadius - falloffStart, 1e-5f));
				float attenuatedHorizon = lerp(horizons[side], horizonCos, rangeWeight);
				horizons[side] = max(horizons[side], attenuatedHorizon);
			}
		}

		projectedLength = lerp(projectedLength, 1.0f, 0.05f);
		visibility += IntegrateSliceVisibility(horizons[0], horizons[1], projectedLength, n, cosN);
	}

	visibility = saturate(visibility / max(float(gDirectionCount), 1.0f));

	// 遮蔽量に対して強度を適用し、完全に可視な面が暗くならないようにする。
	// 小さなしきい値で微小な残留遮蔽を除去し、
	// 半解像度でのサンプリングや再構成誤差による薄いかすみを抑える。
	static const float kResidualOcclusion = 0.03f;
	float occlusion = saturate((1.0f - visibility - kResidualOcclusion) / (1.0f - kResidualOcclusion));

	// Power は遮蔽のコントラストを調整する。1 より大きい場合は、
	// 基準値より弱い AO を抑え、強い AO を強調する。1 未満では AO を弱める。
	static const float kOcclusionPivot = 0.20f;
	if (gPower >= 1.0f)
	{
		occlusion = saturate((occlusion - kOcclusionPivot) * gPower + kOcclusionPivot);
	}
	else
	{
		occlusion *= gPower;
	}

	float ao = 1.0f - saturate(occlusion * gIntensity);
	return float4(ao, ao, ao, 1.0f);
}
