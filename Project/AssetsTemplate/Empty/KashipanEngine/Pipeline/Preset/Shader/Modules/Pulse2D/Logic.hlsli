// 注意: このファイルは単体コンパイルされず、常にShaderModuleComposerによって
// Object/ObjectPS.hlslと同じディレクトリ（Object/）に生成される合成ファイルへ貼り付けられる。
// gTime/gDeltaTimeはObject2D.hlsli（Common/Time.hlsliをinclude済み）により既に宣言されているため、
// このファイル側で改めてincludeする必要はない

// パルス: 1.0を中心にsin(gTime*pulseSpeed)*pulseIntensity分だけ明度を上下させる
// （COMPOSITE_HOOKS_2Dの中でGradientOverlay2Dより後、優先度50で呼ぶこと）
void ApplyPulse2D(inout float4 outputColor, Material mat) {
    if (mat.pulseIntensity <= 0.0001f) return;
    float wave = 1.0f + sin(gTime * mat.pulseSpeed) * mat.pulseIntensity;
    outputColor.rgb *= max(wave, 0.0f);
}
