#include "ObjectVelocity.hlsli"

// カスタムシェーダー向けの追加パラメータは、この構造体の末尾に自由に追加してよい（Material3D.hlsli参照）。
struct Material {
	float enableLighting;
	float enableShadowMapProjection;
	float4 color;
	float4x4 uvTransform;
	float shininess;
	float4 specularColor;
	// gTextures[]（バインドレステクスチャ配列）/ gSamplers[]（静的サンプラー配列）内でのインデックス
	uint textureIndex;
	uint samplerIndex;
};
// バインドレステクスチャ配列（テクスチャごとに個別バインドせず、マテリアルのtextureIndexで選択する）
Texture2D gTextures[2048] : register(t0, space1); // 予約レンジ数(EngineSettings::Limits::maxTextures)と一致させること。GPUがResource Binding Tier 3など、真の無制限配列に非対応な場合があるため有限長にする
StructuredBuffer<Material> gMaterials : register(t1);
// 既定6種のみの静的サンプラー配列（ルートシグネチャに埋め込まれるためバインド操作は不要）
SamplerState gSamplers[6] : register(s3);

struct PSOutput {
	float4 velocity : SV_TARGET0;
};

PSOutput main(ObjectVelocityVSOutput input) {
	PSOutput output;
	
	Material mat = gMaterials[input.instanceId];
	float4 color = mat.color;
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
	float4 textureColor = gTextures[mat.textureIndex].Sample(gSamplers[mat.samplerIndex], transformedUV.xy);
	color *= textureColor;
	if (color.a < 0.1f) {
		discard;
	}
	
	const float eps = 1e-5f;

	// w が不正な場合（カメラ背面/発散）は速度 0 扱いにしてエンコードを安定化
	if (abs(input.position.w) < eps || abs(input.prevPosition.w) < eps || input.position.w <= 0.0f || input.prevPosition.w <= 0.0f) {
		output.velocity = float4(0.5f, 0.5f, 0.0f, 1.0f);
		return output;
	}

	float2 ndcPos = input.position.xy / input.position.w;
	float2 prevNdcPos = input.prevPosition.xy / input.prevPosition.w;

	float2 velocityNdc = ndcPos - prevNdcPos;
	float2 encoded = saturate(velocityNdc * 0.5f + 0.5f);

	output.velocity = float4(encoded, 0.0f, 1.0f);
	return output;
}