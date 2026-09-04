// スペキュラーマップ: ライティング計算前にmat.shininessをテクスチャのグロス値で変調する。
// matはローカルコピーのため、この後のライトループ内の参照へそのまま反映される
// （PRELIGHTING_HOOKSの中でNormalMapの後、優先度20で呼ぶこと）。テクスチャ本体はgTextures[]
// （バインドレステクスチャ配列）からmat.specularTextureIndexで参照する
// （extraParametersで"specularTextureIndex"というTextureRef型パラメータを設定する）
void ApplySpecularMap(inout Material mat, float2 uv) {
    if (mat.specularMapIntensity <= 0.0001f) return;
    float gloss = gTextures[mat.specularTextureIndex].Sample(gSamplers[mat.samplerIndex], uv).r;
    mat.shininess *= gloss;
}
