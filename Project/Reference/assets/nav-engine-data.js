// KashipanEngine Reference - Engine site navigation data
// href is always relative to the Reference/ root folder.
const KE_SITE = "engine";
const KE_SITE_LABEL = "エンジン本体リファレンス";
const KE_OTHER_SITES = [
  { label: "エディタリファレンス", href: "Editor/00_Index.html" },
  { label: "スクリプトリファレンス", href: "Script/00_Index.html" },
];

const KE_PAGES = [
  { id: "00_Index", title: "トップページ", group: "はじめに", href: "Engine/00_Index.html" },
  { id: "01_Overview", title: "エンジン概要とアーキテクチャ", group: "はじめに", href: "Engine/01_Overview.html" },

  { id: "02_Scenes", title: "シーン (Scene / SceneManager)", group: "コアシステム", href: "Engine/02_Scenes.html" },
  { id: "03_GameObjects", title: "ゲームオブジェクト (EmptyObject)", group: "コアシステム", href: "Engine/03_GameObjects.html" },
  { id: "04_ObjectComponents", title: "オブジェクトコンポーネント基礎", group: "コアシステム", href: "Engine/04_ObjectComponents.html" },

  { id: "05_Rendering", title: "描画コンポーネント", group: "描画・物理", href: "Engine/05_Rendering.html" },
  { id: "06_Collision", title: "コライダーと物理演算", group: "描画・物理", href: "Engine/06_Collision.html" },

  { id: "08_Input", title: "入力 (Input / InputCommand)", group: "入力・アセット", href: "Engine/08_Input.html" },
  { id: "09_Assets", title: "アセット管理", group: "入力・アセット", href: "Engine/09_Assets.html" },

  { id: "10_PostEffects", title: "ポストエフェクト", group: "その他システム", href: "Engine/10_PostEffects.html" },
  { id: "11_Window", title: "ウィンドウ / オフスクリーン", group: "その他システム", href: "Engine/11_Window.html" },

  { id: "13_Utilities", title: "ユーティリティ", group: "ユーティリティ", href: "Engine/13_Utilities.html" },
  { id: "14_BuildMacros", title: "ビルドマクロ", group: "ユーティリティ", href: "Engine/14_BuildMacros.html" },
];
