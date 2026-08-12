// TextRenderer用のSDF（符号付き距離場）テキスト描画ピクセルシェーダー
// 頂点シェーダーは既存のObject2D用（ObjectVS.hlsl の Object2D 分岐）をそのまま流用する
// （インスタンスIDでの gTransformationMatrices 参照・Camera2D変換は共通のため）。
#include "../Object/Object2D.hlsli"

// 文字ごとのインスタンスデータ（1文字 = 1インスタンス）。構造体名をMaterialにすることで、
// 通常のMeshRenderer/SpriteRenderer等と同じ汎用パス（PipelineInfo::GetMaterialLayout /
// BuildMaterialElementBytes / WriteMaterialField、DrawBatch参照）に乗せている。
// characterColor/uvRect/boldWeightはDrawBatchがインスタンスごとにWriteMaterialFieldで上書きする
// （colorではなくcharacterColorという名前にしているのは、Object2D/Object3D側の
// 共有マテリアル本体の色を表す"color"フィールドと意味・書き込み経路が異なるため）
struct Material {
	float4 characterColor;
	float4 uvRect;    // x0,y0,x1,y1（アトラス内の正規化UV矩形）
	float boldWeight; // <b>タグによる太らせ量（SDF閾値のシフト量）
	float3 padding;
};

StructuredBuffer<Material> gMaterials : register(t2);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
	PSOutput output;
	Material ch = gMaterials[input.instanceId];

	float2 uv = lerp(ch.uvRect.xy, ch.uvRect.zw, input.texcoord);
	float distance = gTexture.Sample(gSampler, uv).r;

	// SDFは128(0.5相当)が輪郭。ピクセル微分（fwidth）で画面上のアンチエイリアス幅を求め、
	// 太字はこの閾値をシフトして実現する
	float aa = max(fwidth(distance), 1e-5f);
	float threshold = 0.5f - ch.boldWeight;
	float alpha = smoothstep(threshold - aa, threshold + aa, distance);

	output.color = float4(ch.characterColor.rgb, ch.characterColor.a * alpha);
	if (output.color.a < 0.01f) {
		discard;
	}
	return output;
}
