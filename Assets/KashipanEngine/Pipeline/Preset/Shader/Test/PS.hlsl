struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main() {
	PSOutput output;
	output.color = float4(1.0, 0.0, 0.0, 1.0);
	return output;
}
