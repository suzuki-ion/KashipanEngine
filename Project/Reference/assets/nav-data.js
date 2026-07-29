// KashipanEngine Reference - navigation data
// href is always relative to the Reference/ root folder.
const KE_PAGES = [
  { id: "00_Index", title: "トップページ", group: "はじめに", href: "00_Index.html" },
  { id: "01_Overview", title: "エンジン概要とアーキテクチャ", group: "はじめに", href: "01_Overview.html" },

  { id: "02_Scenes", title: "シーン (Scene / SceneManager)", group: "コアシステム", href: "02_Scenes.html" },
  { id: "03_GameObjects", title: "ゲームオブジェクト (EmptyObject)", group: "コアシステム", href: "03_GameObjects.html" },
  { id: "04_ObjectComponents", title: "オブジェクトコンポーネント基礎", group: "コアシステム", href: "04_ObjectComponents.html" },

  { id: "05_Rendering", title: "描画コンポーネント", group: "描画・物理", href: "05_Rendering.html" },
  { id: "06_Collision", title: "コライダーと物理演算", group: "描画・物理", href: "06_Collision.html" },

  { id: "07_Scripting", title: "AngelScriptでのゲームロジック", group: "スクリプティング", href: "07_Scripting.html" },

  { id: "08_Input", title: "入力 (Input / InputCommand)", group: "入力・アセット", href: "08_Input.html" },
  { id: "09_Assets", title: "アセット管理", group: "入力・アセット", href: "09_Assets.html" },

  { id: "10_PostEffects", title: "ポストエフェクト", group: "その他システム", href: "10_PostEffects.html" },
  { id: "11_Window", title: "ウィンドウ / オフスクリーン", group: "その他システム", href: "11_Window.html" },
  { id: "12_SceneEditor", title: "シーンエディタ (ImGui)", group: "その他システム", href: "12_SceneEditor.html" },

  { id: "13_Utilities", title: "ユーティリティ", group: "ユーティリティ", href: "13_Utilities.html" },
  { id: "14_BuildMacros", title: "ビルドマクロ", group: "ユーティリティ", href: "14_BuildMacros.html" },

  { id: "T00_Index", title: "チュートリアル目次", group: "チュートリアル", href: "Tutorial/00_Index.html" },
  { id: "T01_CreateScene", title: "1. シーンを作成する", group: "チュートリアル", href: "Tutorial/01_CreateScene.html" },
  { id: "T02_RegisterScene", title: "2. シーンを登録する", group: "チュートリアル", href: "Tutorial/02_RegisterScene.html" },
  { id: "T03_CreateRenderTarget", title: "3. 描画先オブジェクトを作成する", group: "チュートリアル", href: "Tutorial/03_CreateRenderTarget.html" },
  { id: "T04_CreateCamera", title: "4. カメラオブジェクトを作成する", group: "チュートリアル", href: "Tutorial/04_CreateCamera.html" },
  { id: "T05_CreateLight", title: "5. ライトオブジェクトを作成する", group: "チュートリアル", href: "Tutorial/05_CreateLight.html" },
  { id: "T06_CreateObject", title: "6. オブジェクトを作成する", group: "チュートリアル", href: "Tutorial/06_CreateObject.html" },
  { id: "T07_AddComponents", title: "7. コンポーネントを追加する", group: "チュートリアル", href: "Tutorial/07_AddComponents.html" },
  { id: "T08_AddCollider", title: "8. 当たり判定を付ける", group: "チュートリアル", href: "Tutorial/08_AddCollider.html" },
  { id: "T09_WriteScript", title: "9. スクリプトを書く", group: "チュートリアル", href: "Tutorial/09_WriteScript.html" },
  { id: "T10_SetupInput", title: "10. 入力を設定する", group: "チュートリアル", href: "Tutorial/10_SetupInput.html" },
  { id: "T11_LoadAssets", title: "11. アセットを読み込む", group: "チュートリアル", href: "Tutorial/11_LoadAssets.html" },
  { id: "T12_ChangeScene", title: "12. シーンを切り替える", group: "チュートリアル", href: "Tutorial/12_ChangeScene.html" },
];
