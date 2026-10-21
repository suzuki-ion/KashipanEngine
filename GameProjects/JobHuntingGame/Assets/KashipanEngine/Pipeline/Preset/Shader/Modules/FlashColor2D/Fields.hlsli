// ヒットフラッシュ（被弾時の白点滅等）。flashIntensity=0（既定）で無効。
// C++側からflashIntensityを毎フレーム書き換えて使う想定
float flashIntensity; // @Range(0, 1, 0.01)
float4 flashColor; // @Color
