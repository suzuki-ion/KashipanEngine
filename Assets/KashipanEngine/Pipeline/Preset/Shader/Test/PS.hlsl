struct Material {
	float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main() {
	PSOutput output;
	output.color = gMaterial.color;
	return output;
}
