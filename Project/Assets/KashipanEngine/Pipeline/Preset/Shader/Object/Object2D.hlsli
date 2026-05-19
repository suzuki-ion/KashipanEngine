#include "../Common/Camera2D.hlsli"

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	uint instanceId : INSTANCEID;
};
