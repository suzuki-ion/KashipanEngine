// カメラ動作由来のモーションブラー用、スクリーンスペース速度再構成パス。
// オブジェクトごとの速度G-bufferは持たず、深度から再構成したワールド座標を
// 前フレームのView-Projectionへ再投影することで、カメラの移動・回転による
// 見かけ上の移動量のみを速度として求める（オブジェクト自身の動きには追従しない）。
#include "FullScreenTriangle.hlsli"
#include "MotionBlurVelocityCB.hlsli"

Texture2D gDepthTexture : register(t0);
SamplerState gDepthSampler : register(s0);

/// @brief 速度0（見た目上ブラー無し）
static const float2 kZeroVelocity = float2(0.0f, 0.0f);

float LinearizeDepth(float nonLinearDepth) {
	return (gNearClip * gFarClip) / (gFarClip - nonLinearDepth * (gFarClip - gNearClip));
}

float2 main(VSOutput input) : SV_Target0 {
	float rawDepth = gDepthTexture.Sample(gDepthSampler, input.uv).r;
	if (rawDepth >= 1.0f) {
		// 背景（遠平面）は速度0扱いにする
		return kZeroVelocity;
	}

	// ファークリップ付近は深度→ワールド座標の再構成が数値的に不安定になりやすいため、遠景は速度0扱いにする
	const float linearDepth = LinearizeDepth(rawDepth);
	if (linearDepth >= gFarClip * 0.95f) {
		return kZeroVelocity;
	}

	float2 ndcXY = input.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clipPos = float4(ndcXY, rawDepth, 1.0f);
	// 現フレームの逆View-Projectionでホモジニアスなワールド座標（w != 1）を求める。
	// ここで xyz/w によるデホモジニアス化（実座標への変換）を行わず、そのまま前フレームの
	// View-Projectionへ再投影する（＝2回の透視除算の間に正規化を挟まない）。
	// 透視除算はスケール不変（w倍しても最終結果は変わらない）ため数学的には等価だが、
	// 「デホモジニアス化→再度別行列で変換」という経路は特に近距離・遠距離で w が
	// 非常に小さく/大きくなる画素において丸め誤差が拡大されやすく、静止カメラでも
	// 場所によって誤った方向のブラーが出る原因になっていた
	float4 worldPos4 = mul(clipPos, gInvViewProjection);
	float4 prevClipPos = mul(worldPos4, gPrevViewProjection);
	// wが0以下（前フレームのカメラの背面/発散）の場合は再投影が無効なので速度0扱いにする
	if (prevClipPos.w <= 1e-5f) {
		return kZeroVelocity;
	}
	float2 prevNdc = prevClipPos.xy / prevClipPos.w;
	float2 prevUv = prevNdc * float2(0.5f, -0.5f) + 0.5f;

	float2 velocityUv = input.uv - prevUv;
	if (any(isnan(velocityUv)) || any(isinf(velocityUv))) {
		return kZeroVelocity;
	}
	// カメラ動作由来の速度は通常1フレームで画面の何割も動くことはないため、
	// 数値誤差由来の異常値がブラー方向を狂わせないよう妥当な範囲にクランプする
	// （速度バッファはfloatフォーマットのため、UNORMのようなエンコード/デコードは不要）
	return clamp(velocityUv, -0.25f, 0.25f);
}
