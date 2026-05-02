cbuffer RadialBlurCB : register(b0) {
	float gIntensity;
	uint gSampleCount;
	float2 gRadialCenter;
	float gStartRadius;
	float3 gPad;
};