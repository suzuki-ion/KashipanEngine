// AO計算パス用定数バッファ。ワールド座標の再構成にはビュー射影行列の逆行列を使う
// （深度バッファのみからワールド座標・法線をスクリーンスペースで再構成する方式のため、
//  Object3Dの通常描画で使うgCamera3Dとは別に、このエフェクト専用のカメラ情報を持つ）
cbuffer AOParamsCB : register(b0) {
	float4x4 gViewProjection;
	float4x4 gInvViewProjection;
	float3 gCameraWorldPosition;
	float gRadius;
	float gIntensity;
	float gPower;
	float gBias;
	uint gSampleCount;
	float2 gTexelSize;
	float gRotationSeed;
	float gPad;
};
