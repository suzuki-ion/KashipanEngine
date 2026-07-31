cbuffer WhiteNoiseCB : register(b0) {
	float4 gNoiseColor;
	uint gNoiseSeed;
	float gNoiseIntensity;
	float gNoiseScale;
	float gNoiseTime;
};