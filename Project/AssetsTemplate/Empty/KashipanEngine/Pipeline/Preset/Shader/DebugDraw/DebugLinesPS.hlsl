struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : COLOR0;
};

float4 main(VSOutput input) : SV_Target0 {
	return input.color;
}
