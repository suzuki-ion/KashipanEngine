cbuffer ScanlineCB : register(b0) {
    float2 gInvResolution;
    float gIntensity;
    float gLineSpacing;
    float gThickness;
    float3 gPad;
};
