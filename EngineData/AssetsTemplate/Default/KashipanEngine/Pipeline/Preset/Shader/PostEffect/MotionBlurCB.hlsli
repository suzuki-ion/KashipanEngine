cbuffer MotionBlurCB : register(b0) {
    float gIntensity;
    float gVelocityScale;
    float gMaxBlurPixels;
    uint  gSamples;
    float2 gInvResolution;
    float2 gPad;
};
