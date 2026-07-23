// スクリーンスペース・アンビエントオクルージョン（SSAO）計算パス
// このエンジンはフォワードレンダリングで法線バッファ（G-buffer）を持たないため、
// 深度バッファのみから「ワールド座標」と「法線」をスクリーンスペースで再構成して使う。
// 半球サンプルはハッシュ関数から手続き的に生成し、固定のPoisson/ノイズテクスチャは持たない。
#include "FullScreenTriangle.hlsli"
#include "AmbientOcclusionCB.hlsli"

Texture2D gDepthTexture : register(t0);
SamplerState gDepthSampler : register(s0);

static const float kPi = 3.14159265f;

/// @brief 深度値（0=近平面, 1=遠平面）とUVからワールド座標を再構成する
float3 ReconstructWorldPos(float2 uv, float rawDepth) {
	float2 ndcXY = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clipPos = float4(ndcXY, rawDepth, 1.0f);
	float4 worldPos = mul(clipPos, gInvViewProjection);
	return worldPos.xyz / worldPos.w;
}

/// @brief 半球（+Z方向）内のサンプルベクトルをハッシュ関数から手続き的に生成する
/// @details 固定のサンプルテーブルを持たず、インデックス・回転シードから毎回再現可能に生成する
float3 HemisphereSample(uint index, uint count, float seed) {
	float2 h = float2(index, seed);
	float u1 = frac(sin(dot(h, float2(12.9898f, 78.233f))) * 43758.5453f);
	float u2 = frac(sin(dot(h, float2(39.3468f, 11.135f))) * 24634.6345f);
	float r = sqrt(u1);
	float theta = 2.0f * kPi * u2;
	float x = r * cos(theta);
	float y = r * sin(theta);
	float z = sqrt(max(0.0f, 1.0f - u1));
	// サンプルインデックスが進むほど半径方向へ広げ、近距離の遮蔽をより密にサンプリングする
	float t = (float)(index + 1) / (float)count;
	float scale = lerp(0.1f, 1.0f, t * t);
	return float3(x, y, z) * scale;
}

float4 main(VSOutput input) : SV_Target0 {
	float rawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (rawDepth >= 1.0f) {
		return float4(1.0f, 1.0f, 1.0f, 1.0f); // 背景（遠平面）は遮蔽なし
	}

	float3 worldPos = ReconstructWorldPos(input.uv, rawDepth);

	// 法線を深度から再構成する（頂点法線バッファが無いための近似）。
	// ddx/ddyは常に片側固定の隣接ピクセルを機械的に見るため、物体のシルエット境界（背景や
	// 手前の別ジオメトリとの深度の飛び）をまたぐピクセルでは全く無関係な深度差から接ベクトルを
	// 作ってしまい、輪郭に沿った法線の破綻（AOのハロー状のノイズ）が出やすい。
	// 左右・上下それぞれで中心により近い深度を持つ側だけを採用することで、境界をまたがない
	// 側から接ベクトルを求める（同じ面上にある可能性が高い側を選ぶ）
	float2 texel = gTexelSize;
	float rawDepthL = gDepthTexture.Sample(gDepthSampler, input.uv - float2(texel.x, 0.0f)).r;
	float rawDepthR = gDepthTexture.Sample(gDepthSampler, input.uv + float2(texel.x, 0.0f)).r;
	float rawDepthU = gDepthTexture.Sample(gDepthSampler, input.uv - float2(0.0f, texel.y)).r;
	float rawDepthD = gDepthTexture.Sample(gDepthSampler, input.uv + float2(0.0f, texel.y)).r;

	float3 worldPosL = ReconstructWorldPos(input.uv - float2(texel.x, 0.0f), rawDepthL);
	float3 worldPosR = ReconstructWorldPos(input.uv + float2(texel.x, 0.0f), rawDepthR);
	float3 worldPosU = ReconstructWorldPos(input.uv - float2(0.0f, texel.y), rawDepthU);
	float3 worldPosD = ReconstructWorldPos(input.uv + float2(0.0f, texel.y), rawDepthD);

	float3 tangentX = (abs(rawDepthL - rawDepth) < abs(rawDepthR - rawDepth)) ? (worldPos - worldPosL) : (worldPosR - worldPos);
	float3 tangentY = (abs(rawDepthU - rawDepth) < abs(rawDepthD - rawDepth)) ? (worldPos - worldPosU) : (worldPosD - worldPos);

	float3 normal = normalize(cross(tangentY, tangentX));
	float3 toCamera = normalize(gCameraWorldPosition - worldPos);
	if (dot(normal, toCamera) < 0.0f) {
		normal = -normal;
	}

	// TBNを構築する（ピクセル単位の回転はハッシュシードで与える）
	float3 tangent = normalize(tangentX - normal * dot(tangentX, normal));
	if (!any(tangent)) {
		tangent = normalize(cross(normal, float3(0.0f, 1.0f, 0.0f)));
	}
	float3 bitangent = cross(normal, tangent);
	float3x3 tbn = float3x3(tangent, bitangent, normal);

	float pixelSeed = gRotationSeed + frac(sin(dot(input.pos.xy, float2(12.9898f, 78.233f))) * 43758.5453f) * 100.0f;

	float occlusion = 0.0f;
	for (uint i = 0; i < gSampleCount; ++i) {
		float3 sampleOffset = mul(HemisphereSample(i, gSampleCount, pixelSeed), tbn);
		float3 samplePos = worldPos + sampleOffset * gRadius;

		float4 sampleClip = mul(float4(samplePos, 1.0f), gViewProjection);
		if (sampleClip.w <= 0.0f) {
			continue;
		}
		float3 sampleNdc = sampleClip.xyz / sampleClip.w;
		float2 sampleUv = sampleNdc.xy * float2(0.5f, -0.5f) + 0.5f;
		if (sampleUv.x < 0.0f || sampleUv.x > 1.0f || sampleUv.y < 0.0f || sampleUv.y > 1.0f) {
			continue;
		}

		float sampleSceneDepth = gDepthTexture.Sample(gDepthSampler, sampleUv).r;
		if (sampleSceneDepth >= 1.0f) {
			continue;
		}
		float3 sampleSceneWorldPos = ReconstructWorldPos(sampleUv, sampleSceneDepth);

		float distToSampleCandidate = distance(gCameraWorldPosition, samplePos);
		float distToActualSurface = distance(gCameraWorldPosition, sampleSceneWorldPos);
		// 実際のジオメトリがサンプル点より手前にある場合＝遮蔽されている
		if (distToActualSurface < distToSampleCandidate - gBias) {
			// 遮蔽物が半径から大きく離れている場合は無関係な奥のジオメトリとして減衰させる
			float rangeCheck = saturate(1.0f - distance(worldPos, sampleSceneWorldPos) / gRadius);
			occlusion += rangeCheck;
		}
	}
	occlusion /= max(gSampleCount, 1u);

	float ao = saturate(1.0f - occlusion * gIntensity);
	ao = pow(ao, gPower);
	return float4(ao, ao, ao, 1.0f);
}
