[![DebugBuild](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/DebugBuild.yml)
 [![DevelopmentBuild](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/DevelopmentBuild.yml)
 [![ReleaseBuild](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/suzuki-ion/KashipanEngine/actions/workflows/ReleaseBuild.yml)
 
# KashipanEngine

## KashipanEngineとは
KashipanEngineは、『菓子パンのように安っちいが手に取りやすく、ちょっとしたことであれば簡単に実装できる』をコンセプトにしたゲームエンジンです。  
ゲーム画面自体の映えや作れるゲームのジャンルの範囲はそこそこですが、手軽にゲームを作成できることを目指しています。 

## 出来ること・出来ないこと
KashipanEngineはゲームエンジンではあるものの、個人で制作しているゲームエンジンであるためUnityやUnreal、Godotといった有名なゲームエンジンには到底及びません。
あくまで小規模～中規模のゲームを作ったりちょっとしたプロトを作ったりするためのゲームエンジンであることをご了承ください。
### KashipanEngineで出来ること
- Windows環境を想定したゲーム制作
- ミニゲーム程度の規模のゲームや、複数ステージに分割されているパズルゲーム、アクションゲームの制作
- そこそこ良い感じのライティングされた画面の制作
- ちょっとしたオブジェクトの挙動の作成
- ウィンドウを用いたゲームの制作（KashipanEngineの特色として、ウィンドウが複数作成できたり透明なウィンドウを作成できたりするといったことが可能なため。）
### KashipanEngineで出来ないこと
- MacやAndroid、WebなどといったWindows環境以外の環境を想定したゲーム制作
- 広大なステージを歩き回ったり、ハイポリなモデルが複数置かれたりゲーム内のオブジェクト数が数万個あるといった大規模なゲームの制作
- UnityやUnreal Engineのように本格的なライティング
- 本格的な物理挙動や流体シミュレーション

## ゲームエンジンリファレンス
KashipanEngineの内部コード、及びエディター上の操作やオブジェクトのコンポーネントなどといったリファレンスは[こちらのページ](https://suzuki-ion.github.io/KashipanEngine/)にすべてまとまっています。

## 使用ライブラリ
- DirectX 12
- [DirectXTex](https://github.com/microsoft/directxtex)
- [ImGui](https://github.com/ocornut/imgui)
- [Assimp 5.3.0](https://github.com/assimp)
- [React Physics 3D](https://github.com/DanielChappuis/reactphysics3d?)
- [nlohmann/json](https://github.com/DanielChappuis/reactphysics3d?)
- [UTF8-CPP](https://github.com/nemtrif/utfcpp)
- [Angel Script](https://github.com/anjo76/angelscript)
