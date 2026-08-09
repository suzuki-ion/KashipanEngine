// 発光（ライティング非依存で常時加算）。emissionIntensity=0（既定）で無効。
// gEmissionTexはextraParametersでTextureRef型として設定する
float4 emissionColor; // @Color
float emissionIntensity; // @Range(0, 5, 0.01)
