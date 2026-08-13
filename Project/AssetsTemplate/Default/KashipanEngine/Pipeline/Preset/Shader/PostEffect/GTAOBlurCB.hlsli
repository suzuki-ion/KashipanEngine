cbuffer GTAOBlurCB : register(b0) {
	float2 gAOTexelSize;
	float2 gBlurDirection;
	int gBlurRadius;
	float gDepthThreshold;
	float gNearClip;
	float gFarClip;
};
