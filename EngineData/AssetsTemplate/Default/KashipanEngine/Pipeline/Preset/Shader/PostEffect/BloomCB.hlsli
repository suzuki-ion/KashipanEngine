cbuffer BloomCB : register(b0) {
	float gThreshold;
	float gSoftKnee;
	float gIntensity;
	float gBlurRadius;
};