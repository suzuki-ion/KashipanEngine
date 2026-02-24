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
		blockFaller_.Initialize(Application::kBlockSize * 10.0f, Application::kBlockSize);
		matchResolver_.Initialize();
		cursor_.Initialize(
			Application::kBlockContainerRows / 2, Application::kBlockContainerCols / 2,
			Application::kBlockContainerRows, Application::kBlockContainerCols);

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

		// カーソル描画用
		{
			std::unique_ptr<Sprite> obj = std::make_unique<Sprite>();
			cursorSprite_ = obj.get();
			cursorSprite_->SetName("CursorSprite");
			if (auto* tr = obj->GetComponent2D<Transform2D>()) {
				float size = Application::kBlockSize * 1.2f; // カーソルはブロックより少し大きくする
				tr->SetScale(Vector2(size, size));
			}

			if (auto* mat = cursorSprite_->GetComponent2D<Material2D>()) {
				mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.5f));
			}
			obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
			AddObject2D(std::move(obj));
		}
	}

	GameScene::~GameScene() {
	}

	void GameScene::OnUpdate() {
		// カーソルの更新
		cursor_.UpdatePosition(KashipanEngine::SceneBase::GetInputCommand());
		if (KashipanEngine::SceneBase::GetInputCommand()->Evaluate("Submit").Triggered()) {
			// 決定ボタンが押されたとき、カーソルの位置にブロックを差し込む
			auto [row, col] = cursor_.GetPosition();
			blockContainer_.PushBlock(row, col, rand() % 3 + 1);
			UpdateBlockColor();
		}

		// ブロックのスクロール
		if (!matchResolver_.HaveMatch() && !blockFaller_.IsFalling()) {
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
					if (prevBlockType != 0 && blockType == prevBlockType) {
						// 前のブロックと同じタイプが出たら、次のブロックタイプを出すようにする（同じタイプが3つ以上並ぶのを防止）
						blockType = (blockType + 1) % 3 + 1;
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

		// ブロックの落下
		if (!matchResolver_.HaveMatch()) {
			blockFaller_.Update(KashipanEngine::GetDeltaTime(), blockContainer_);

			if (blockFaller_.IsFalling()) {
				// 落下中のブロックの描画位置を更新
				// すべてのブロックの位置をリセットしてから、落下中のブロックだけオフセットを加える
				blockSpriteContainer_.RsetBlockPosition(
					blockSpriteBasePos_ + Vector2(0.0f, blockScroller_.GetCurrentScroll()),
					Vector2(Application::kBlockSize, Application::kBlockSize));

				float fallDelta = blockFaller_.GetCurrentFallOffset();
				for (auto& pos : blockFaller_.GetFallingBlocks()) {
					if (auto* sprite = blockSpriteContainer_.GetBlockSprite(pos.first, pos.second)) {
						if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
							Vector2 currentPos = tr->GetTranslate();
							tr->SetTranslate(currentPos - Vector2(0.0f, fallDelta));
						}
					}
				}
			}

			if (blockFaller_.IsFallComplete()) {
				// 落下完了時に色を更新
				UpdateBlockColor();
				// 位置もリセット
				blockSpriteContainer_.RsetBlockPosition(
					blockSpriteBasePos_ + Vector2(0.0f, blockScroller_.GetCurrentScroll()),
					Vector2(Application::kBlockSize, Application::kBlockSize));
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
		if (matchResolver_.IsCompleteMatchStopFrame()) {
			// マッチが完全に停止したフレームで、マッチしたブロックを消す
			for (auto& match : matches) {
				int row = match.first;
				int col = match.second;
				blockContainer_.SetBlock(row, col, 4);
			}
			UpdateBlockColor();
		}

		// ブロックが4のブロックに隣接しているかを判定し、空きがあれば周囲八マスを爆破する
		auto isolatedBlocks = matchResolver_.ResolveIsolatedBlocks(blockContainer_.GetBlocks());
		if (isolatedBlocks.size() > 0) {
			for (auto& isolated : isolatedBlocks) {
				int row = isolated.first;
				int col = isolated.second;
				for (int dr = -1; dr <= 1; dr++) {
					for (int dc = -1; dc <= 1; dc++) {
						int r = row + dr;
						int c = col + dc;
						if (r >= 0 && r < Application::kBlockContainerRows && c >= 0 && c < Application::kBlockContainerCols) {
							blockContainer_.SetBlock(r, c, 0);
						}
					}
				}
			}
			UpdateBlockColor();
		}

		// カーソルの描画位置を更新
		if (cursorSprite_) {
			if (auto* tr = cursorSprite_->GetComponent2D<Transform2D>()) {
				int32_t row, col;
				std::tie(row, col) = cursor_.GetPosition();

				Vector2 cursorPos =
					blockSpriteContainer_.GetBlockPosition(row, col);
				tr->SetTranslate(cursorPos);
			}
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