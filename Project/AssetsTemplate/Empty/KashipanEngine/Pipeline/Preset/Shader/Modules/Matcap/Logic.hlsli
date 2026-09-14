float3 BlendMatcapLayer(float3 baseColor, float3 layerColor, float blendMode) {
    if (blendMode < 0.5f) {
        return baseColor + layerColor;
    } else if (blendMode < 1.5f) {
        return baseColor * layerColor;
    } else if (blendMode < 2.5f) {
        return 1.0f - (1.0f - baseColor) * (1.0f - layerColor);
    }
    return layerColor;
}

// マットキャップ: ビュー空間法線からUVを求め、後乗せレイヤーとして合成する（COMPOSITE_HOOKSの中で最初、
// 優先度10で呼ぶこと）。テクスチャ本体はgTextures[]（バインドレステクスチャ配列）から
// mat.matcap(2nd)TextureIndexで参照する
void ApplyMatcap(inout float4 outputColor, Material mat, float3 shadingNormal) {
    float3 viewNormal = mul(shadingNormal, (float3x3)gCamera3D.view);
    float2 matcapUV = viewNormal.xy * 0.5f + 0.5f;
    if (mat.matcapIntensity > 0.0f) {
        float3 matcapColor = gTextures[mat.matcapTextureIndex].Sample(gSamplers[mat.samplerIndex], matcapUV).rgb * mat.matcapIntensity;
        outputColor.rgb = BlendMatcapLayer(outputColor.rgb, matcapColor, mat.matcapBlendMode);
    }
    if (mat.matcap2ndIntensity > 0.0f) {
        float3 matcap2ndColor = gTextures[mat.matcap2ndTextureIndex].Sample(gSamplers[mat.samplerIndex], matcapUV).rgb * mat.matcap2ndIntensity;
        outputColor.rgb = BlendMatcapLayer(outputColor.rgb, matcap2ndColor, mat.matcap2ndBlendMode);
    }
}
