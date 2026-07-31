cbuffer OutlineCB : register(b0) {
	float2 gTexelSize;
	float gDepthThreshold;
	float gOutlineThickness;
	float4 gOutlineColor;
	float gNearClip;
	float gFarClip;
	float2 gPad;
};