// TextRenderer用のSDF（符号付き距離場）テキスト描画ピクセルシェーダー
// 頂点シェーダーは既存のObject2D用（ObjectVS.hlsl の Object2D 分岐）をそのまま流用する
// （インスタンスIDでの gTransformationMatrices 参照・Camera2D変換は共通のため）。
#include "../Object/Object2D.hlsli"

// 文字ごとのインスタンスデータ（1文字 = 1インスタンス）。構造体名をMaterialにすることで、
// 通常のMeshRenderer/SpriteRenderer等と同じ汎用パス（PipelineInfo::GetMaterialLayout /
// BuildMaterialElementBytes / WriteMaterialField、DrawBatch参照）に乗せている。
// color等は割り当てた通常マテリアルから、instanceColor/uvRect/boldWeight/アウトライン値は
// DrawBatchが文字インスタンスごとにWriteMaterialFieldで書き込む。
struct Material {
	// 通常の2D/3Dマテリアルと共通の基本フィールド。テクスチャとuvTransformは
	// TextRendererではフォントアトラス保護のため描画側が無視する。
	float4 color;
	float4x4 uvTransform;
	float useTexture;
	float3 padding0;
	float4 instanceColor;
	float instanceColorBlendMode;
	float3 padding1;
	// gTextures[]（バインドレステクスチャ配列）/ gSamplers[]（静的サンプラー配列）内でのインデックス
	// （TextRendererではフォントアトラスを指す）
	uint textureIndex;
	uint samplerIndex;
	float4 uvRect;    // x0,y0,x1,y1（アトラス内の正規化UV矩形）
	float boldWeight; // <b>タグによる太らせ量（SDF閾値のシフト量）
	float outlineWidth;
	float2 padding2;
	float4 outlineColor;
};

StructuredBuffer<Material> gMaterials : register(t2);
// バインドレステクスチャ配列（テクスチャごとに個別バインドせず、マテリアルのtextureIndexで選択する）
Texture2D gTextures[2048] : register(t0, space1); // 予約レンジ数(EngineSettings::Limits::maxTextures)と一致させること。GPUがResource Binding Tier 3など、真の無制限配列に非対応な場合があるため有限長にする
// 既定6種のみの静的サンプラー配列（ルートシグネチャに埋め込まれるためバインド操作は不要）
SamplerState gSamplers[6] : register(s3);

struct PSOutput {
	float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
	PSOutput output;
	Material ch = gMaterials[input.instanceId];

	float2 uv = lerp(ch.uvRect.xy, ch.uvRect.zw, input.texcoord);
	float distance = gTextures[ch.textureIndex].Sample(gSamplers[ch.samplerIndex], uv).r;

	// SDFは128(0.5相当)が輪郭。ピクセル微分（fwidth）で画面上のアンチエイリアス幅を求め、
	// 太字はこの閾値をシフトして実現する
	float aa = max(fwidth(distance), 1e-5f);
	float4 fillColor = ch.color;
	if (ch.instanceColorBlendMode < 0.5f) {
		fillColor = ch.instanceColor;
	} else if (ch.instanceColorBlendMode < 1.5f) {
		fillColor *= ch.instanceColor;
	} else if (ch.instanceColorBlendMode < 2.5f) {
		fillColor += ch.instanceColor;
	} else {
		fillColor -= ch.instanceColor;
	}
	fillColor = saturate(fillColor);

	float fillThreshold = 0.5f - ch.boldWeight;
	float fillCoverage = smoothstep(fillThreshold - aa, fillThreshold + aa, distance);
	float outlineThreshold = fillThreshold - max(ch.outlineWidth, 0.0f);
	float outlineCoverage = smoothstep(outlineThreshold - aa, outlineThreshold + aa, distance);
	float outlineOnlyCoverage = saturate(outlineCoverage - fillCoverage);

	// 塗りとアウトラインをstraight alphaで合成する。境界のAA部分でも色が暗く濁らないよう、
	// 一度premultiplied相当で重み付けしてからRGBを戻す。
	float fillAlpha = fillColor.a * fillCoverage;
	float outlineAlpha = ch.outlineColor.a * outlineOnlyCoverage;
	float alpha = saturate(fillAlpha + outlineAlpha);
	float3 weightedColor = fillColor.rgb * fillAlpha + ch.outlineColor.rgb * outlineAlpha;
	output.color = float4(alpha > 1e-5f ? weightedColor / alpha : fillColor.rgb, alpha);
	if (output.color.a < 0.01f) {
		discard;
	}
	return output;
}
