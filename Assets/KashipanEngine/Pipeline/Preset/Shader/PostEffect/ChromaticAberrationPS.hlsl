Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ChromaticAberrationCB : register(b0)
{
    float2 gDirection; // normalized-ish screen direction (e.g. (1,0))
    float  gStrength;  // UV offset magnitude
    float  _pad;
};

struct PSIn {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn input) : SV_Target0 {
    float2 uv = input.uv;
    float2 off = gDirection * gStrength;

    float r = gTexture.Sample(gSampler, uv + off).r;
    float g = gTexture.Sample(gSampler, uv).g;
    float b = gTexture.Sample(gSampler, uv - off).b;

    return float4(r, g, b, 1.0);
}
