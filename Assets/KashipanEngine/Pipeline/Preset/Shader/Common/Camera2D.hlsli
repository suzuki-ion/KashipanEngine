struct Camera2D {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
};

ConstantBuffer<Camera2D> gCamera : register(b0);
