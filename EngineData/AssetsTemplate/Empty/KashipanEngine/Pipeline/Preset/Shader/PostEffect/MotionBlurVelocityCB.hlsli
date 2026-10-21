cbuffer MotionBlurVelocityCB : register(b0) {
	float4x4 gInvViewProjection;
	float4x4 gPrevViewProjection;
	float gNearClip;
	float gFarClip;
	float2 gPad;
};
