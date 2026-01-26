#include "FullscreenTriangle.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer FXAACB : register(b0) {
	float2 gInvScreenSize; // (1.0 / width, 1.0 / height)
	float gEdgeThreshold;
	float gEdgeThresholdMin;
	float gStrength;
};

// Luma計算（Rec.709）
float Luma(float3 rgb) {
	return dot(rgb, float3(0.2126, 0.7152, 0.0722));
}

float4 main(VSOutput input) : SV_TARGET {
	float2 uv = input.uv;

    // 近傍サンプル
	float2 px = gInvScreenSize;

	float3 rgbM = gTexture.Sample(gSampler, uv).rgb;
	float3 rgbN = gTexture.Sample(gSampler, uv + float2(0.0, -px.y)).rgb;
	float3 rgbS = gTexture.Sample(gSampler, uv + float2(0.0, px.y)).rgb;
	float3 rgbW = gTexture.Sample(gSampler, uv + float2(-px.x, 0.0)).rgb;
	float3 rgbE = gTexture.Sample(gSampler, uv + float2(px.x, 0.0)).rgb;

	float lumaM = Luma(rgbM);
	float lumaN = Luma(rgbN);
	float lumaS = Luma(rgbS);
	float lumaW = Luma(rgbW);
	float lumaE = Luma(rgbE);

	float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
	float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));

	float lumaRange = lumaMax - lumaMin;

    // エッジが弱ければそのまま返す
	if (lumaRange < max(gEdgeThreshold, lumaMax * gEdgeThresholdMin)) {
		return float4(rgbM, 1.0);
	}

    // 勾配方向の推定
	float lumaNS = lumaN + lumaS;
	float lumaWE = lumaW + lumaE;

	bool isHorizontal = (abs(lumaNS - 2.0 * lumaM) >= abs(lumaWE - 2.0 * lumaM));

	float2 dir;
	if (isHorizontal) {
        // 水平方向にブラー
		float lumaN2 = Luma(gTexture.Sample(gSampler, uv + float2(0.0, -2.0 * px.y)).rgb);
		float lumaS2 = Luma(gTexture.Sample(gSampler, uv + float2(0.0, 2.0 * px.y)).rgb);

		float lumaSum = lumaN + lumaS + lumaN2 + lumaS2;
		dir = float2(0.0, (lumaN2 + lumaN - lumaS2 - lumaS) / (abs(lumaSum) + 1e-4));
	} else {
        // 垂直方向にブラー
		float lumaW2 = Luma(gTexture.Sample(gSampler, uv + float2(-2.0 * px.x, 0.0)).rgb);
		float lumaE2 = Luma(gTexture.Sample(gSampler, uv + float2(2.0 * px.x, 0.0)).rgb);

		float lumaSum = lumaW + lumaE + lumaW2 + lumaE2;
		dir = float2((lumaW2 + lumaW - lumaE2 - lumaE) / (abs(lumaSum) + 1e-4), 0.0);
	}

    // 正規化＆スケール
	float dirReduce = max(
        (lumaN + lumaS + lumaW + lumaE) * (0.25 * 0.5),
        1.0 / 128.0
    );

	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = saturate(dir * rcpDirMin) * px;

    // サブピクセルブレンド
	float3 rgbA = 0.5 * (
        gTexture.Sample(gSampler, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
        gTexture.Sample(gSampler, uv + dir * (2.0 / 3.0 - 0.5)).rgb
    );

	float3 rgbB = rgbA * 0.5 + 0.25 * (
        gTexture.Sample(gSampler, uv + dir * -0.5).rgb +
        gTexture.Sample(gSampler, uv + dir * 0.5).rgb
    );

	float lumaB = Luma(rgbB);
	
	float3 resultRgb;
	if (lumaB < lumaMin || lumaB > lumaMax) {
		resultRgb = rgbA;
	} else {
		resultRgb = rgbB;
	}
	
	float3 finalRgb = lerp(rgbM, resultRgb, gStrength);
	return float4(finalRgb, 1.0);
}