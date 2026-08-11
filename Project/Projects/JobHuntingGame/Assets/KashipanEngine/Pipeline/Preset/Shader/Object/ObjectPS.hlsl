#ifdef Object2D
#include "Object2D.hlsli"
// struct Materialの本体はここで組み立てる。ADDITIONAL_MATERIAL_FIELDS_2Dの位置には、
// Graphics/Pipeline/System/ShaderModuleComposerが実行時に選択されたシェーダーモジュール
// （Modules/<Name>/Fields.hlsli）のフィールドを合成して差し込む。このファイルを直接
// （モジュール合成なしで）コンパイルする場合、コメントは無視されるため基本フィールドのみになる
struct Material {
	float4 color;
	float4x4 uvTransform;
    float useTexture;
	float3 padding;
/*{{ADDITIONAL_MATERIAL_FIELDS_2D}}*/
};
#endif

#ifdef Object3D
#include "../Common/ShadowMap.hlsli"
#include "../Common/AreaLight.hlsli"
#include "Object3D.hlsli"

// struct Materialの本体はここで組み立てる。基本フィールドはMaterial3D.hlsliに定義されており、
// ADDITIONAL_MATERIAL_FIELDSの位置には、Graphics/Pipeline/System/ShaderModuleComposerが実行時に
// 選択されたシェーダーモジュール（Modules/<Name>/Fields.hlsli）のフィールドを合成して差し込む。
// このファイルを直接（モジュール合成なしで）コンパイルする場合、コメントは無視されるため
// 基本フィールドのみのstruct Materialになる（押し出しアウトライン導入以前と同じ最小構成）
struct Material {
#include "../Common/Material3D.hlsli"
/*{{ADDITIONAL_MATERIAL_FIELDS}}*/
};

StructuredBuffer<PointLight> gPointLights : register(t4);
StructuredBuffer<SpotLight> gSpotLights : register(t5);
StructuredBuffer<DirectionalLight> gDirectionalLights : register(t6);
StructuredBuffer<SphereLight> gSphereLights : register(t8);
StructuredBuffer<DiscLight> gDiscLights : register(t9);
StructuredBuffer<RectLight> gRectLights : register(t10);
StructuredBuffer<TubeLight> gTubeLights : register(t11);
StructuredBuffer<BoxLight> gBoxLights : register(t12);

cbuffer LightCounts : register(b3) {
	uint gPointLightCount;
	uint gSpotLightCount;
	uint gDirectionalLightCount;
	uint gSphereLightCount;
	uint gDiscLightCount;
	uint gRectLightCount;
	uint gTubeLightCount;
	uint gBoxLightCount;
};

// Forward+ タイルライトカリングの結果（Directional以外の全種別。Directionalは対象外で従来通り全件ループする）。
// パックされたインデックスの上位3bitがライト種別タグ（LIGHT_TAG_*）、下位29bitが配列インデックス
#define LIGHT_TAG_POINT  0u
#define LIGHT_TAG_SPOT   1u
#define LIGHT_TAG_SPHERE 2u
#define LIGHT_TAG_DISC   3u
#define LIGHT_TAG_RECT   4u
#define LIGHT_TAG_TUBE   5u
#define LIGHT_TAG_BOX    6u
#define LIGHT_TAG_SHIFT  29u
#define LIGHT_INDEX_MASK 0x1FFFFFFFu

StructuredBuffer<uint> gTileLightIndices : register(t3);
cbuffer TileCullingConstants : register(b4) {
	float2 gScreenSize;
	uint gTileCountX;
	uint gTileCountY;
	uint gPointLightCountForCulling;
	uint gSpotLightCountForCulling;
	uint gSphereLightCountForCulling;
	uint gDiscLightCountForCulling;
	uint gRectLightCountForCulling;
	uint gTubeLightCountForCulling;
	uint gBoxLightCountForCulling;
	uint gMaxLightsPerTile;
	uint gTileSize;
};
#endif

Texture2D gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t2);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

#ifdef Object2D
// 選択されたシェーダーモジュール（Modules/<Name>/Logic.hlsli、テクスチャ宣言＋フック関数）がここに合成される。
// gTexture/gMaterials/gSamplerの宣言より後（モジュールがgSampler等を参照できる位置）に置くこと。
// モジュール合成なしでこのファイルを直接コンパイルする場合、コメントは無視されるため何も追加されない
/*{{MODULE_LOGIC_2D}}*/
#endif

#ifdef Object3D
// 選択されたシェーダーモジュール（Modules/<Name>/Logic.hlsli、テクスチャ宣言＋フック関数）がここに合成される。
// HalfLambert内のTONE_HOOK（MultiToneモジュール等）がここで定義される関数を呼び出すため、
// Lambert/HalfLambertより前に合成しておく必要がある。モジュール合成なしでこのファイルを直接
// コンパイルする場合、コメントは無視されるため何も追加されない
/*{{MODULE_LOGIC}}*/

float Lambert(float3 normal, float3 lightDir) {
	float cos = saturate(dot(normalize(normal), lightDir));
	return cos;
}

float HalfLambert(float3 normal, float3 lightDir, Material mat) {
	float NdotL = dot(normalize(normal), normalize(lightDir));
	float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
#ifdef ObjectToon
	// なめらかな階調ではなく、影・中間・明部の3段階へ量子化する（トゥーン調）。
	// 境界はsmoothstepでわずかにぼかし、バンド間のジャギーを抑える。
	// MultiToneモジュール選択時はTONE_HOOKがここを多段影（2/3階調切替可能）ロジックへ差し替える
	/*{{TONE_HOOK_BEGIN}}*/
	const float kShadowLevel = 0.35f;
	const float kMidLevel = 0.7f;
	const float kLitLevel = 1.0f;
	const float kBandSoftness = 0.05f;
	float toon = kShadowLevel;
	toon = lerp(toon, kMidLevel, smoothstep(0.45f - kBandSoftness, 0.45f + kBandSoftness, halfLambert));
	toon = lerp(toon, kLitLevel, smoothstep(0.8f - kBandSoftness, 0.8f + kBandSoftness, halfLambert));
	return toon;
	/*{{TONE_HOOK_END}}*/
#else
	return halfLambert;
#endif
}

float PhongReflection(float3 normal, float3 lightDir, float3 worldPos, float shininess) {
	float3 viewDir = normalize(gCamera3D.eyePosition.xyz - worldPos);
	float3 reflectDir = reflect(lightDir, normal);
	float RdotE = dot(reflectDir, viewDir);
	float spec = pow(saturate(RdotE), shininess);
	return spec;
}

float BlinnPhongReflection(float3 normal, float3 lightDir, float3 worldPos, float shininess) {
	float3 viewDir = normalize(gCamera3D.eyePosition.xyz - worldPos);
	float3 halfDir = normalize(-lightDir + viewDir);
	float NdotH = dot(normal, halfDir);
	float spec = pow(saturate(NdotH), shininess);
#ifdef ObjectToon
	// なめらかな減衰ではなく、輪郭のはっきりしたハイライトにする（トゥーン調）
	return smoothstep(0.45f, 0.55f, spec);
#else
	return spec;
#endif
}

// ディザ用ハッシュ・ブルーノイズテーブル・ComputeDitherThreshold本体はShadowMapPS.hlslとも共有するため
// 共通ファイルへ切り出してある
#include "../Common/BlueNoiseDither.hlsli"
#endif

PSOutput main(VSOutput input) {
	PSOutput output;
	Material mat = gMaterials[input.instanceId];
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
#ifdef Object2D
    float4 textureColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	if (mat.useTexture > 0.5f) {
		textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	}
	output.color = mat.color * textureColor;
	// 選択されたモジュール（例: Vignette2D→ColorGrading2D、優先度順）の合成後処理をここで呼び出す
	/*{{COMPOSITE_HOOKS_2D}}*/
	// 選択されたモジュール（例: Dissolve2D）のアルファ処理をここで呼び出す
	/*{{ALPHA_HOOKS_2D}}*/
#endif

#ifdef Object3D
    float4 textureColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	if (mat.useTexture > 0.5f) {
		textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	}
	float4 baseColor = mat.color * textureColor;
	// オブジェクト単位の色（MeshRendererのInstance Color）をマテリアルの色へ適用する。
	// 0=Override(置き換え), 1=Multiply(乗算), 2=Add(加算), 3=Subtract(減算)。アルファも同じ規則で合成する
	// （baseColor.aとして保持し、下のoutput.color.a算出で使う）
	if (mat.instanceColorBlendMode < 0.5f) {
		baseColor = mat.instanceColor;
	} else if (mat.instanceColorBlendMode < 1.5f) {
		baseColor *= mat.instanceColor;
	} else if (mat.instanceColorBlendMode < 2.5f) {
		baseColor += mat.instanceColor;
	} else {
		baseColor -= mat.instanceColor;
	}
	float4 lightingColor = float4(0,0,0,0);
	float4 envColor = float4(0,0,0,0);
	if (!mat.enableLighting) {
		lightingColor = float4(1,1,1,1);
	}

	// カメラへの方向（リムライトやローカルライトの鏡面反射で共通して使う）
	float3 viewDir = normalize(gCamera3D.eyePosition.xyz - input.worldPosition);

	// Gradationモジュール用の陰影度（ディレクショナルライトのHalfLambert結果）。ライトが無ければ1.0（明部）のまま
	float toonFactor = 1.0f;

	// 法線マップ: 接空間（TBN）で摂動した法線をライティング全体で使う。シャドウのバイアス計算
	// （ShadowSlopeFactor）だけは、細かい凹凸に引きずられて不安定にならないよう幾何法線のまま使う
	float3 shadingNormal = normalize(input.normal);
	// 選択されたモジュール（例: NormalMap→SpecularMap、優先度順）のライティング前処理をここで呼び出す
	/*{{PRELIGHTING_HOOKS}}*/

	// Directional lights
	if (mat.enableLighting) {
		for (uint i = 0; i < gDirectionalLightCount; ++i) {
			DirectionalLight light = gDirectionalLights[i];
			if (!light.enabled) {
				continue;
			}
			float3 toLightDir = -light.direction;
			float lam = HalfLambert(shadingNormal, toLightDir, mat);
			toonFactor = lam;
			float spec = BlinnPhongReflection(shadingNormal, light.direction, input.worldPosition, mat.shininess);
			float4 diffuse = light.color * lam * light.intensity;
			float4 speculer = light.color * light.intensity * spec * mat.specularColor;
			// このライトが影を生成する場合、カスケードシャドウマップから影係数を求めて
			// このライトの寄与のみを減衰させる
			float shadow = 1.0f;
			if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
				shadow = ComputeDirectionalShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, light.direction);
			}
			lightingColor += (diffuse + speculer) * shadow;

			// リムライト: 視線に対して縁になる部分を、このライトが「カメラの逆側（背後）」にある
			// ときほど強く縁取る（逆光時に輪郭が光る表現）。mat.rimIntensity=0（既定）では無効
			if (mat.rimIntensity > 0.0f) {
				float rimFactor = pow(saturate(1.0f - saturate(dot(shadingNormal, viewDir))), mat.rimPower);
				float backlightMask = saturate(-dot(toLightDir, viewDir));
				// RimShadeモジュール選択時、RIM_COLOR_HOOKがここを影側別色ロジックへ差し替える
				/*{{RIM_COLOR_HOOK_BEGIN}}*/
				float3 rimColor = mat.rimColor.rgb;
				/*{{RIM_COLOR_HOOK_END}}*/
				float3 rim = rimColor * mat.rimIntensity * rimFactor * backlightMask * light.color.rgb * light.intensity * shadow;
				lightingColor.rgb += rim;
			}

			// 選択されたモジュール（例: Backlight）の追加ディレクショナルライト処理をここで呼び出す
			/*{{DIRECTIONAL_HOOKS}}*/
		}
	}

	// ローカルライト（Forward+: このピクセルが属するタイルのライトインデックスリストだけをループする）
	if (mat.enableLighting) {
		float3 reflectDir = reflect(-viewDir, shadingNormal);

		uint2 tileCoord = uint2(input.position.xy) / max(gTileSize, 1u);
		tileCoord = min(tileCoord, uint2(gTileCountX - 1, gTileCountY - 1));
		uint tileIndex = tileCoord.y * gTileCountX + tileCoord.x;
		uint tileBase = tileIndex * (1 + gMaxLightsPerTile);
		uint tileLightCount = gTileLightIndices[tileBase];

		for (uint t = 0; t < tileLightCount; ++t) {
			uint packedIndex = gTileLightIndices[tileBase + 1 + t];
			uint lightTag = packedIndex >> LIGHT_TAG_SHIFT;
			uint lightIndex = packedIndex & LIGHT_INDEX_MASK;

			if (lightTag == LIGHT_TAG_POINT) {
				PointLight light = gPointLights[lightIndex];
				if (!light.enabled) continue;

				float3 toLight = light.position - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.radius) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float atten = pow(saturate(-dist / light.radius + 1.0f), light.decay);
				if (atten <= 0.0f) continue;

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, lightDir, input.worldPosition, mat.shininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				// このライトが影を生成する場合、キューブシャドウマップから影係数を求める
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputePointShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, light.position);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_SPOT) {
				SpotLight light = gSpotLights[lightIndex];
				if (!light.enabled) continue;

				float3 toLight = light.position - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.distance) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float theta = dot(-lightDir, normalize(light.direction));
				float inner = cos(light.innerAngle);
				float outer = cos(light.outerAngle);
				float spot = saturate((theta - outer) / (inner - outer));
				if (spot <= 0.0f) continue;

				float atten = pow(saturate(-dist / light.distance + 1.0f), light.decay) * spot;
				if (atten <= 0.0f) continue;

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, lightDir, input.worldPosition, mat.shininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				// このライトが影を生成する場合、シャドウマップから影係数を求める
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputeSpotShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, lightDir);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_SPHERE) {
				SphereLight light = gSphereLights[lightIndex];
				if (!light.enabled) continue;

				float3 toLight = light.position - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.radius) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float atten = pow(saturate(-dist / light.radius + 1.0f), light.decay);
				if (atten <= 0.0f) continue;

				// 鏡面は形状上の代表点（反射ベクトルと球面の最近接点）への方向で評価し、
				// 光源サイズに応じてハイライトを広げる（真のLTC等ではなく代表点法による近似）
				float3 representativePoint = SphereRepresentativePoint(input.worldPosition, reflectDir, light.position, light.sourceRadius);
				float3 specDir = normalize(input.worldPosition - representativePoint);
				float adjustedShininess = AreaLightAdjustedShininess(mat.shininess, light.sourceRadius, dist);

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, specDir, input.worldPosition, adjustedShininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputePointShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, light.position);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_DISC) {
				DiscLight light = gDiscLights[lightIndex];
				if (!light.enabled) continue;

				float3 toLight = light.position - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.distance) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float3 discNormal = normalize(light.direction);
				// ディスクは片面発光。発光面の法線と表面へ向かう方向のなす角で正面/背面を判定する
				float facing = saturate(dot(-lightDir, discNormal));
				if (facing <= 0.0f) continue;

				float atten = pow(saturate(-dist / light.distance + 1.0f), light.decay) * facing;
				if (atten <= 0.0f) continue;

				float3 representativePoint = DiscRepresentativePoint(input.worldPosition, reflectDir, light.position, discNormal, light.sourceRadius);
				float3 specDir = normalize(input.worldPosition - representativePoint);
				float adjustedShininess = AreaLightAdjustedShininess(mat.shininess, light.sourceRadius, dist);

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, specDir, input.worldPosition, adjustedShininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputeSpotShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, lightDir);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_RECT) {
				RectLight light = gRectLights[lightIndex];
				if (!light.enabled) continue;

				float3 toLight = light.position - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.distance) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float3 rectNormal = normalize(light.direction);
				// 矩形も片面発光
				float facing = saturate(dot(-lightDir, rectNormal));
				if (facing <= 0.0f) continue;

				float atten = pow(saturate(-dist / light.distance + 1.0f), light.decay) * facing;
				if (atten <= 0.0f) continue;

				float3 representativePoint = RectRepresentativePoint(input.worldPosition, reflectDir, light.position, rectNormal,
					light.right, light.up, light.width * 0.5f, light.height * 0.5f);
				float3 specDir = normalize(input.worldPosition - representativePoint);
				float sourceRadius = max(light.width, light.height) * 0.5f;
				float adjustedShininess = AreaLightAdjustedShininess(mat.shininess, sourceRadius, dist);

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, specDir, input.worldPosition, adjustedShininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputeSpotShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, lightDir);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_TUBE) {
				TubeLight light = gTubeLights[lightIndex];
				if (!light.enabled) continue;

				// チューブ上の最近接点を代表点として、球ライトと同じ扱いで拡散・鏡面を求める
				float3 closest = TubeClosestPoint(input.worldPosition, light.p0, light.p1);
				float3 toLight = closest - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.radius) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float atten = pow(saturate(-dist / light.radius + 1.0f), light.decay);
				if (atten <= 0.0f) continue;

				float3 representativePoint = SphereRepresentativePoint(input.worldPosition, reflectDir, closest, light.sourceRadius);
				float3 specDir = normalize(input.worldPosition - representativePoint);
				float adjustedShininess = AreaLightAdjustedShininess(mat.shininess, light.sourceRadius, dist);

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, specDir, input.worldPosition, adjustedShininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					float3 midpoint = (light.p0 + light.p1) * 0.5f;
					shadow = ComputePointShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, midpoint);
				}
				lightingColor += (diffuse + speculer) * shadow;
			} else if (lightTag == LIGHT_TAG_BOX) {
				BoxLight light = gBoxLights[lightIndex];
				if (!light.enabled) continue;

				// ボックス表面上の最近接点を代表点として、チューブライトと同じ扱いで拡散・鏡面・減衰を求める
				// （内部・表面付近は最近接点=表面上のクランプ済み点となり減衰なし）
				float3 halfExtents = float3(light.halfWidth, light.halfHeight, light.halfDepth);
				float3 closest = BoxClosestPoint(input.worldPosition, light.position, light.right, light.up, light.forward, halfExtents);
				float3 toLight = closest - input.worldPosition;
				float dist = length(toLight);
				if (dist > light.radius) continue;

				float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
				float atten = pow(saturate(-dist / light.radius + 1.0f), light.decay);
				if (atten <= 0.0f) continue;

				float sourceRadius = max(max(light.halfWidth, light.halfHeight), light.halfDepth);
				float3 representativePoint = SphereRepresentativePoint(input.worldPosition, reflectDir, closest, sourceRadius);
				float3 specDir = normalize(input.worldPosition - representativePoint);
				float adjustedShininess = AreaLightAdjustedShininess(mat.shininess, sourceRadius, dist);

				float lam = HalfLambert(shadingNormal, lightDir, mat);
				float spec = BlinnPhongReflection(shadingNormal, specDir, input.worldPosition, adjustedShininess);
				float4 diffuse = light.color * lam * light.intensity * atten;
				float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
				float shadow = 1.0f;
				if (mat.enableShadowMapProjection && light.shadowMapIndex >= 0) {
					shadow = ComputePointShadowFactor((uint)light.shadowMapIndex, input.worldPosition, input.normal, light.position);
				}
				lightingColor += (diffuse + speculer) * shadow;
			}
		}
	}

	// 選択されたモジュール（EnvironmentMap）によるenvColor算出をここで呼び出す
	/*{{ENVIRONMENT_HOOKS}}*/

	output.color = baseColor * lightingColor + envColor;

	// 選択されたモジュール（例: Matcap→Gradation→Emission→ColorGrading、優先度順）の合成後処理をここで呼び出す
	/*{{COMPOSITE_HOOKS}}*/

	// mat.color.a * textureColor.aで再計算し直すとInstance Colorのアルファ（baseColor.aに反映済み）が
	// 無視されてしまうため、baseColor.aをそのまま使う
	output.color.a = baseColor.a;

	// 選択されたモジュール（例: DistanceFade→Dissolve、優先度順）のアルファ処理をここで呼び出す
	/*{{ALPHA_HOOKS}}*/

	if (output.color.a < 0.01f) {
		discard;
	}
	if (output.color.a < 1.0f) {
		float distanceFromCamera = length(input.worldPosition - gCamera3D.eyePosition.xyz);
		float threshold = ComputeDitherThreshold(input.position.xy, input.idSeed, mat.enableTemporalDither, distanceFromCamera, mat.ditherDepthBucketSize, mat.useBayerDither);
		if (output.color.a <= threshold) {
			discard;
		}
		output.color.a = 1.0f;
	}
#endif
	return output;
}
