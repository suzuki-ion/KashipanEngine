// TextRendererを3D空間に配置するためのSDF（符号付き距離場）テキスト描画ピクセルシェーダー
// 頂点シェーダーは既存のObject3D用（ObjectVS.hlsl の Object3D 分岐）をそのまま流用する
// （インスタンスIDでの gTransformationMatrices 参照・Camera3D変換・深度書き込みが必要なため）。
// ライティングは行わない（TextSDFPS.hlslと同じくSDFアルファのみを計算する不透明でないUnlit描画）
#include "../Object/Object3D.hlsli"

// 文字ごとのインスタンスデータ（1文字 = 1インスタンス。Renderer.cppのTextCharacterElementと同レイアウト）
struct TextCharacterElement {
	float4 color;
	float4 uvRect;    // x0,y0,x1,y1（アトラス内の正規化UV矩形）
	float boldWeight; // <b>タグによる太らせ量（SDF閾値のシフト量）
	float3 padding;
};

StructuredBuffer<TextCharacterElement> gMaterials : register(t2);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
	PSOutput output;
	TextCharacterElement ch = gMaterials[input.instanceId];

	float2 uv = lerp(ch.uvRect.xy, ch.uvRect.zw, input.texcoord);
	float distance = gTexture.Sample(gSampler, uv).r;

	// SDFは128(0.5相当)が輪郭。ピクセル微分（fwidth）で画面上のアンチエイリアス幅を求め、
	// 太字はこの閾値をシフトして実現する
	float aa = max(fwidth(distance), 1e-5f);
	float threshold = 0.5f - ch.boldWeight;
	float alpha = smoothstep(threshold - aa, threshold + aa, distance);

	output.color = float4(ch.color.rgb, ch.color.a * alpha);
	if (output.color.a < 0.01f) {
		discard;
	}
	return output;
}
