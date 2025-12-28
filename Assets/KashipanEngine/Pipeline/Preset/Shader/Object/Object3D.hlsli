struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	float3 normal : NORMAL;
	uint instanceId : INSTANCEID;
};

struct DirectionalLight {
	uint enabled;
	float4 color;
	float3 direction;
	float intensity;
};