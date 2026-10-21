cbuffer DoFBlurCB : register(b0) {
	float2 gTexelSize;
	float2 gMaxBlurRadiusUv; // 軸ごとの最大ぼかし半径（UV単位。非正方形解像度でも円形のボケになるよう軸別に持つ）
	uint gSampleCount;
	float gRotationSeed;
	float2 gPad;
};
