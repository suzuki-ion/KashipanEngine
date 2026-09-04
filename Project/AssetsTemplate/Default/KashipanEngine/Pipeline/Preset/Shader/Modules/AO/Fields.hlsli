// アンビエントオクルージョン（AO）。aoIntensity=0（既定）で無効
float aoIntensity; // @Range(0, 1, 0.01)
// バインドレステクスチャ配列（gTextures[]）内でのAOテクスチャのインデックス。
// extraParametersで"aoTextureIndex"というTextureRef型パラメータとして設定する
uint aoTextureIndex;
