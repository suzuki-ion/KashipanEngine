// struct Materialの基本フィールド一覧（structの中身のみ、struct宣言自体はObjectPS.hlsl側で行う）。
// カスタムシェーダー向けの追加パラメータは、末尾のADDITIONAL_MATERIAL_FIELDSマーカー位置に
// シェーダーモジュール合成システム（Graphics/Pipeline/System/ShaderModuleComposer）経由で追加される。
// フィールド名が MaterialManager::Material::extraParameters（.matの"parameters"）のキーと一致していれば、
// C++側の対応コード変更なしに値が自動的にここへ渡る（対応する型は float/float2/float3/float4/float4x4/int/uint/bool）。
float enableLighting;
float enableShadowMapProjection;
float useTexture;
float4 color;
float4x4 uvTransform;
float shininess;
float4 specularColor;
// リムライト（ライト方向を考慮した逆光縁取り）。rimIntensity=0（既定）の場合は無効
float4 rimColor;
float rimPower;
float rimIntensity;
// 半透明ディザ(ブルーノイズ)の位相をフレームごとに無相関にシフトし、粒状感を時間的に均す。
// 既定はfalse（従来通り、位相はオブジェクト単位で固定）。trueにする場合は、対象の描画先に
// TemporalBlendEffectを併用すること（単体では無相関ノイズが常時ちらつくだけになる）
bool enableTemporalDither;
// 半透明ディザの位相に、カメラからのワールド距離を量子化した項を追加する（単位: メートル、
// この値がバケットの刻み幅）。同一メッシュ内で奥行き方向に重なる層（フード付き服の
// フードと胴体など）を、同一idSeedのままでも独立してディザできるようにする。
// 既定は0（無効＝従来通りidSeedのみで位相を決める）。1つの面の中でこの刻み幅を大きく
// 超える奥行き変化がある場合、その境目にディザパターンの継ぎ目が見えることがある点に注意
float ditherDepthBucketSize;
// オブジェクト単位の色（MeshRendererのInstance Color）。マテリアル本体（共有アセット）とは別に、
// インスタンス（描画エントリ）ごとに異なる値を持つ
float4 instanceColor;
// instanceColorの適用方法。0=Override（置き換え）, 1=Multiply（乗算）, 2=Add（加算）, 3=Subtract（減算）
float instanceColorBlendMode;
