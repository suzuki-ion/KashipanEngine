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
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
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

	// Directional
	if (gDirectionalLight.enabled && mat.enableLighting) {
		float lam = HalfLambert(input.normal, -gDirectionalLight.direction);
		float spec = BlinnPhongReflection(input.normal, gDirectionalLight.direction, input.worldPosition, mat.shininess);
		float4 diffuse = gDirectionalLight.color * lam * gDirectionalLight.intensity;
		float4 speculer = gDirectionalLight.color * gDirectionalLight.intensity * spec * mat.specularColor;
		lightingColor += diffuse + speculer;
	}

	// Point light
	if (gPointLight.enabled && mat.enableLighting) {
		float3 toLight = gPointLight.position - input.worldPosition;
		float dist = length(toLight);
		float3 lightDir = normalize(toLight);
		float atten = pow(saturate(-dist / gPointLight.radius + 1.0f), gPointLight.decay);
		
		float lam = HalfLambert(input.normal, lightDir);
		float spec = BlinnPhongReflection(input.normal, lightDir, input.worldPosition, mat.shininess);
		float4 diffuse = gPointLight.color * lam * gPointLight.intensity * atten;
		float4 speculer = gPointLight.color * gPointLight.intensity * spec * mat.specularColor * atten;
		lightingColor += diffuse + speculer;
	}

	// Spot light
	if (gSpotLight.enabled && mat.enableLighting) {
		float3 toLight = gSpotLight.position - input.worldPosition;
		float dist = length(toLight);
		float3 lightDir = normalize(toLight);
		float theta = dot(-lightDir, normalize(gSpotLight.direction));
		float inner = cos(gSpotLight.innerAngle);
		float outer = cos(gSpotLight.outerAngle);
		float spot = saturate((theta - outer) / (inner - outer));
		float atten = pow(saturate(-dist / gSpotLight.distance + 1.0f), gSpotLight.decay) * spot;
		
		float lam = HalfLambert(input.normal, lightDir);
		float spec = BlinnPhongReflection(input.normal, lightDir, input.worldPosition, mat.shininess);
		float4 diffuse = gSpotLight.color * lam * gSpotLight.intensity * atten;
		float4 speculer = gSpotLight.color * gSpotLight.intensity * spec * mat.specularColor * atten;
		lightingColor += diffuse + speculer;
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
