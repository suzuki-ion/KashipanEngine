struct Camera3D {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4 eyePosition;
	float fov;
};

ConstantBuffer<Camera3D> gCamera : register(b0);