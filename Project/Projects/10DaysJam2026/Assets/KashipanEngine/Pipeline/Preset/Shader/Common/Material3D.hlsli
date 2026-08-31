// struct Materialの基本フィールド一覧（structの中身のみ、struct宣言自体はObjectPS.hlsl側で行う）。
// カスタムシェーダー向けの追加パラメータは、末尾のADDITIONAL_MATERIAL_FIELDSマーカー位置に
// シェーダーモジュール合成システム（Graphics/Pipeline/System/ShaderModuleComposer）経由で追加される。
// フィールド名が MaterialManager::Material::extraParameters（.matの"parameters"）のキーと一致していれば、
// C++側の対応コード変更なしに値が自動的にここへ渡る（対応する型は float/float2/float3/float4/float4x4/int/uint/bool）。
float enableLighting;
float enableShadowMapProjection;
float useTexture;
// gTextures[]（バインドレステクスチャ配列）/ gSamplers[]（静的サンプラー配列）内でのインデックス。
// テクスチャ未設定時はフォールバック（白1x1）テクスチャのインデックスを指す
uint textureIndex;
uint samplerIndex;
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
// 半透明ディザの方式をブルーノイズ（既定）からBayer行列（16x16）へ切り替える。ブルーノイズより
// わずかに軽いが、斜めのクロスハッチ状の規則的な模様が視認できる（意図的なレトロ表現向け）。
// enableTemporalDither/ditherDepthBucketSize等、他のディザ関連フィールドとは独立して併用できる
// （Common/BlueNoiseDither.hlsliのComputeDitherThreshold参照）。既定はfalse（ブルーノイズ）
bool useBayerDither;
// useBayerDither有効時、Bayer行列のサイズを16x16（既定）から64x64へ切り替える。64x64は階調が
// 4096（ブルーノイズと同数）まで増え、大きな面での模様の繰り返しが16x16よりは目立ちにくくなる
// （斜めのクロスハッチという規則的な模様の性質自体は消えない）。useBayerDitherがfalseの場合は
// 無視される（ブルーノイズ使用時は常に4096階調のため）。既定はfalse（16x16）
bool useBayer64x64;
// 半透明ディザの位相に、カメラからのワールド距離を量子化した項を追加する（単位: メートル、
// この値がバケットの刻み幅）。同一メッシュ内で奥行き方向に重なる層（フード付き服の
// フードと胴体など）を、同一idSeedのままでも独立してディザできるようにする。
// 既定は0（無効＝従来通りidSeedのみで位相を決める）。1つの面の中でこの刻み幅を大きく
// 超える奥行き変化がある場合、その境目にディザパターンの継ぎ目が見えることがある点に注意
float ditherDepthBucketSize;
// 半透明ディザを複数回（位相違いで）描画して結果をブレンドし、粒状感を1フレーム内で滑らかにする
// （Renderer::RenderMultiPassDither参照）。このフィールド自体はシェーダー内では参照されず、
// C++側がマテリアルエディタでの表示・編集用に読むだけ（対象エントリの振り分けに使う）。
// 既定はfalse。有効にした分だけ該当オブジェクトの描画コストが数倍になる点に注意
bool enableMultiPassDither;
// enableMultiPassDither有効時のパス数N（Renderer::RenderMultiPassDither参照)。C++側で1〜8に
// クランプして使用し、0または未設定の場合はエンジン既定値(4)を使う。値を大きくするほど
// 粒状感は滑らかになるが、その分だけ該当オブジェクトの描画コストが増える
int multiPassDitherCount;
// このマテリアルが落とす影を、本体と同じブルーノイズディザで薄くする(ShadowMapPS.hlsl)処理を
// 無効化する。trueにすると影は従来通りアルファ0.01以上で一律フル濃度になる（テクスチャの
// アルファ抜き形状はそのまま活きるため、フォリッジ等「アルファは切り抜き用」なマテリアルで
// 使う想定）。既定はfalse（影もアルファに応じて薄くなる、現状の挙動のまま）
bool disableShadowDither;
// オブジェクト単位の色（MeshRendererのInstance Color）。マテリアル本体（共有アセット）とは別に、
// インスタンス（描画エントリ）ごとに異なる値を持つ
float4 instanceColor;
// instanceColorの適用方法。0=Override（置き換え）, 1=Multiply（乗算）, 2=Add（加算）, 3=Subtract（減算）
float instanceColorBlendMode;
// 半透明を「ディザ(スクリーンドア)」ではなく、本格的なアルファブレンドで表現する。trueにすると
// ObjectPS.hlslの上のディザ判定(ComputeDitherThreshold呼び出し)は丸ごとスキップされ、アルファ値が
// そのままブレンドステージ(BlendState)へ渡る。ShadowMapPS.hlslでも同様にディザをスキップし、
// disableShadowDither=trueと同じくアルファ0.01以上で一律フル濃度の影になる。
// 有効にする場合、MeshRenderer等のpipelineNameを深度書き込みなしの半透明パイプライン
// （例: Object3D.Solid.BlendTranslucent。PipelineVariantBuilderのBlendから選択可能）に設定すること。
// 深度書き込みが無いため、重なった半透明オブジェクト同士は自動でソートされない。RenderPriority
// （パイプライン単位・レンダラーコンポーネント単位のどちらも）を使い、奥にあるものから先に
// 描画されるよう手動で順序を設定する必要がある。既定はfalse（従来通りディザ方式）
bool useAlphaBlend;
