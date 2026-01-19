struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 worldPosition : WORLDPOSITION;
	uint instanceId : INSTANCEID;
};

struct DirectionalLight {
	uint enabled;
	float4 color;
	float3 direction;
	float intensity;
};

struct PointLight {
	uint enabled;
	float4 color;
	float3 position;
	float radius;
	float intensity;
	float decay;
};

struct SpotLight {
	uint enabled;
	float4 color;
	float3 position;
	float distance;
	float3 direction;
	float innerAngle;
	float outerAngle;
	float intensity;
	float decay;
};
