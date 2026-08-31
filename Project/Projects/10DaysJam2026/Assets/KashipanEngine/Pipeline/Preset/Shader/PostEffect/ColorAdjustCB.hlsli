cbuffer ColorAdjustCB : register(b0) {
	float gBrightness;
	float gContrast;
	float gSaturation;
	float gTemperature;
	float3 gColorBalance;
	float gPad;
};
