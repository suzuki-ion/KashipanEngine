// 多段影: HalfLambert内のObjectToon量子化ロジックを差し替える（TONE_HOOKから呼び出される）
float ApplyMultiTone(float halfLambert, Material mat) {
    const float kShadowLevel = 0.35f;
    const float kMidLevel = 0.7f;
    const float kLitLevel = 1.0f;
    float shadowThreshold = (mat.shadowThreshold > 0.0001f) ? mat.shadowThreshold : 0.45f;
    float midThreshold = (mat.midThreshold > 0.0001f) ? mat.midThreshold : 0.8f;
    float bandSoftness = (mat.bandSoftness > 0.0001f) ? mat.bandSoftness : 0.05f;
    bool twoTone = (mat.toneCount > 1.5f && mat.toneCount < 2.5f);
    float toon = kShadowLevel;
    if (twoTone) {
        toon = lerp(kShadowLevel, kLitLevel, smoothstep(shadowThreshold - bandSoftness, shadowThreshold + bandSoftness, halfLambert));
    } else {
        toon = lerp(toon, kMidLevel, smoothstep(shadowThreshold - bandSoftness, shadowThreshold + bandSoftness, halfLambert));
        toon = lerp(toon, kLitLevel, smoothstep(midThreshold - bandSoftness, midThreshold + bandSoftness, halfLambert));
    }
    return toon;
}
