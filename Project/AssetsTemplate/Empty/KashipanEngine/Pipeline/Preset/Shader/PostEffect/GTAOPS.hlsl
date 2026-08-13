// Ground-Truth Ambient Occlusion: half-resolution, spatial-only implementation.
// The depth buffer is treated as a height field. Visibility is integrated analytically
// from the two horizon angles found for each screen-space slice.
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
	// Algorithm 1: integrate the two visible arcs around the projected surface normal.
	// The negative screen direction maps to h0 and the positive direction to h1.
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
	// A GTAO slice is a camera-screen direction, not a direction on the reconstructed
	// surface. Keeping the surface tangents here incorrectly darkens the whole image on slopes.
	float3 screenX = ReconstructWorldPos(input.uv + float2(gDepthTexelSize.x, 0.0f), rawDepth) - worldPos;
	float3 screenY = ReconstructWorldPos(input.uv + float2(0.0f, gDepthTexelSize.y), rawDepth) - worldPos;
	if (dot(screenX, screenX) < 1e-8f || dot(screenY, screenY) < 1e-8f) return 1.0f.xxxx;
	screenX = normalize(screenX);
	screenY = normalize(screenY);

	float noise = InterleavedGradientNoise(floor(input.pos.xy));
	float rotation = noise * kPi;
	float visibility = 0.0f;
	// Move the center toward the camera. This is the GTAO depth-precision bias and does
	// not tilt the horizon of a flat surface as a normal-direction offset can.
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

		// A missing sample means the horizon is below the normal's visible hemisphere,
		// not a fixed -1 cosine. This guarantees AO=1 on an unoccluded flat surface.
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

	// Work in occlusion space so intensity never darkens fully visible surfaces.
	// A small residual threshold also removes the low-level haze caused by
	// half-resolution sampling and reconstruction error.
	static const float kResidualOcclusion = 0.03f;
	float occlusion = saturate((1.0f - visibility - kResidualOcclusion) / (1.0f - kResidualOcclusion));

	// Power is an occlusion contrast control. Above 1 it suppresses weak AO and
	// emphasizes strong AO around the pivot; below 1 it simply softens AO.
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
