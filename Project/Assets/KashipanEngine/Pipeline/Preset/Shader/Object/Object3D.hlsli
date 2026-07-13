#include "../Common/Camera3D.hlsli"

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
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
};

struct PointLight {
	uint enabled;
	float4 color;
	float3 position;
	float radius;
	float intensity;
	float decay;
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
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
	// 影を生成するライトのシャドウスロット番号（影を生成しない場合は -1）
	int shadowMapIndex;
};
