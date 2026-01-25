#ifdef Object2D
#include "Object2D.hlsli"
struct Material {
	float4 color;
	float4x4 uvTransform;
};
#endif

#ifdef Object3D
#include "../Common/Camera3D.hlsli"
#include "../Common/ShadowMap.hlsli"
#include "Object3D.hlsli"
struct Material {
	float enableLighting;
	float enableShadowMapProjection;
	float4 color;
	float4x4 uvTransform;
	float shininess;
	float4 specularColor;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

StructuredBuffer<PointLight> gPointLights : register(t3);
StructuredBuffer<SpotLight> gSpotLights : register(t4);

cbuffer LightCounts : register(b3) {
	uint gPointLightCount;
	uint gSpotLightCount;
};
#endif

Texture2D gTexture : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
SamplerState gSampler : register(s0);

struct PSOutput {
	float4 color : SV_TARGET0;
};

#ifdef Object3D
float Lambert(float3 normal, float3 lightDir) {
	float cos = saturate(dot(normalize(normal), lightDir));
	return cos;
}

float HalfLambert(float3 normal, float3 lightDir) {
	float NdotL = dot(normalize(normal), normalize(lightDir));
	float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);
	return halfLambert;
}

float PhongReflection(float3 normal, float3 lightDir, float3 worldPos, float shininess) {
	float3 viewDir = normalize(gCamera.eyePosition.xyz - worldPos);
	float3 reflectDir = reflect(lightDir, normal);
	float RdotE = dot(reflectDir, viewDir);
	float spec = pow(saturate(RdotE), shininess);
	return spec;
}

float BlinnPhongReflection(float3 normal, float3 lightDir, float3 worldPos, float shininess) {
	float3 viewDir = normalize(gCamera.eyePosition.xyz - worldPos);
	float3 halfDir = normalize(-lightDir + viewDir);
	float NdotH = dot(normal, halfDir);
	float spec = pow(saturate(NdotH), shininess);
	return spec;
}
#endif

PSOutput main(VSOutput input) {
	PSOutput output;
	Material mat = gMaterials[input.instanceId];
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), mat.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
#ifdef Object2D
	output.color = mat.color * textureColor;
#endif

#ifdef Object3D
	float4 baseColor = mat.color * textureColor;
	float4 lightingColor = float4(0,0,0,0);
	if (!mat.enableLighting) {
		lightingColor = float4(1,1,1,1);
	}

	// Directional
	if (gDirectionalLight.enabled && mat.enableLighting) {
		float lam = HalfLambert(input.normal, -gDirectionalLight.direction);
		float spec = BlinnPhongReflection(input.normal, gDirectionalLight.direction, input.worldPosition, mat.shininess);
		float4 diffuse = gDirectionalLight.color * lam * gDirectionalLight.intensity;
		float4 speculer = gDirectionalLight.color * gDirectionalLight.intensity * spec * mat.specularColor;
		lightingColor += diffuse + speculer;
	}

	// Point lights
	if (mat.enableLighting) {
		for (uint i = 0; i < gPointLightCount; ++i) {
			PointLight light = gPointLights[i];
			if (!light.enabled) {
				continue;
			}
			
			float3 toLight = light.position - input.worldPosition;
			float dist = length(toLight);
			if (dist > light.radius) continue;

			float3 lightDir = (dist > 1e-5f) ? (toLight / dist) : float3(0.0f, 1.0f, 0.0f);
			float atten = pow(saturate(-dist / light.radius + 1.0f), light.decay);
			if (atten <= 0.0f) continue;

			float lam = HalfLambert(input.normal, lightDir);
			float spec = BlinnPhongReflection(input.normal, lightDir, input.worldPosition, mat.shininess);
			float4 diffuse = light.color * lam * light.intensity * atten;
			float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
			lightingColor += diffuse + speculer;
		}
	}

	// Spot lights
	if (mat.enableLighting) {
		for (uint i = 0; i < gSpotLightCount; ++i) {
			SpotLight light = gSpotLights[i];
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

			float lam = HalfLambert(input.normal, lightDir);
			float spec = BlinnPhongReflection(input.normal, lightDir, input.worldPosition, mat.shininess);
			float4 diffuse = light.color * lam * light.intensity * atten;
			float4 speculer = light.color * light.intensity * spec * mat.specularColor * atten;
			lightingColor += diffuse + speculer;
		}
	}

	output.color = baseColor * lightingColor;

	if (mat.enableShadowMapProjection) {
		float shadow = ComputeShadowFactor(input.worldPosition);
		output.color *= shadow;
	}
	
	output.color.a = mat.color.a * textureColor.a;
	if (output.color.a < 0.01f) {
		discard;
	}
#endif
	return output;
}
