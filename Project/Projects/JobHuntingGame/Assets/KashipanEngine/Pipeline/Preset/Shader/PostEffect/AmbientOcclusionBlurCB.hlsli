cbuffer AOBlurCB : register(b0) {
	float2 gTexelSize;
	int gRadius;
	float gDepthThreshold;
	float gNearClip;
	float gFarClip;
	float2 gPad;
};
