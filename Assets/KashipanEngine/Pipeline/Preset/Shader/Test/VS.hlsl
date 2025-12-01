struct VSInput {
    float4 position : POSITION0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = input.position;
    return output;
}
