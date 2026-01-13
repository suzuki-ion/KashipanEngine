struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint vid : SV_VertexID) {
    // Fullscreen triangle
    float2 pos;
    pos.x = (vid == 2) ? 3.0 : -1.0;
    pos.y = (vid == 1) ? -3.0 : 1.0;

    VSOut o;
    o.pos = float4(pos, 0.0, 1.0);
    o.uv = pos * float2(0.5, -0.5) + 0.5;
    return o;
}
