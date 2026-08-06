// 環境マップ用キューブマップテクスチャ。extraParametersで"gEnvironmentMap"というTextureCubeRef型
// パラメータを設定すると、描画時に自動でここへバインドされる（RendererInternal::BindExtraTextureParameters参照）
TextureCube gEnvironmentMap : register(t1);

// 環境マップ: 反射方向をサンプルしたenvColorを返す。mat.enableEnvironmentMapping<=0.5なら無効
// （ENVIRONMENT_HOOKSの中で優先度10、output.color合成の直前に呼ぶこと）
float4 ApplyEnvironmentMap(Material mat, float3 shadingNormal, float3 worldPosition) {
    if (mat.enableEnvironmentMapping <= 0.5f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 cameraToPosition = worldPosition - gCamera3D.eyePosition.xyz;
    float3 reflectDir = reflect(cameraToPosition, shadingNormal);
    float4 envColor = gEnvironmentMap.Sample(gSampler, reflectDir);
    envColor *= mat.environmentCoefficient;
    return envColor;
}
