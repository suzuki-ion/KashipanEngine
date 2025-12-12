struct TransformationMatrix {
	float4x4 wvp;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VSInput {
    float4 position : POSITION0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output;
	output.position = mul(input.position, gTransformationMatrix.wvp);
    return output;
}
