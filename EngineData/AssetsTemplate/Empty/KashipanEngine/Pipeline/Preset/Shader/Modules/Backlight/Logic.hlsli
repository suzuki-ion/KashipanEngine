// バックライト: 光がオブジェクトの背後（法線から見て裏面）にあるとき、視線に対する縁を光らせる
// （DIRECTIONAL_HOOKSから、ディレクショナルライトループ内で毎回呼び出される）
void ApplyBacklight(inout float4 lightingColor, Material mat, float3 shadingNormal, float3 viewDir, float3 toLightDir, DirectionalLight light, float shadow) {
    if (mat.backlightIntensity <= 0.0f) return;
    float ndotl = dot(shadingNormal, normalize(toLightDir));
    float backFacing = saturate(-ndotl);
    float backlightFactor = pow(saturate(1.0f - saturate(dot(shadingNormal, viewDir))), mat.backlightPower);
    float3 backlight = mat.backlightColor.rgb * mat.backlightIntensity * backlightFactor * backFacing * light.color.rgb * light.intensity * shadow;
    lightingColor.rgb += backlight;
}
