#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <Config/GameSceneConfig.h>

namespace KashipanEngine {

	GameScene::GameScene()
		: SceneBase("GameScene") {
	}

	void GameScene::Initialize() {
		sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

		// シーンのコンポーネント追加
		AddSceneComponent(std::make_unique<SceneChangeIn>());
		AddSceneComponent(std::make_unique<SceneChangeOut>());

		if (auto* in = GetSceneComponent<SceneChangeIn>()) {
			in->Play();
		}

		// * ゲームで使うデータの初期化 * //
		blockContainer_.Initialize(
			Application::kBlockContainerRows,
			Application::kBlockContainerCols);

		// * ゲームシステムの初期化 * //
		thermometer_.SetTemperature(Application::kInitialTemperature);
		thermometer_.SetMaxTemperature(100.0f);
		thermometer_.SetTemperatureChangeSpeed(5.0f);
		blockScroller_.Initialize(thermometer_.GetTemperature(), Application::kBlockSize);
		matchResolver_.Initialize();

		// * 描画用のオブジェクトの生成 * //
		// 2D用オフスクリーンバッファを取得。描画先として使用するため、以降のオブジェクト生成時に必要になる。
		auto* screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();
		// ブロック描画用
		blockSpriteContainer_.Initialize(
			Application::kBlockContainerRows,
			Application::kBlockContainerCols);
		blockSpriteBasePos_ = Vector2(1920.0f * 0.5f, 1080.0f * 0.5f);

		for (int row = 0; row < Application::kBlockContainerRows; row++) {
			for (int col = 0; col < Application::kBlockContainerCols; col++) {
				auto obj = std::make_unique<Sprite>();
				blockSpriteContainer_.SetBlockSprite(row, col, obj.get());

				// オブジェクト名は任意だが、デバッグ表示などで使用されるため、わかりやすい名前をつけることが望ましい
				std::string name = "BlockSprite[" + std::to_string(row) + "][" + std::to_string(col) + "]";
				obj->SetName(name);

				if (auto* mat = obj->GetComponent2D<Material2D>()) {
					mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}

				// オブジェクト生成時は以下のように、必ず最後に AttachToRenderer を呼び出してからシーンに追加すること
				// AttachToRenderer の引数は、描画先と使用するパイプライン名を指定する
				obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
				// シーンにオブジェクトを追加。これを行わないと更新処理がされないため注意。
				// オブジェクトの所有権はシーンが持つため、std::move して渡すこと。
				AddObject2D(std::move(obj));
			}
		}

		blockSpriteContainer_.RsetBlockPosition(blockSpriteBasePos_, Vector2(Application::kBlockSize, Application::kBlockSize));
		UpdateBlockColor();
	}

	GameScene::~GameScene() {
	}

	void GameScene::OnUpdate() {
		// ブロックのスクロール
		if (!matchResolver_.HaveMatch()) {
			blockScroller_.Update(KashipanEngine::GetDeltaTime());

			blockSpriteContainer_.RsetBlockPosition(
				blockSpriteBasePos_ + Vector2(0.0f, blockScroller_.GetCurrentScroll()),
				Vector2(Application::kBlockSize, Application::kBlockSize));

			if (blockScroller_.IsScrollComplete()) {
				blockContainer_.ResetOverflowCount();
				// 一番下の行に新しい行を追加
				std::vector<int32_t> newRow(Application::kBlockContainerCols, 0);
				// ランダムなブロックを生成
				int prevBlockType = 0;
				for (int col = 0; col < Application::kBlockContainerCols; col++) {
					int blockType = rand() % 3 + 1; // 1から3のランダムな値
					if(prevBlockType != 0 && blockType == prevBlockType) {
						// 前のブロックと同じタイプが出たら、次のブロックタイプを出すようにする（同じタイプが3つ以上並ぶのを防止）
						blockType = (blockType + 1) % 3 +1;
					}

					newRow[col] = blockType;
					prevBlockType = blockType;
				}
				blockContainer_.PushRow(newRow);

				// ブロックの押し上げで範囲外に出たブロックの数を温度に加算
				thermometer_.AddTemperature(static_cast<float>(blockContainer_.GetOverflowCount()));

				// 温度が最大値を超えた場合は、温度を初期値にリセットし、ブロックも全てリセットする
				if (thermometer_.IsOverheated()) {
					thermometer_.SetTemperature(Application::kInitialTemperature);
					blockContainer_.ResetBlocks();
				}

				// ブロックの色を更新
				UpdateBlockColor();
			}
		}
		blockScroller_.SetScrollSpeed(thermometer_.GetTemperature());

		// ブロックが3つ以上繋がっているかを判定し、繋がっているブロックのスケールを変化させる
		auto matches = matchResolver_.ResolveMatches(blockContainer_.GetBlocks());
		for (auto& match : matches) {
			int row = match.first;
			int col = match.second;
			if (auto* sprite = blockSpriteContainer_.GetBlockSprite(row, col)) {
				if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
					float scale = Application::kBlockSize;
					tr->SetScale(Vector2(scale, scale));
				}
			}
		}
		matchResolver_.Update(KashipanEngine::GetDeltaTime());
		if(matchResolver_.IsCompleteMatchStopFrame()) {
			// マッチが完全に停止したフレームで、マッチしたブロックを消す
			for (auto& match : matches) {
				int row = match.first;
				int col = match.second;
				blockContainer_.SetBlock(row, col, 4);
			}
			UpdateBlockColor();
		}

		// デバッグ用：シーン遷移コマンドの評価
		if (auto* ic = GetInputCommand()) {
			if (ic->Evaluate("DebugSceneChange").Triggered()) {
				if (GetNextSceneName().empty()) {
					SetNextSceneName("MenuScene");
				}
				if (auto* out = GetSceneComponent<SceneChangeOut>()) {
					out->Play();
				}
			}
		}
		// シーン遷移の完了を待ってから次のシーンに切り替える
		if (!GetNextSceneName().empty()) {
			if (auto* out = GetSceneComponent<SceneChangeOut>()) {
				if (out->IsFinished()) {
					ChangeToNextScene();
				}
			}
		}

		//#ifdef _DEBUG
		//	ImGui::Begin("GameScene Debug");
		//	ImGui::Text("BlockContainer:");
		//	ImGui::Text("Rows: %d", blockContainer_.GetRows());
		//	ImGui::Text("Cols: %d", blockContainer_.GetCols());
		//    // ブロックの状態を表示
		//    if (ImGui::CollapsingHeader("BlockContainer State")) {
		//		// テーブル形式でブロックの状態を表示
		//        if (ImGui::BeginTable("BlockTable", blockContainer_.GetCols(),
		//            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
		//            for (int row = 0; row < blockContainer_.GetRows(); ++row) {
		//                ImGui::TableNextRow();
		//                for (int col = 0; col < blockContainer_.GetCols(); ++col) {
		//                    ImGui::TableNextColumn();
		//                    ImGui::Text("%d", blockContainer_.GetBlock(row, col));
		//                }
		//            }
		//            ImGui::EndTable();
		//        }
		//    }
		//#endif // _DEBUG
	}

	void GameScene::UpdateBlockColor()
	{
		for (int row = 0; row < Application::kBlockContainerRows; row++) {
			for (int col = 0; col < Application::kBlockContainerCols; col++) {
				int value = blockContainer_.GetBlock(row, col);
				if (auto* sprite = blockSpriteContainer_.GetBlockSprite(row, col)) {
					if (auto* mat = sprite->GetComponent2D<Material2D>()) {
						switch (value) {
						case 0:
							mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.5f));
							break;
						case 1:
							mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
							break;
						case 2:
							mat->SetColor(Vector4(0.0f, 1.0f, 0.0f, 1.0f));
							break;
						case 3:
							mat->SetColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f));
							break;
						default:
							mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
							break;
						}
					}
				}
			}
		}
	}

} // namespace KashipanEngine