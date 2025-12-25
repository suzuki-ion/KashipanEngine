struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

struct DirectionalLight {
	bool enabled;
	float4 color;
	float3 direction;
	float intensity;
	float4x4 viewProjectionMatrix;
};