// NV12(輝度Y+色差UVの2プレーン)テクスチャをRGBAテクスチャへ変換するコンピュートシェーダー。
// 変換後はRenderer::ProcessVideoConversionsが後段の通常描画パスより先に完了させるため、
// 描画側は普通のRGBAテクスチャとしてそのまま読める
Texture2D<float> gTextureY : register(t0);
Texture2D<float2> gTextureUV : register(t1);
RWTexture2D<float4> gOutput : register(u0);

// BT.601 limited range のYUV→RGB変換
float3 YUVtoRGB(float y, float2 uv) {
	float Y = y * 255.0f - 16.0f;
	float U = uv.x * 255.0f - 128.0f;
	float V = uv.y * 255.0f - 128.0f;
	return saturate(float3(
		1.164f * Y + 1.596f * V,
		1.164f * Y - 0.392f * U - 0.813f * V,
		1.164f * Y + 2.017f * U) / 255.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
	uint width, height;
	gOutput.GetDimensions(width, height);
	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) return;

	float y = gTextureY.Load(int3(dispatchThreadID.xy, 0));
	// UV平面は4:2:0サブサンプリングのため輝度の半分の解像度
	float2 uv = gTextureUV.Load(int3(dispatchThreadID.xy / 2, 0));
	float3 rgb = YUVtoRGB(y, uv);

	gOutput[dispatchThreadID.xy] = float4(rgb, 1.0f);
}
