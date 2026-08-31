#include "FullscreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer FXAACB : register(b0) {
	float2 gInvScreenSize; // (1.0 / width, 1.0 / height)
	float gEdgeThreshold;
	float gEdgeThresholdMin;
	float gStrength;
	// サブピクセル（1ピクセル程度の孤立した明暗・細線）のエイリアシング除去の強さ。0で無効、1で最大
	float gSubpixelBlend;
};

// エッジ端点探索の反復回数（増やすほど浅い角度・長い辺への追従が良くなる代わりに重くなる）
#define FXAA_SEARCH_STEPS 10

// Luma計算（Rec.709）
float Luma(float3 rgb) {
	return dot(rgb, float3(0.2126, 0.7152, 0.0722));
}

float LumaAt(float2 uv) {
	return Luma(gTexture.SampleLevel(gSampler, uv, 0.0).rgb);
}

float4 main(VSOutput input) : SV_TARGET {
	float2 uv = input.uv;
	float2 px = gInvScreenSize;

	float3 rgbM = gTexture.Sample(gSampler, uv).rgb;
	float lumaM = Luma(rgbM);

	// 上下左右4近傍（この時点では対角は見ない。エッジの有無を安く判定するため）
	float lumaN = Luma(gTexture.Sample(gSampler, uv + float2(0.0, -px.y)).rgb);
	float lumaS = Luma(gTexture.Sample(gSampler, uv + float2(0.0, px.y)).rgb);
	float lumaW = Luma(gTexture.Sample(gSampler, uv + float2(-px.x, 0.0)).rgb);
	float lumaE = Luma(gTexture.Sample(gSampler, uv + float2(px.x, 0.0)).rgb);

	float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
	float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
	float lumaRange = lumaMax - lumaMin;

    // エッジが弱ければそのまま返す
	if (lumaRange < max(gEdgeThreshold, lumaMax * gEdgeThresholdMin)) {
		return float4(rgbM, 1.0);
	}

	// 対角4近傍（エッジ方向判定の頑健化と、サブピクセルエイリアシング検出に使う）
	float lumaNW = Luma(gTexture.Sample(gSampler, uv + float2(-px.x, -px.y)).rgb);
	float lumaNE = Luma(gTexture.Sample(gSampler, uv + float2(px.x, -px.y)).rgb);
	float lumaSW = Luma(gTexture.Sample(gSampler, uv + float2(-px.x, px.y)).rgb);
	float lumaSE = Luma(gTexture.Sample(gSampler, uv + float2(px.x, px.y)).rgb);

	// --------- サブピクセル（1ピクセル程度の孤立した明暗・細線）のエイリアシング検出 ---------
	// 通常のエッジ判定（4近傍のみ）ではすり抜けやすい、周囲全体からわずかに浮いた孤立ピクセルを
	// 「3x3近傍の加重平均から中心がどれだけ離れているか」で検出し、後段のオフセットへ加味する
	float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaN + lumaS + lumaW + lumaE) + (lumaNW + lumaNE + lumaSW + lumaSE));
	float subpixelContrast = abs(lumaAverage - lumaM);
	float subpixelBlend = saturate(subpixelContrast / max(lumaRange, 1e-4));
	subpixelBlend = subpixelBlend * subpixelBlend * gSubpixelBlend; // 二乗して弱いコントラストへの過剰反応を抑える

	// --------- エッジ方向の推定（3x3の加重ラプラシアンで、対角成分も加味して頑健にする） ---------
	// horzContrast: 行ごとの左右方向の変化量の合計 → 大きいほど「縦に走るエッジ」（左右で輝度が大きく変わる）
	// vertContrast: 列ごとの上下方向の変化量の合計 → 大きいほど「横に走るエッジ」（上下で輝度が大きく変わる）
	float horzContrast =
		abs(0.25 * lumaNW - 0.5 * lumaN + 0.25 * lumaNE) +
		abs(0.50 * lumaW - 1.0 * lumaM + 0.50 * lumaE) +
		abs(0.25 * lumaSW - 0.5 * lumaS + 0.25 * lumaSE);
	float vertContrast =
		abs(0.25 * lumaNW - 0.5 * lumaW + 0.25 * lumaSW) +
		abs(0.50 * lumaN - 1.0 * lumaM + 0.50 * lumaS) +
		abs(0.25 * lumaNE - 0.5 * lumaE + 0.25 * lumaSE);
	bool isHorizontalEdge = vertContrast >= horzContrast;

	// --------- エッジに垂直な方向で、勾配がより急な側を選ぶ ---------
	float luma1 = isHorizontalEdge ? lumaN : lumaW;
	float luma2 = isHorizontalEdge ? lumaS : lumaE;
	float gradient1 = luma1 - lumaM;
	float gradient2 = luma2 - lumaM;
	bool is1Steepest = abs(gradient1) >= abs(gradient2);
	float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

	float stepLength = isHorizontalEdge ? px.y : px.x;
	float lumaLocalAverage;
	if (is1Steepest) {
		stepLength = -stepLength;
		lumaLocalAverage = 0.5 * (luma1 + lumaM);
	} else {
		lumaLocalAverage = 0.5 * (luma2 + lumaM);
	}

	// エッジに垂直な方向へ半ピクセルずらした位置を起点に、エッジに沿う方向（接線方向）へ探索する
	float2 currentUv = uv;
	if (isHorizontalEdge) {
		currentUv.y += stepLength * 0.5;
	} else {
		currentUv.x += stepLength * 0.5;
	}

	float2 edgeStep = isHorizontalEdge ? float2(px.x, 0.0) : float2(0.0, px.y);
	float2 uv1 = currentUv - edgeStep;
	float2 uv2 = currentUv + edgeStep;

	// --------- エッジの両端（局所平均からの輝度差がgradientScaledを超える位置）を反復探索する ---------
	bool reached1 = false;
	bool reached2 = false;
	float lumaEnd1 = 0.0;
	float lumaEnd2 = 0.0;

	[loop]
	for (int i = 0; i < FXAA_SEARCH_STEPS; ++i) {
		if (!reached1) {
			lumaEnd1 = LumaAt(uv1) - lumaLocalAverage;
			reached1 = abs(lumaEnd1) >= gradientScaled;
		}
		if (!reached2) {
			lumaEnd2 = LumaAt(uv2) - lumaLocalAverage;
			reached2 = abs(lumaEnd2) >= gradientScaled;
		}
		if (reached1 && reached2) {
			break;
		}
		if (!reached1) {
			uv1 -= edgeStep;
		}
		if (!reached2) {
			uv2 += edgeStep;
		}
	}

	float distance1 = isHorizontalEdge ? (uv.x - uv1.x) : (uv.y - uv1.y);
	float distance2 = isHorizontalEdge ? (uv2.x - uv.x) : (uv2.y - uv.y);
	float distanceFinal = min(distance1, distance2);
	float edgeThickness = distance1 + distance2;

	// 端に近いピクセルほど強く（最大0.5ピクセル分）補正する
	float pixelOffset = 0.5 - distanceFinal / max(edgeThickness, 1e-4);

	// 見つかった端の輝度変化の向きが、このピクセルの輝度と局所平均の大小関係と整合しているかを確認する。
	// 整合していない場合（探索が誤った側の輪郭を拾ってしまった場合）は補正を適用しない
	bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
	bool correctVariation = ((distance1 < distance2) ? (lumaEnd1 < 0.0) : (lumaEnd2 < 0.0)) != isLumaCenterSmaller;
	float edgeOffset = correctVariation ? pixelOffset : 0.0;

	// エッジ探索によるオフセットと、サブピクセルエイリアシングのオフセットの大きい方を採用する
	float finalOffset = max(edgeOffset, subpixelBlend);

	float2 finalUv = uv;
	if (isHorizontalEdge) {
		finalUv.y += finalOffset * stepLength;
	} else {
		finalUv.x += finalOffset * stepLength;
	}

	float3 resultRgb = gTexture.Sample(gSampler, finalUv).rgb;
	float3 finalRgb = lerp(rgbM, resultRgb, gStrength);
	return float4(finalRgb, 1.0);
}
