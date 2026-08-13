cbuffer GTAOCB : register(b0) {
	float4x4 gViewProjection;
	float4x4 gInvViewProjection;
	float3 gCameraWorldPosition;
	float gRadius;
	float gIntensity;
	float gPower;
	float gBias;
	float gDepthThreshold;
	uint gDirectionCount;
	uint gStepCount;
	float2 gDepthTexelSize;
	float2 gAOTexelSize;
	float gNearClip;
	float gFarClip;
};
