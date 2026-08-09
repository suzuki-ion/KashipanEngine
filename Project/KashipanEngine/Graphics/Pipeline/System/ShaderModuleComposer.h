#pragma once
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief シェーダーモジュールの合成先スロット
/// @details Tone/RimColorは「単一枠・既定値あり」（Object/ObjectPS.hlsl内の対応するコメント区間を
///          丸ごと置き換える）、Directional/Composite/Alphaは「複数枠・優先順位付き蓄積」
///          （対応するコメントマーカーを、選択されたモジュールの呼び出し文を優先度順に並べたもので置き換える）
enum class ModuleHookSlot {
    Tone,        // HalfLambert内のObjectToon量子化ロジックを差し替える（同時に選べるのは実質1つ）
    RimColor,    // ディレクショナルライトループ内のrimColor算出式を差し替える（同時に選べるのは実質1つ）
    PreLighting, // shadingNormal算出直後、ディレクショナルライトループの直前へ呼び出しを追加する
                 // （shadingNormalやmatをライティング計算前に書き換えるモジュール用。例: NormalMap, SpecularMap）
    Directional, // ディレクショナルライトループの末尾へ呼び出しを追加する
    Environment, // ローカルライトループ直後、output.color合成の直前へ呼び出しを追加する（envColorを計算するモジュール用）
    Composite,   // output.color = baseColor*lightingColor+envColor; の直後へ呼び出しを追加する
    Alpha,       // output.color.a確定直後へ呼び出しを追加する
    Composite2D, // Object2D側: output.color = mat.color*textureColor; の直後へ呼び出しを追加する（Object2D専用）
    Alpha2D,     // Object2D側: Composite2Dフックの直後へ呼び出しを追加する（アルファ処理用。Object2D専用）
};

/// @brief 指定スロットがObject2D専用（Composite2D/Alpha2D）かどうか
/// @details Object3D用のトークン受理（PipelineVariantResolver）・UIでの表示フィルタ
///          （PipelineVariantBuilder）の両方で、選択中のベース（Object3D/Object2D）と
///          モジュールのスロットが一致するかを判定するために使う
bool IsTwoDimensionalSlot(ModuleHookSlot slot);

/// @brief 1つのシェーダーモジュールの定義
struct ShaderModuleDefinition {
    std::string token;          // パイプライン名/UIで使う識別子（例: "Dissolve"）。Modules/<token>/ 配下を参照する
    ModuleHookSlot slot;
    int priority;                // Directional/Composite/Alphaでの並び順（小さいほど先に呼ばれる）
    std::string callExpression;  // 生成する呼び出し文（Tone/RimColorは対象区間全体の置き換えテキストとしても使う）
};

/// @brief 登録済みシェーダーモジュール一覧を取得
/// @details PipelineVariantResolverでのパイプライン名トークン照合、MeshRendererのモジュール選択UIの
///          両方から共通で参照される
const std::vector<ShaderModuleDefinition> &GetShaderModuleRegistry();

/// @brief 選択されたモジュール（トークン集合）から、Object/ObjectPS.hlslをベースにした合成済みHLSLソースを
///        生成し、<shaderBaseDir>/Generated/ 以下へ書き出す
/// @details ObjectPS.hlsl内のコメントマーカー（/{{...}}/形式）を、選択モジュールのModules/<token>/Fields.hlsli・
///          Logic.hlsliの内容や呼び出し文で置換する。マーカーが見つからない場合は何もしない（安全側に倒す）。
///          生成ファイル名は選択トークンをソートして連結したものになるため、選択順によらず同じ組み合わせは
///          常に同じファイル名になる
/// @param selectedTokens 選択されたモジュールのトークン一覧（順不同、登録されていないトークンは無視される）
/// @param shaderBaseDir シェーダー資産のベースディレクトリ（PipelineManagerの pipelineFolderPath_ + "/Preset/Shader"）
/// @return 生成したファイルの絶対パス（selectedTokensが空、または書き込みに失敗した場合は空文字）
std::string ComposeAndWriteShader(const std::vector<std::string> &selectedTokens, const std::string &shaderBaseDir);

/// @brief 指定モジュールのFields.hlsliからフィールド名だけを抽出する（型・注釈は見ない、名前のみ）
/// @param token モジュールのトークン名（Modules/<token>/Fields.hlsli を参照する）
/// @param shaderBaseDir シェーダー資産のベースディレクトリ
/// @return 抽出したフィールド名一覧（ファイルが無い/読めない場合は空）
std::vector<std::string> GetModuleFieldNames(const std::string &token, const std::string &shaderBaseDir);

} // namespace KashipanEngine
