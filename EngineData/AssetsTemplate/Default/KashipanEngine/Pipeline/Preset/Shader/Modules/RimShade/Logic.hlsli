// リムシェード: 影側（lamが小さいほど）はrimShadeColorへ寄せる（RIM_COLOR_HOOKから呼び出される）
float3 ApplyRimShade(Material mat, float lam) {
    if (mat.rimShadeBlend <= 0.0001f) return mat.rimColor.rgb;
    return lerp(mat.rimColor.rgb, mat.rimShadeColor.rgb, saturate(mat.rimShadeBlend * (1.0f - lam)));
}
