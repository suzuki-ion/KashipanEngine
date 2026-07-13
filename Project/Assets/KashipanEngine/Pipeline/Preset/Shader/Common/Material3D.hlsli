struct Material {
    float enableLighting;
    float enableEnvironmentMapping;
    float enableShadowMapProjection;
    float useTexture;
    float4 color;
    float4x4 uvTransform;
    float shininess;
    float4 specularColor;
    float environmentCoefficient;
};