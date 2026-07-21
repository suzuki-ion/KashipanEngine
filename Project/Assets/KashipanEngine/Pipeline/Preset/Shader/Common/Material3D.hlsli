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
    // リムライト（ライト方向を考慮した逆光縁取り）。rimIntensity=0（既定）の場合は無効
    float4 rimColor;
    float rimPower;
    float rimIntensity;
};