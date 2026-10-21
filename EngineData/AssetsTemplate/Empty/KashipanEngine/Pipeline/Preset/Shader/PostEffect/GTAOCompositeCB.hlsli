cbuffer GTAOCompositeCB : register(b0) {
	float2 gAOTexelSize;
	float gDepthThreshold;
	float gNearClip;
	float gFarClip;
	uint gShowAOOnly;
	float2 gCompositePadding;
};
