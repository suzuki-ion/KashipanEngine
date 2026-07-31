// 被写界深度: CoCを半径として使うVogel disk方式の可変半径ガザリングブラー
// 近景・遠景どちらのブラーにも同じシェーダーを使い回す（バインドするCoCテクスチャが異なるだけ）。
// gCoCTexture の r チャンネルをこのパス用のCoC(0..1)として読む
// （遠景パスはCoC計算結果のrをそのまま、近景パスはダイレーション後の値をrに詰めて渡す）
#include "FullScreenTriangle.hlsli"
#include "DepthOfFieldBlurCB.hlsli"

Texture2D gTexture : register(t0);
Texture2D gCoCTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gCoCSampler : register(s1);

static const float kGoldenAngle = 2.39996323f; // ラジアン

/// @brief Vogel disk（黄金角スパイラル）で単位円内に均等分布したサンプルオフセットを生成する
/// @details 固定のPoisson disk等のテーブルを持たず、手続き的に再現可能な形で生成する
float2 VogelDiskSample(int index, int count, float rotation) {
	float r = sqrt((index + 0.5f) / count);
	float theta = index * kGoldenAngle + rotation;
	float s, c;
	sincos(theta, s, c);
	return float2(r * c, r * s);
}

float4 main(VSOutput input) : SV_Target0 {
	float coc = gCoCTexture.Sample(gCoCSampler, input.uv).r;
	float4 centerColor = gTexture.Sample(gSampler, input.uv);
	if (coc <= 0.001f) {
		return centerColor;
	}

	float2 radiusUv = coc * gMaxBlurRadiusUv;
	float rotation = gRotationSeed + frac(sin(dot(input.pos.xy, float2(12.9898f, 78.233f))) * 43758.5453f) * 6.2831853f;

	float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (uint i = 0; i < gSampleCount; ++i) {
		float2 offset = VogelDiskSample(i, gSampleCount, rotation) * radiusUv;
		sum += gTexture.Sample(gSampler, input.uv + offset);
	}
	return sum / max(gSampleCount, 1u);
}
