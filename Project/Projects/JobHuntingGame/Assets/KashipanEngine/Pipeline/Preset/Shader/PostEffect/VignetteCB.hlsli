cbuffer VignetteCB : register(b0) {
    float2 gVignetteCenter;
	float2 gVignettePad0;
    float4 gVignetteColor;
    float gVignetteIntensity;
    float gVignetteInnerRadius;
    float gVignetteSmoothness;
    float gVignettePad1;
};
