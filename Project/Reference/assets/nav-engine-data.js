// KashipanEngine Reference - Engine site navigation data
// href is always relative to the Reference/ root folder.
const KE_SITE = "engine";
const KE_SITE_LABEL = "エンジン本体リファレンス";
const KE_OTHER_SITE_LABEL = "エディタリファレンス";
const KE_OTHER_SITE_HREF = "Editor/00_Index.html";

const KE_PAGES = [
  { id: "00_Index", title: "トップページ", group: "はじめに", href: "Engine/00_Index.html" },
  { id: "01_Overview", title: "エンジン概要とアーキテクチャ", group: "はじめに", href: "Engine/01_Overview.html" },

  { id: "02_Scenes", title: "シーン (Scene / SceneManager)", group: "コアシステム", href: "Engine/02_Scenes.html" },
  { id: "03_GameObjects", title: "ゲームオブジェクト (EmptyObject)", group: "コアシステム", href: "Engine/03_GameObjects.html" },
  { id: "04_ObjectComponents", title: "オブジェクトコンポーネント基礎", group: "コアシステム", href: "Engine/04_ObjectComponents.html" },

  { id: "05_Rendering", title: "描画コンポーネント", group: "描画・物理", href: "Engine/05_Rendering.html" },
  { id: "06_Collision", title: "コライダーと物理演算", group: "描画・物理", href: "Engine/06_Collision.html" },

  { id: "Script_00_Index", title: "スクリプティング概要・基本的な使い方", group: "スクリプティング: 基礎", href: "Engine/Script/00_Index.html" },
  { id: "Script_01_Lifecycle", title: "ライフサイクルと衝突・ウィンドウイベント", group: "スクリプティング: 基礎", href: "Engine/Script/01_Lifecycle.html" },
  { id: "Script_02_SerializeField", title: "SerializeField属性", group: "スクリプティング: 基礎", href: "Engine/Script/02_SerializeField.html" },
  { id: "Script_03_ComponentAccess", title: "コンポーネント操作とグローバル関数", group: "スクリプティング: 基礎", href: "Engine/Script/03_ComponentAccess.html" },
  { id: "Script_04_ObjectSceneVariables", title: "Object・Scene・Tag・シーン変数", group: "スクリプティング: 基礎", href: "Engine/Script/04_ObjectSceneVariables.html" },

  { id: "Script_05_ComponentTypes", title: "コンポーネント型リファレンス（基本）", group: "スクリプティング: APIリファレンス", href: "Engine/Script/05_ComponentTypes.html" },
  { id: "Script_06_RenderAndWindow", title: "描画・ライト・ウィンドウ系コンポーネント", group: "スクリプティング: APIリファレンス", href: "Engine/Script/06_RenderAndWindow.html" },
  { id: "Script_07_ColliderAndPostEffect", title: "コライダーとポストエフェクト", group: "スクリプティング: APIリファレンス", href: "Engine/Script/07_ColliderAndPostEffect.html" },
  { id: "Script_08_MathAndUtility", title: "数学型・Math・Easing・Random", group: "スクリプティング: APIリファレンス", href: "Engine/Script/08_MathAndUtility.html" },
  { id: "Script_09_JsonAndDictionary", title: "dictionaryとJson", group: "スクリプティング: APIリファレンス", href: "Engine/Script/09_JsonAndDictionary.html" },
  { id: "Script_10_ProceduralGeneration", title: "手続き生成（WFC・ステージグラフ）", group: "スクリプティング: APIリファレンス", href: "Engine/Script/10_ProceduralGeneration.html" },

  { id: "Script_11_PlayerExample", title: "実例: Player.asを読み解く", group: "スクリプティング: 実践・ツール", href: "Engine/Script/11_PlayerExample.html" },
  { id: "Script_12_EditorToolScripting", title: "EditorTool（エディタ拡張スクリプト）", group: "スクリプティング: 実践・ツール", href: "Engine/Script/12_EditorToolScripting.html" },
  { id: "Script_13_Debugging", title: "VS CodeによるAngelScriptデバッグ", group: "スクリプティング: 実践・ツール", href: "Engine/Script/13_Debugging.html" },
  { id: "Script_14_Notes", title: "リロード・エラー確認・注意事項", group: "スクリプティング: 実践・ツール", href: "Engine/Script/14_Notes.html" },

  { id: "08_Input", title: "入力 (Input / InputCommand)", group: "入力・アセット", href: "Engine/08_Input.html" },
  { id: "09_Assets", title: "アセット管理", group: "入力・アセット", href: "Engine/09_Assets.html" },

  { id: "10_PostEffects", title: "ポストエフェクト", group: "その他システム", href: "Engine/10_PostEffects.html" },
  { id: "11_Window", title: "ウィンドウ / オフスクリーン", group: "その他システム", href: "Engine/11_Window.html" },

  { id: "13_Utilities", title: "ユーティリティ", group: "ユーティリティ", href: "Engine/13_Utilities.html" },
  { id: "14_BuildMacros", title: "ビルドマクロ", group: "ユーティリティ", href: "Engine/14_BuildMacros.html" },
];
