#include "../Object/Object3D.hlsli"
#include "../Common/BlueNoiseDither.hlsli"

struct Material {
#include "../Common/Material3D.hlsli"
};

Texture2D gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
SamplerState gSampler : register(s0);

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput o;
	Material mat = gMaterials[input.instanceId];
	float4 color = mat.color;
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	o.color = color * textureColor;
	if (o.color.a < 0.01f) {
		discard;
	}
	if (o.color.a < 1.0f && !mat.disableShadowDither && !mat.useAlphaBlend) {
		// 本体の半透明ディザ(ObjectPS.hlsl)と同じ閾値テーブル・同じidSeedを使い、影の濃さも
		// アルファに応じて薄くする。フレーム間の無相関化(enableTemporalDither)もキャスターの
		// マテリアルと同じ値を使う。シャドウマップ自体には時間的ブレンドの仕組みが無いが、
		// 影の結果は最終的に合成後のカラーバッファへ反映されるため、そちらにTemporalBlendEffectが
		// かかっていれば本体の面と同様に時間方向で平均化され滑らかになる
		// （gCamera3D.eyePositionはこのパイプラインではライトの位置が入っている。ShadowMapVS.hlsl参照）
		// disableShadowDitherがtrueの場合、またはuseAlphaBlend（本格アルファブレンド）がtrueの場合は
		// ここを丸ごとスキップし、上のalpha<0.01判定だけで影を落とす
		// （テクスチャのアルファ抜き形状はそのまま活きる、フォリッジ等向けの挙動）
		float distanceFromLight = length(input.worldPosition - gCamera3D.eyePosition.xyz);
		float threshold = ComputeDitherThreshold(input.position.xy, input.idSeed, mat.enableTemporalDither, distanceFromLight, mat.ditherDepthBucketSize, mat.useBayerDither, mat.useBayer64x64);
		if (o.color.a <= threshold) {
			discard;
		}
	}
    return o;
}
