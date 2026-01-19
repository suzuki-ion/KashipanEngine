#pragma once

struct ObjectVelocityCamera {
	float4x4 viewProjection;
	float4x4 prevViewProjection;
};

ConstantBuffer<ObjectVelocityCamera> gObjectVelocityCamera : register(b1);

struct ObjectVelocityVSOutput {
	float4 position : SV_POSITION;
	float4 prevPosition : PREVPOSITION;
	float2 texcoord : TEXCOORD;
	uint instanceId : INSTANCEID;
};
