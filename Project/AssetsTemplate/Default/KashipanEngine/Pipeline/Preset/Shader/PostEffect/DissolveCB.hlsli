cbuffer DissolveCB : register(b0) {
	float maskThreshold;
	float edgeThickness;
	bool useBaseTexture;
	bool useMaskTexture;
	float4 baseTextureColor;
	float4 edgeColor;
};