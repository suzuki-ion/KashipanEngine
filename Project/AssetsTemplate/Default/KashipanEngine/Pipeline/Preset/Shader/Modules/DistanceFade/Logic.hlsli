// 距離フェード: カメラからの距離でアルファを減衰させる（ALPHA_HOOKSの中で最初、優先度10で呼ぶこと）
void ApplyDistanceFade(inout float4 outputColor, Material mat, float3 worldPosition) {
    if (mat.fadeEndDistance <= mat.fadeStartDistance) return;
    float distanceToCamera = distance(gCamera3D.eyePosition.xyz, worldPosition);
    float fade = smoothstep(mat.fadeStartDistance, mat.fadeEndDistance, distanceToCamera);
    outputColor.a *= (1.0f - fade);
}
