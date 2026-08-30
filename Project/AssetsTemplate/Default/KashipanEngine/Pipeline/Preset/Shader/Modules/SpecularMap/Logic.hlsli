// スペキュラー/グロスマップ用テクスチャ。extraParametersで"gSpecularTex"というTextureRef型パラメータを設定する
Texture2D gSpecularTex : register(t18);

// スペキュラーマップ: ライティング計算前にmat.shininessをテクスチャのグロス値で変調する。
// matはローカルコピーのため、この後のライトループ内の参照へそのまま反映される
// （PRELIGHTING_HOOKSの中でNormalMapの後、優先度20で呼ぶこと）
void ApplySpecularMap(inout Material mat, float2 uv) {
    if (mat.specularMapIntensity <= 0.0001f) return;
    float gloss = gSpecularTex.Sample(gSamplers[mat.samplerIndex], uv).r;
    mat.shininess *= gloss;
}
